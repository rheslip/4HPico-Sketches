
// Copyright 2026 Rich Heslip
//
// Author: Rich Heslip 
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
/*
 * April 2026 - sketch to calibrate the DACs on the 4HPico
 */
// VOut1 = DAC outputs on lower jacks CV1 and CV2
// VOut2 = DAC outputs on middle jacks CV3 and CV4
//
// how to calibrate:
// load this sketch and set VOut1 to 0V in the menus
// trim RV1 to get CV1 and CV2 as close to 0V as possible. both DACs share the same trimmer so they probably won't be exactly the same. Adjust so the average of both CVouts is 0V
// set VOut1 to 5V
// adjust CV/Volt1  to get CV1 and CV2 as close to 5.0V out as possible. If there was an offset from 0V it should be the same offset here. ie if CV1 was 0.05V, adjust for 5.05V
// plug this CV/Volt1 value back into the sketch in the CVOUT_VOLT1 #define
// adjust VOut1 thru -5V to 5V - the voltage on CV1 and CV2 should track within +-5mv or so. if not, you probably have a bad DAC or opamp
// repeat the above for the second DAC ie CV3,CV4, RV2. make sure you have the board jumpered for CVout on the middle jacks

#include <Arduino.h>
#include "4HPico.h"
#include <I2S.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Monospaced_12.h"
#include "Clickencoder.h"
#include "lut.h"


#define SAMPLERATE 22050  // saves CPU cycles

I2S DAC1(OUTPUT);  // 
I2S DAC2(OUTPUT);  // 

#define CVIN_VOLT 580.6  // a/d count per volt - **** adjust this value to calibrate V/octave input
#define CVOUT_VOLT1 5524// D/A count per volt - nominally +-5v range for -+32767 DAC values- ***** adjust this value to calibrate V/octave out
#define CVOUT_VOLT2 5517 // D/A count per volt - nominally +-5v range for -+32767 DAC values- ***** adjust this value to calibrate V/octave out

int16_t cvout1,cvout2;
int16_t voltage1=0;
int16_t voltage2=0;
int16_t cv_volt1=CVOUT_VOLT1;
int16_t cv_volt2=CVOUT_VOLT2;

enum UISTATES {RUN,DORMANT,WAIT_BUTTON_RELEASE};
int16_t UI_state=RUN; // initial UI state

#define DISPLAY_BLANK_MS 60000

#define DEBOUNCE_CYCLES 100 // counter to debounce button release

int32_t displaytimer ; // display blanking timer

ClickEncoder menuenc(ENCA_IN,ENCB_IN,ENCSW_IN,ENCDIVIDE); // menu encoder object

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// timer routines
static void alarm_in_us(uint32_t delay_us) {
  hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM);
  irq_set_exclusive_handler(ALARM_IRQ, alarm_irq);
  irq_set_enabled(ALARM_IRQ, true);
  alarm_in_us_arm(delay_us);
}

static void alarm_in_us_arm(uint32_t delay_us) {
  uint64_t target = timer_hw->timerawl + delay_us;
  timer_hw->alarm[ALARM_NUM] = (uint32_t) target;
}

// timer interrupt handler
//handles the menu encoder

static void alarm_irq(void) {
  menuenc.service(); // handle the menu encoder
  hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM); // clear IRQ flag
  alarm_in_us_arm(TIMER_MICROS);  // reschedule interrupt
}


#include "menusystem.h"  // has to come after display and encoder objects creation

void setup() {
  Serial.begin(115200);
 // while (!Serial);

// init IO ports
  pinMode(ENCA_IN, INPUT_PULLUP);  // menu encoder and switch
  pinMode(ENCB_IN, INPUT_PULLUP);    
  pinMode(ENCSW_IN, INPUT_PULLUP); 

// set up I2C pins 
  Wire.setSDA(PIN_WIRE_SDA);
  Wire.setSCL(PIN_WIRE_SCL);
  Wire.begin();

// set up timer interrupt 
  alarm_in_us(TIMER_MICROS);

  // set up OLED display
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    while(1); // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.setFont(&Monospaced_bold_12); // 8x10 pixel font is more readable than the stock 5x7
  display.setTextSize(1);

	display.setTextColor(WHITE,BLACK); // foreground, background  
  display.setCursor(0,10);
  display.printf(" 4HPico\n Calibr8r"); // power on message
  display.display();
  delay(3000);
  displaytimer=millis(); // reset display blanking timer
  drawmenu(0); // show first menu item

// set up Pico I2S for PT8211 stereo DAC
	DAC1.setBCLK(BCLK);
	DAC1.setDATA(I2S_DATA);
	DAC1.setBitsPerSample(16);
	DAC1.setBuffers(1, 128, 0); // DMA buffer - 32 bit L/R words
	DAC1.setLSBJFormat();  // needed for PT8211 which has funny timing
	DAC1.begin(SAMPLERATE);

  // set up Pico I2S for PT8211 stereo DAC
	DAC2.setBCLK(BCLK1);
	DAC2.setDATA(I2S_DATA1);
	DAC2.setBitsPerSample(16);
	DAC2.setBuffers(1, 128, 0); // DMA buffer - 32 bit L/R words
	DAC2.setLSBJFormat();  // needed for PT8211 which has funny timing
	DAC2.begin(SAMPLERATE);  
}

// first Pico core does UI etc - not super time critical
void loop() {
  static int16_t debouncecounter;
  int16_t encvalue;

  if ((millis()-displaytimer) > DISPLAY_BLANK_MS) {
    UI_state=DORMANT;  // 
    blankdisplay(); // protect the OLED from burnin
  } 

  switch (UI_state) {
    case RUN:
      domenus();  // call the menu state machine
      break;    
    case DORMANT:  // using menu encoder will start screen up again
      encvalue=menuenc.getValue();
      if (encvalue || !digitalRead(ENCSW_IN)) {
        updatedisplay(); // restore the display
        debouncecounter=DEBOUNCE_CYCLES;
        UI_state=WAIT_BUTTON_RELEASE;
      }
      break; 
    case WAIT_BUTTON_RELEASE:  // intermediate state - wait for button release so it doesn't mess up menus
      if (digitalRead(ENCSW_IN)) {
        -- debouncecounter;
        if (debouncecounter==0) UI_state=RUN;  // if button released move to run state
      }
      break;
    default:
      UI_state=RUN;
      break;

  }
}


// second core setup
// second core dedicated to sample processing
void setup1() {
delay (1000); // wait for main core to start up perhipherals
}

// second core sends to DACs

void loop1() {

  cvout1=-voltage1*cv_volt1; // CVouts are inverted
  cvout2=-voltage2*cv_volt2;
  DAC1.write(cvout1); // 
  DAC1.write(cvout1); // 
  DAC2.write(cvout2); // 
  DAC2.write(cvout2); // 

}


