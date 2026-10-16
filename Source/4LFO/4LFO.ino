
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
 * April 2026 - test of the 4HPico menu system
 */

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

enum UISTATES {RUN,DORMANT,WAIT_BUTTON_RELEASE};
int16_t UI_state=RUN; // initial UI state

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

// maximum/minimum increments for the DDS which set the fastest/slowest LFO rates
#define RANGE 6  // exponential map 10***0 to 10**5. max ~50hz at 44khz sampling
#define MIN_DELTA 300 // minimum value for DDS adder - avoids rediculously slow ramps - this gives a few minutes at 44.1khz
#define MAX_AMPLITUDE AD_RANGE  // for scaling the output
#define NUM_LFOS 4  // 
#define NWAVES 6  // number of different waveforms

enum {RAMP, SINE, EXPO, QUARTIC, RANDOM1, PULSE}; // wave shape indexes
struct lfodata {
  int16_t wave; // waveform
  int32_t rate1; // rate for first section of waveform
  int32_t rate2; // rate for second section of waveform
  int16_t amplitude; // output level 0-32k
  int32_t acc; // bottom 16 are accumulator for DDS algorithm, top 16 used for waveform generation
  bool phase; // flags first or second section of waveform
  int16_t dacout; // current DAC value
  int16_t scaledout; // scaled DAC output
  }lfo[NUM_LFOS];

void init_lfos(void) {
  for (int16_t i=0; i< NUM_LFOS;++i) {
    lfo[i].wave=SINE;
    lfo[i].rate1=0x0fffff;
    lfo[i].rate2=0x0fffff;
    lfo[i].amplitude=32767;
  }
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
  display.printf(" 4HPico\n  4LFO"); // power on message
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

  init_lfos();
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

// second core calculates samples and sends to DAC
// I suspect the interpolation code is not quite right but it works

void loop1() {
  bool phasechange; // indicates ramp has changed direction
  unsigned a,x,y,delta,out; 
  unsigned char i;

  while (rp2040.fifo.available()) { // only command in this sketch is sync
    rp2040.fifo.pop(); //
    if ((lfo[i].wave == SINE) || (lfo[i].wave == RAMP)) { // these two need a phase offset so they reset to 0v
      lfo[0].acc=0x1fffffff; 
      lfo[0].phase=1;
    }
    else {   // when reset via sync the expo and quartic waveforms go to +5 and the pulse goes to -5V. not sure what is best
      lfo[0].acc=0;
      lfo[0].phase=0;
    }
  }

  for (i=0; i<NUM_LFOS; ++i) {
    phasechange=0;  
    
    if (lfo[i].phase == 0) lfo[i].acc+=lfo[i].rate1; // ramp up for first part
        else lfo[i].acc-=lfo[i].rate2; // ramp down for second part

    if (lfo[i].acc >= 0x3fffffff) {  // test for accumulator overflow
      lfo[i].phase = 1 ;// ramp down once we hit the top
      lfo[i].acc=0x3fffffff; // set to max for next DAC output
      phasechange=1;
    }
    if (lfo[i].acc <=0) { // test for accumulator underflow
      lfo[i].phase = 0; //ramp up when we hit bottom
      lfo[i].acc=0; // set to 0 min for start of up ramp
      phasechange=1;
    }

    switch (lfo[i].wave) {
      case RAMP:
        lfo[i].dacout=(lfo[i].acc >> 14)-32768; // get top 16 bits of accumulator. ramp is 0-65535 but DAC needs signed
        break;    
      case PULSE: // variable duty cycle pulse
        if (lfo[i].phase) lfo[i].dacout=32767;
        else lfo[i].dacout=-32767;
        break;
      case RANDOM1:  // random value every time we hit max or min ramp
        if (phasechange) lfo[i].dacout=random(65535);
        break;
      // table lookup for exponential and quartic
      case EXPO:
        a=lfo[i].acc >> 14; // our 16 bit ramp value 
        if (lfo[i].phase) a=65535-a; // reverse the lookup for 2nd half
        x=(a>>8)&0xff; // 256 word lookup table - it has 16 bit values
          // note that x+1 will index off the end of the table so I made the table 257 long
        delta=(lut_env_expo[x+1]-lut_env_expo[x])>>4; // interpolate between the table values - 16 steps
        y=lut_env_expo[x]; // lookup base value from table
        out=(y+(a&0xf)*delta);
        if (lfo[i].phase) out=65535-out; // invert the value for 2nd half
        lfo[i].dacout=out -32768; // DAC uses signed values
        break; 
      case QUARTIC:
        a=lfo[i].acc >> 14; // our 16 bit ramp value 
        if (lfo[i].phase) a=65535-a; // reverse the lookup for 2nd half
        x=(a>>8)&0xff; // 256 word lookup table - it has 16 bit values
          // note that x+1 will index off the end of the table so I made the table 257 long
        delta=(lut_env_quartic[x+1]-lut_env_quartic[x])>>4; // interpolate between the table values - 16 steps
        y=lut_env_quartic[x]; // lookup base value from table
        out=(y+(a&0xf)*delta); // interpolated output = base value + steps*delta. scale the 16 bit table value to 12 bits
        if (lfo[i].phase) out=65535-out; // invert the value for 2nd half
        lfo[i].dacout=out-32768; 
        break;         
      default:  // in case there are bogus values in EEPROM

      case SINE:
        a=lfo[i].acc >> 14; // our 16 bit ramp value range 0-65535
        if (lfo[i].phase) a=65535-a; // reverse the lookup for 2nd half
        x=(a>>8)&0xff; // 256 word lookup table - it has 16 bit values
          // note that x+1 will index off the end of the table so I made the table 257 long
        delta=(lut_raised_cosine[x+1]-lut_raised_cosine[x])>>4; // interpolate between the table values - 16 steps
        y=lut_raised_cosine[x]; // lookup base value from table
        out=(y+(a&0xf)*delta); // interpolated output = base value + steps*delta. 
        if (lfo[i].phase) out=65535-out; // invert the value for 2nd half
        lfo[i].dacout=out-32768;      // DAC uses signed values
       // the trig version - pot phase is wrong with this code 
       // double rads=PI/2+PI*(double(lfo[i].acc)/double(0xfffffff)); // convert accumulator value to radians
       // PI/2 shift above gives us a tilted sinewave when rate1 and rate2 are different
      //  lfo[i].dacout=2048+2047*(sin(rads)); // scale and offset the DAC output
        break;
    }

  // scale the output
    int32_t scaledout=((int32_t)lfo[i].dacout*(int32_t)lfo[i].amplitude)>>16; // use long int math here or it will overflow
    lfo[i].scaledout=scaledout; //  

  }

#ifdef MONITOR_CPU1  
  digitalWrite(CPU_USE,0); // low = core1 stalled because I2S buffer is full
#endif
   // send the output value to the appropriate DAC - will stall on buffer full
  DAC1.write(lfo[0].scaledout); // an interesting option would be to have one channel uipolar and one bipolar, or opposite phases, quadrature phases
  DAC1.write(lfo[1].scaledout); // 
  DAC2.write(lfo[2].scaledout); // 
  DAC2.write(lfo[3].scaledout); // 


/*
  static int16_t t=0;
  DAC1.write(t); // an interesting option would be to have one channel uipolar and one bipolar, or opposite phases, quadrature phases
  DAC1.write(t); // 
  DAC2.write(t); // 
  DAC2.write(t); // 
*/
#ifdef MONITOR_CPU1
  digitalWrite(CPU_USE,1); // hi = core1 busy
#endif
}


