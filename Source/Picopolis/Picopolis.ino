
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
 * June 2026 -  Picopolis - Dual Metropolis style sequencer for the 4HPico 
 */

#include <Arduino.h>
#include "4HPico.h"
#include <I2S.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Monospaced_12.h"
#include "Clickencoder.h"


#define SAMPLERATE 44100  // high sample rate gives best resolution/lowest latency on CV and gate outputs
#define CLOCKIN AIN2  // top left jack is clock input
#define RESETIN AIN3   // top right jack resets the sequencer
#define CVIN_VOLT 580.6  // a/d count per volt - **** adjust this value to calibrate V/octave input
//#define CVOUT_VOLT 6554 // D/A count per volt - nominally +-5v range for -+32767 DAC values- ***** adjust this value to calibrate V/octave out
#define CVOUT_VOLT1 6553// D/A count per volt - nominally +-5v range for -+32767 DAC values- ***** adjust this value to calibrate V/octave out
#define CVOUT_VOLT2 5517 // D/A count per volt - nominally +-5v range for -+32767 DAC values- ***** adjust this value to calibrate V/octave out
#define CVOUTMIN -2*CVOUT_VOLT1  // lowest output CV ie MIDI note 0
#define GATEHIGH -32767    // DAC values for gate levels
#define GATELOW 32767

I2S DAC1(OUTPUT);  // 
I2S DAC2(OUTPUT);  // 

// stuff related to main UI
enum UISTATES {SHOWSEQUENCERS,DOSEQUENCERS,SHOWMENUS,SHOWBOTTOMMENU,SELECTBOTTOMMENU,DORMANT,WAIT_BUTTON_RELEASE,WAIT_STAGE_BUTTON_RELEASE,
  WAIT_EDIT_BUTTON_RELEASE,WAIT_MENU_BUTTON_RELEASE,EDITOFFSET,EDITPULSES,EDITGATEMODE,EDITENABLE,WAIT_MENUEXIT_BUTTON_RELEASE,
  WAIT_SECONDARY_MENU_BUTTON_RELEASE};
int16_t BOTTOMMENUSTATES[] = {EDITOFFSET,EDITPULSES,EDITGATEMODE,EDITENABLE}; // states used for bottom menu editing
int16_t UI_state=SHOWSEQUENCERS; // initial UI state
enum GATEMODES {ONEPULSE,ALLPULSES,GATEMODEHIGH,GATEMODELOW,RATCHETS2,RATCHETS3,RATCHETS4,NUMGATEMODES};
enum STAGEMODES {FORWARD,REVERSE,PINGPONG,BROWNIAN,NUMSTAGEMODES};
enum CVMODES {PITCH,PITCHADD,PITCHSUBTRACT,PITCHBAcc,NUMCVMODES};
enum ENABLEMODES {DISABLED,ENABLED,SLIDE,NUMENABLEMODES};
#define FWD 1    // stage step direction increment
#define REV -1
const char * menugatemodes[] = {"1P","NP","Hi","Lo","R2","R3","R4"};
const char * menuenabled[] = {"X","O","/"};

int16_t stage=0;
int16_t sequencer=0;
int16_t UI_stage=0;
int16_t bottommenuindex=0;
#define BOTTOMMENUITEMS 4

#define DEBOUNCE_CYCLES 100 // counter to debounce button release

uint32_t displaytimer; // display blanking timer

ClickEncoder menuenc(ENCA_IN,ENCB_IN,ENCSW_IN,ENCDIVIDE); // menu encoder object

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#define DISPLAY_BLANK_MS 300000  // display blanking time

#include "seq.h"
#include "menusystem.h"  // has to come after display and encoder objects creation
#include "graphics.h"

int16_t cvoffset[NUM_SEQUENCERS]={12000,12000};; // base pitch set in menus



// RP2040 timer code from https://github.com/raspberrypi/pico-examples/blob/master/timer/timer_lowlevel/timer_lowlevel.c
// Use alarm 0 for encoder timer interrupt
// use alarm 1 for gate interrupt

#define ALARM_ENCODER 0
#define ALARM_ENCODER_IRQ timer_hardware_alarm_get_irq_num(timer_hw, ALARM_ENCODER)

static void encoder_alarm_in_us(uint32_t delay_us) {
  hw_set_bits(&timer_hw->inte, 1u << ALARM_ENCODER);
  irq_set_exclusive_handler(ALARM_ENCODER_IRQ, encoder_irq);
  irq_set_enabled(ALARM_ENCODER_IRQ, true);
  encoder_alarm_in_us_arm(delay_us);
}

static void encoder_alarm_in_us_arm(uint32_t delay_us) {
  uint64_t target = timer_hw->timerawl + delay_us;
  timer_hw->alarm[ALARM_ENCODER] = (uint32_t) target;
}

// encoder IRQ handler
static void encoder_irq(void) {
  menuenc.service(); // handle the menu encoder
  hw_clear_bits(&timer_hw->intr, 1u << ALARM_ENCODER); // clear IRQ flag
  encoder_alarm_in_us_arm(TIMER_MICROS);  // reschedule interrupt
}


void setup() {
  Serial.begin(115200);
 // while (!Serial);

// init IO ports
  pinMode(ENCA_IN, INPUT_PULLUP);  // menu encoder and switch
  pinMode(ENCB_IN, INPUT_PULLUP);    
  pinMode(ENCSW_IN, INPUT_PULLUP); 
  pinMode(CLOCKIN, INPUT);    
  pinMode(RESETIN, INPUT); 

// set up I2C pins 
  Wire.setSDA(PIN_WIRE_SDA);
  Wire.setSCL(PIN_WIRE_SCL);
  Wire.begin();

// set up timer interrupt 
  encoder_alarm_in_us(TIMER_MICROS);

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
  display.printf(" 4HPico\n Metrop"); // power on message
  display.display();
  delay(1000);
  displaytimer=millis(); // reset display blanking timer
 // drawmenu(0); // show first menu item

// set up Pico I2S for PT8211 stereo DAC
	DAC1.setBCLK(BCLK);
	DAC1.setDATA(I2S_DATA);
	DAC1.setBitsPerSample(16);
//	DAC1.setBuffers(1, 128, 0); // DMA buffer - 32 bit L/R words
  DAC1.setBuffers(1, 1, 0); // small DMA buffer to minimize CV/Gate latency
	DAC1.setLSBJFormat();  // needed for PT8211 which has funny timing
	DAC1.begin(SAMPLERATE);

  // set up Pico I2S for PT8211 stereo DAC
	DAC2.setBCLK(BCLK1);
	DAC2.setDATA(I2S_DATA1);
	DAC2.setBitsPerSample(16);
//	DAC2.setBuffers(1, 128, 0); // DMA buffer - 32 bit L/R words
  DAC2.setBuffers(1, 1, 0); // small DMA buffer to minimize CV/Gate latency
	DAC2.setLSBJFormat();  // needed for PT8211 which has funny timing
	DAC2.begin(SAMPLERATE);  

  UI_state=SHOWSEQUENCERS;
  resetsequencers();
}

// first Pico core does UI 
void loop() {
  static int16_t debouncecounter;
  static int32_t longpresstimer;
  int16_t encvalue;

  static int16_t temp,counter;
  static uint32_t looptimer,latency;

/*
  latency=micros()-looptimer;
  looptimer=micros();
  if (++counter >=5000) {
    Serial.printf("looptime %U\n",latency);
    counter=0;
  }

  if (temp!= UI_state) {
    Serial.printf("last state %d new state %d\n",temp,UI_state);
    temp=UI_state;
  }
*/

  if ((millis()-displaytimer) > DISPLAY_BLANK_MS ) {
    UI_state=DORMANT;  // 
    blankdisplay(); // protect the OLED from burnin
  } 

  // update sequencer screen stage locations when on main screen. Do not update when using menusystem or display blanked
  if ((UI_state != DORMANT) && (UI_state != SHOWMENUS) && (UI_state !=WAIT_SECONDARY_MENU_BUTTON_RELEASE)) updatestagepositions();   

// UI state machine - edits main sequencer parameters on graphics page, calls the menusystem for additional "deep" parameter editing
// non-blocking which is essential for fast response to user input
// I measured it - the loop latency is only a few microseconds!

  switch (UI_state) {
    case SHOWSEQUENCERS:
      display.clearDisplay();
      drawstages();
      drawunderscore(UI_stage);
      drawbottommenu(sequencer,stage);
      UI_state=DOSEQUENCERS;
      break;
    case DOSEQUENCERS:
      encvalue=menuenc.getValue();
      if (encvalue!=0) {
        undrawunderscore(UI_stage);
        UI_stage+=encvalue;
        if (UI_stage >= (NUM_SEQUENCERS*NUM_STAGES)) UI_stage=0;
        if (UI_stage < 0) UI_stage=(NUM_SEQUENCERS*NUM_STAGES)-1;
        sequencer=UI_stage/NUM_STAGES;
        stage=UI_stage%NUM_STAGES;
        drawbottommenu(sequencer,stage);
        drawunderscore(UI_stage);
      }
      if (!digitalRead(ENCSW_IN)) { // stage selected, do bottom menus
        longpresstimer=millis();
        UI_state=WAIT_STAGE_BUTTON_RELEASE;
      }
      break;  
    case WAIT_STAGE_BUTTON_RELEASE:  // intermediate state - wait for button release to avoid race condition
      if (digitalRead(ENCSW_IN)) {
        -- debouncecounter;
        if (debouncecounter==0) {
          UI_state=BOTTOMMENUSTATES[bottommenuindex];  // go to the edit state for current item 
        }
      }
      else if ((millis()-longpresstimer) > LONGPRESS) { // longpress to enter secondary menusystem
        menuindex=NUM_MENUS/2*sequencer; // set menu entry point to the sequencer we are on
        drawmenu(menuindex);  // draw last large menu used
        UI_state=WAIT_SECONDARY_MENU_BUTTON_RELEASE; // move on to secondary menus
      }
      break; 
    case SELECTBOTTOMMENU:  // scroll thru bottom menu items while encoder button is pressed
      encvalue=menuenc.getValue();
      if (encvalue!=0) {
        undrawbottomunderscore(bottommenuindex);
        bottommenuindex+=encvalue;
        bottommenuindex=constrain(bottommenuindex,0,BOTTOMMENUITEMS-1);
        drawbottomunderscore(bottommenuindex);
      }
      if (digitalRead(ENCSW_IN)) { // button released
        UI_state=WAIT_EDIT_BUTTON_RELEASE;
      }      
      break; 
    case WAIT_EDIT_BUTTON_RELEASE:  // intermediate state - wait for button release when a new bottom menu item was selected for editing
      if (digitalRead(ENCSW_IN)) {
        -- debouncecounter;
        if (debouncecounter==0) {
          UI_state=BOTTOMMENUSTATES[bottommenuindex];  // go to the edit state for that item
        }
      } 
      break;  
    case WAIT_MENU_BUTTON_RELEASE:  // intermediate state - bottom menu item clicked while editing
      if (digitalRead(ENCSW_IN)) {
        -- debouncecounter;
        if (debouncecounter==0) {
          UI_state=DOSEQUENCERS;
          drawunderscore(UI_stage); 
          undrawbottomunderscore(bottommenuindex); // 
        }
      }
      else if ((millis()-longpresstimer) > LONGPRESS) { // longpress to scroll thru bottom menu items
          UI_state=SELECTBOTTOMMENU;  // go to the edit state for that item
          drawbottomunderscore(bottommenuindex);
      }
      break;    
    case EDITOFFSET:
      flashbottomunderscore(bottommenuindex); // show we are in edit mode
      flashunderscore(UI_stage);
      encvalue=menuenc.getValue();
      if (encvalue!=0) {  // adjust the value
        pitch_offset[sequencer][stage]=constrain(pitch_offset[sequencer][stage]+encvalue,-24,24);
        drawbottommenu(sequencer,stage);
      }
      if (!digitalRead(ENCSW_IN)) { // button clicked, exit editing or select new menu item
        longpresstimer=millis();
        drawbottomunderscore(bottommenuindex);  // visual cue for longpress case
        UI_state=WAIT_MENU_BUTTON_RELEASE;
      }      
      break;  
    case EDITPULSES:
      flashbottomunderscore(bottommenuindex); // show we are in edit mode
      flashunderscore(UI_stage);
      encvalue=menuenc.getValue();
      if (encvalue!=0) {  // adjust the value
        pulses[sequencer][stage]=constrain(pulses[sequencer][stage]+encvalue,1,8);
        drawbottommenu(sequencer,stage);
      }
      if (!digitalRead(ENCSW_IN)) { // button clicked , exit editing or select new menu item
        longpresstimer=millis();
        UI_state=WAIT_MENU_BUTTON_RELEASE;
      }      
      break; 
    case EDITGATEMODE:
      flashbottomunderscore(bottommenuindex); // show we are in edit mode
      flashunderscore(UI_stage);
      encvalue=menuenc.getValue();
      if (encvalue!=0) {  // adjust the value
        gatemode[sequencer][stage]=constrain(gatemode[sequencer][stage]+encvalue,0,NUMGATEMODES-1);
        drawbottommenu(sequencer,stage);
      }
      if (!digitalRead(ENCSW_IN)) { // button clicked , exit editing or select new menu item
        longpresstimer=millis();
        UI_state=WAIT_MENU_BUTTON_RELEASE;
      }      
      break; 
    case EDITENABLE:
      flashbottomunderscore(bottommenuindex); // show we are in edit mode
      flashunderscore(UI_stage);
      encvalue=menuenc.getValue();
      if (encvalue!=0) {  // adjust the value
        undrawstage(sequencer,stage); // redraw the enabled status
        enabled[sequencer][stage]=constrain(enabled[sequencer][stage]+encvalue,0,NUMENABLEMODES-1);
        drawstage(sequencer,stage); 
        drawbottommenu(sequencer,stage);
      }
      if (!digitalRead(ENCSW_IN)) { // button clicked , exit editing or select new menu item
        longpresstimer=millis();
        UI_state=WAIT_MENU_BUTTON_RELEASE;
      }      
      break; 
    case WAIT_SECONDARY_MENU_BUTTON_RELEASE:  // intermediate state - wait for button release before entering manusystem
      if (digitalRead(ENCSW_IN)) {
        -- debouncecounter;
        if (debouncecounter==0) {
          UI_state=SHOWMENUS;  // go to the edit state for that item
        }
      } 
      break; 
    case SHOWMENUS:
      if (domenus()){   // true if user exited the menusystem
        menustate=PARAM_SELECT; // reset menu state for next time
        display.clearDisplay(); // redraw seq screen as visual cue
        drawstages();
        drawunderscore(UI_stage);
        drawbottommenu(sequencer,stage);
        UI_state=WAIT_MENUEXIT_BUTTON_RELEASE; 
      };  
      break; 
    case WAIT_MENUEXIT_BUTTON_RELEASE:  // wait for menusystem longpress release
      if (digitalRead(ENCSW_IN)) {
        -- debouncecounter;
        if (debouncecounter==0) {
          UI_state=DOSEQUENCERS;
          undrawbottomunderscore(bottommenuindex); // 
        }
      }
      break; 
    case DORMANT:  // using menu encoder will start screen up again
      encvalue=menuenc.getValue();
      if (encvalue || !digitalRead(ENCSW_IN)) {
        updatedisplay(); // restore the display
        debouncecounter=DEBOUNCE_CYCLES;
        UI_state=WAIT_BUTTON_RELEASE;
      }
      break; 
    default:
      UI_state=SHOWSEQUENCERS;
      break;
  }
}

// find first active stage
int16_t firststage(int16_t s) {
  for (int16_t i=0; i< NUM_STAGES; ++i) 
    if (enabled[s][i]) return i;
  return 0; // shouldn't happen unless all stages are disabled
}

// find last active stage
int16_t laststage(int16_t s) {
  for (int16_t i=NUM_STAGES-1; i>=0; --i) 
    if (enabled[s][i]) return i;
  return 0; // shouldn't happen unless all stages are disabled
}

// nextstage - find next active stage
int16_t nextstage(int16_t s) {
  int16_t i=seq[s].index;
  int16_t len=NUM_STAGES;
  while (len > 0) {
    i+=seq[s].direction;
    if(i<0) i=NUM_STAGES-1; // handle wrap around
    if (i>(NUM_STAGES-1)) i=0;
    if (enabled[s][i]) return i;
    --len;
  }
  return 0; // only time this should happen is if all stages are disabled
}

void resetsequencers(void) {
  seq[0].index=seq[1].index=0; // 
  seq[0].pulsecounter=seq[1].pulsecounter=1; // pulses count up from 1
  seq[0].clockcounter=seq[0].clockdivider;
  seq[1].clockcounter=seq[1].clockdivider;
}

// second core does sequencer logic and sends CV and gate signals to DAC
// note - 2 cores can share variables as long as only ONE of them writes the variable
// in this case the UI running on core 1 writes the variables that affect sequencer behavior and core 2 reads those variables
// core 2 writes some counters that core 1 never uses. Core 1 reads the stage the sequencers are on to update the display

// second core setup
void setup1() {
delay (1000); // wait for main core to start up peripherals
}



void loop1() {
  static int32_t cvout[NUM_SEQUENCERS],lastcvout[NUM_SEQUENCERS];
  static int16_t gate[NUM_SEQUENCERS];
  static uint32_t gatetimer[NUM_SEQUENCERS]={0,0};
  static uint32_t gateontime[NUM_SEQUENCERS];
  static uint32_t gateofftime[NUM_SEQUENCERS];
  static int8_t gatecount[NUM_SEQUENCERS];
  static int8_t gatestate[NUM_SEQUENCERS];
  static int8_t slew[NUM_SEQUENCERS];
  static int32_t slewrate[NUM_SEQUENCERS];
  static uint32_t thisclock,lastclock,clockperiod;
  static bool clocked=0;
  static int16_t clockcnt;
  enum gatestates {GATEBEGIN,GATEDELAY,GATEONTIME,GATEOFFTIME,GATEON};

#define GATEDELAYTIME 1000 // delay gate a short time to insure CV is stable

/*
 // loop latency timer - should be pretty accurate other than it contributes to loop latency
  static uint32_t looptimer,latency,maxlatency;
  static uint32_t loopcounter=0; 
#define MAXCOUNT 100000

  if (loopcounter==0) {
    maxlatency=0;
    looptimer=micros(); // set time reference for next loop measurment
    ++loopcounter;
  }
  else {
    latency=micros()-looptimer;
    looptimer=micros();
    if (latency > maxlatency) maxlatency=latency; // capture max value
    if (++loopcounter >= MAXCOUNT) {
      Serial.printf("avg looptime %U max loop time %U over %U loops\n",latency,maxlatency,loopcounter);
      loopcounter=0; // star over. printf should not affect the first loop measurement
    }
  }
*/

// reset when input is high
  if (!digitalRead(RESETIN)) resetsequencers(); // look for reset - reset input is inverted


// gate state machine
// **** gate off can go to zero at high clock rates > 175bpm or so, ratcheting and high gate length 90%
// I think the problems is a combination of loop latency and using I2S to output the gate. timing wobbles around somewhat
  for (int16_t i=0;i<NUM_SEQUENCERS;++i) {
    switch (gatestate[i]) {
      case GATEBEGIN:     // if there are gates pending start the gate sequence, otherwise hold gate low
        gate[i]=GATELOW;
        if (gatecount[i] >0) { // count down the gates
          --gatecount[i];
          gatetimer[i]=micros();
          gatestate[i]=GATEDELAY;
        }
        break;
      case GATEDELAY:   // short delay before setting gate high to allow CV to settle
        if((micros()-gatetimer[i]) > GATEDELAYTIME) {
          gatetimer[i]=micros();
          gatestate[i]=GATEONTIME;
          gate[i]=GATEHIGH;
        }
        break;
      case GATEONTIME:  // gate on timing
        if((micros()-gatetimer[i]) > gateontime[i]) {
          gatetimer[i]=micros();
          gatestate[i]=GATEOFFTIME;
          gate[i]=GATELOW;
        }
        break;
      case GATEOFFTIME: // gate off timing - needed for ratchets
        if((micros()-gatetimer[i]) > gateofftime[i]) {
          gatestate[i]=GATEBEGIN;         
        }
        break;
      case GATEON:  // state used for gate high mode 
        gate[i]=GATEHIGH;
        break;
      default:
        gatestate[i]=GATEBEGIN;
    }
  }

// note that the clock is not debounced. my Performer only outputs a 2ms clock pulse which could be missed if loop latency is high
//  if (!digitalRead(CLOCKIN)) {  // look for rising edge of clock input which is inverted
  if (analogRead(CLOCKIN) < 600) {  // look for rising edge of clock input which is inverted  - **** hack for using 3.3v clock from  my sampler module

    if (!clocked) {
      clocked=1;
      ++clockcnt;  
      lastclock=thisclock; // calculate clock period
      thisclock=micros();
      if ((thisclock-lastclock) < 2000000) {   // change clock period if clock is > 30 bpm - otherwise assume its stopped
        if(clockcnt >=2)  clockperiod=thisclock-lastclock; // we don't change clock period until we have a couple of clocks
      }
      else clockcnt=0;

 //     Serial.printf("clockperiod %ld count %d\n",clockperiod,clockcnt);
      for (int16_t i=0;i<NUM_SEQUENCERS;++i) {
        if (--seq[i].clockcounter<=0) { // do clock divider
          seq[i].clockcounter=seq[i].clockdivider; // at this point we are clocking the current stage which will have at least 1 pulse
          seq[i].current=seq[i].index;  //current stage is saved so the display gets updated when we trigger this stage. also used in CV modes below because we need to clock BOTH stages before processing CV
          // when index gets updated below its pointing to the next stage, not the one thats active
          seq[i].clocked=1; // flag this stage as clocked 

          // schedule gates  - gate timing is done in the state machine above
          gatecount[i]=1;  // initialize the gate state machine for 1 gate
          gatestate[i]=GATEBEGIN; 
          gateontime[i]=((clockperiod-GATEDELAY)*seq[i].gatelength)/100; // gatelength is a percentage of clock period
          gateofftime[i]=(clockperiod-GATEDELAY)-gateontime[i]-2000; // make the off time a bit shorter to help compensate for loop latency
          switch (gatemode[i][seq[i].index]) { // handle gate modes
            case ALLPULSES: // state machine will do 1 gate per pulse as set above
              break;
            case ONEPULSE:
              if (seq[i].pulsecounter != 1) gatecount[i]=0;  // no gates after the first one
              break;
            case GATEMODEHIGH:
              gatestate[i]=GATEON;
              break;
            case GATEMODELOW: 
              gatecount[i]=0;  // state machine will keep gate low if count is zero
              break;
            case RATCHETS2: // 2 gates per clock
              gatecount[i]=2;
              gateontime[i]=gateontime[i]/2;
              gateofftime[i]=gateofftime[i]/2; // 
              break;
            case RATCHETS3: // 3 gates per clock
              gatecount[i]=3;
              gateontime[i]=gateontime[i]/3;
              gateofftime[i]=gateofftime[i]/3; // 
              break;
            case RATCHETS4: // 4 gates per clock
              gatecount[i]=4;
              gateontime[i]=gateontime[i]/4;
              gateofftime[i]=gateofftime[i]/4; // 
              break;
            default:
              break;
          };


          // figure out which stage is next and set the stage index to the stage to be processed on the next clock
          // restart the sequence if pulse limit is hit
          if ((seq[i].maxpulses !=0) && (++seq[i].totalpulses >= seq[i].maxpulses)) { // if we hit the pulse limit end the cycle
            seq[i].totalpulses=0;
            seq[i].pulsecounter=pulses[i][seq[i].index]; // force end of cycle
            seq[i].stagecounter=0;
           // if (seq[i].maxpulses==1) seq[i].current=-1; // special case - fake a stage change to get display to update
          }
          if (++seq[i].pulsecounter > pulses[i][seq[i].index]) {  // if we hit the last pulse on this stage go to next stage
            seq[i].pulsecounter=1; // reset pulse counter for next stage
            // process number of stages 
            if (--seq[i].stagecounter <=0 ) { // if we are on the last stage
              seq[i].stagecounter=seq[i].stages;
              switch (seq[i].stagemode) {
                case REVERSE:
                  seq[i].direction=REV;
                  seq[i].index=laststage(i); 
                 // Serial.printf("rev mode stages %d index %d  direction %d\n",seq[i].stagecounter,seq[i].index,seq[i].direction);
                  break;
                case FORWARD:
                  seq[i].direction=FWD;
                  seq[i].index=firststage(i); 
                //  Serial.printf("fwd mode stages %d index %d  direction %d\n",seq[i].stagecounter,seq[i].index,seq[i].direction);
                  break;
                case PINGPONG: // pingpong only works "correctly" when stages is set to number of active stages - otherwise it generates pingpong with jump at end - its cool tho!
                  seq[i].stagecounter=seq[i].stages-1;
                  if (seq[i].direction==FWD) {
                    seq[i].direction=REV; 
                    seq[i].index=laststage(i); 
                  }
                  else {
                    seq[i].direction=FWD;
                    seq[i].index=firststage(i);
                  }
                  break;
                default:                 
                  break;
              }
            }
            else {  // not on last stage, handle stage movement
              switch (seq[i].stagemode) {
                case BROWNIAN:
                  if (random(2)) seq[i].direction=REV; // random back and forth
                  else seq[i].direction=FWD;
                  break;
                case PINGPONG:  // reverse direction at last/first stage
                //  Serial.printf("seq %d index %d first %d last %d next %d direction %d\n",i,seq[i].index,firststage(i),laststage(i),nextstage(i),seq[i].direction);
                //  if (seq[i].index==laststage(i)) seq[i].direction=REV;
                 // if (seq[i].index>=(seq[i].stages-1)) seq[i].direction=REV;
                 // if (seq[i].index==firststage(i)) seq[i].direction=FWD;
                  break;
                default:  
                  break;
              }
              seq[i].index=nextstage(i);  // move to next stage using current direction
            }
          }
        }
        else seq[i].clocked=0; // flag this stage as not clocked 
      }
    }
  }
  else {   
      clocked=0;
  }

// process CVs. broken out of loop above so both sequencers have been processed before we potentially combine CVs
  for (int16_t i=0;i<NUM_SEQUENCERS;++i) {
    if (seq[i].clocked) {
      lastcvout[i]=cvout[i]; // save current pitch for slewing
      switch (seq[i].CVmode) { // 1v per octave. note numbers are MIDI style 0-127. DAC out is inverted.
        case PITCH: // use this sequencer pitch + root
          cvout[i]=-(quantize(pitch_offset[i][seq[i].current]+seq[i].root,scales[seq[i].scale],0)*(CVOUT_VOLT1/12)+CVOUTMIN+cvoffset[i]); 
          break;
        case PITCHADD: // add both sequencer pitches + root
          cvout[i]=-(quantize(pitch_offset[i][seq[i].current]+pitch_offset[i^1][seq[i^1].current]+seq[i].root,scales[seq[i].scale],0)
            *(CVOUT_VOLT1/12)+CVOUTMIN+cvoffset[i]);
          break;
          case PITCHSUBTRACT: // subtract other sequencer pitch + root
          cvout[i]=-(quantize(pitch_offset[i][seq[i].current]-pitch_offset[i^1][seq[i^1].current]+seq[i].root,scales[seq[i].scale],0)
            *(CVOUT_VOLT1/12)+CVOUTMIN+cvoffset[i]);
          break;
        case PITCHBAcc:  // add pitch of other sequencer to accumulator 
          accumulator[i][seq[i].current]+=pitch_offset[i^1][seq[i^1].current];
          int16_t span=acc_max[i]-acc_min[i]; // menu range set up so span is at least 1 so we don't divide by zero below
          // if we go out of range wrap around
       //   Serial.printf("acc modulo = %d\n",accumulator[i][seq[i].current]%span);
          if (accumulator[i][seq[i].current] > acc_max[i]) accumulator[i][seq[i].current]=acc_min[i]+(accumulator[i][seq[i].current]%span-acc_max[i]); 
          if (accumulator[i][seq[i].current] < acc_min[i]) accumulator[i][seq[i].current]=acc_max[i]+(accumulator[i][seq[i].current]%span-acc_min[i]);
      //    Serial.printf("acc[%d][%d] = %d  span %d\n",i,seq[i].current,accumulator[i][seq[i].current],span);
          
          cvout[i]=-(quantize(pitch_offset[i][seq[i].current]+accumulator[i][seq[i].current]+seq[i].root,scales[seq[i].scale],0)
            *(CVOUT_VOLT1/12)+CVOUTMIN+cvoffset[i]);
          break;
      }
      cvout[i]=cvout[i]<<16; // use 16:16 so we can slew by int:fraction
      // calculate slew rate if this step is a slide
      if(enabled[i][seq[i].current]==SLIDE) {
        int16_t sl = SAMPLERATE/(52-cvslew[i]*5); // slew rate comes from secondary menu value
        slewrate[i]=(cvout[i]-lastcvout[i])/sl; 
    //    Serial.printf("stage %d slewrate=%ld span=%ld\n",seq[i].current,slewrate[i],lastcvout[i]-cvout[i]);      
      }
      else {
        slewrate[i]=0;
        lastcvout[i]=cvout[i]; // no slewing on this step so set slew CV to current CV
      }
    }
    seq[i].clocked=0; // we have done CV processing so reset clock flag
  }


 
// process next sample if there is room in the I2S output buffer
  if (DAC1.availableForWrite() >=4) { 
    // do CV slew (portamento)
    // slew time is constant - rate depends on last and this CV. we do linear CV slew - most oscillators respond exponentially to CV input
    // if you multiply the slew rate by a constant each time it could be turned into an exponential
    if (abs(cvout[0]-lastcvout[0]) <= abs(slewrate[0])) lastcvout[0]=cvout[0]; // when its close force it to the correct CV target
    else lastcvout[0]+=slewrate[0];
    if (abs(cvout[1]-lastcvout[1]) <= abs(slewrate[1])) lastcvout[1]=cvout[1]; // when its close force it to the correct CV target
    else lastcvout[1]+=slewrate[1];

    DAC1.write((int16_t)(lastcvout[0]>>16)); // left
    DAC1.write((int16_t)(lastcvout[1]>>16)); // right
//    DAC1.write((int16_t)(cvout[0]>>16)); // left
//    DAC1.write((int16_t)(cvout[1]>>16)); // right
  }
 
  if (DAC2.availableForWrite() >=4) {  
    DAC2.write(gate[0]); // left
    DAC2.write(gate[1]); // right
  }

}


