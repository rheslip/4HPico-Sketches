


// Copyright 2020 Rich Heslip
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
//
// this is adapted from my XVA1 menu system. this one is bitmapped so its a mix of character and pixel addressing
// Feb 2022 - adapted again as a single encoder menu system - very similar to the Arduino Neu-rah menu system but only 2 levels
// menu items are displayed top to bottom of screen with a title bar
// encoder scrolls menu pages, click to edit parameter
// parameters can be:
// text strings which return ordinals
// 16 bit integers - range and increment set in the submenu table
// floats in range -32.767 to +32.767 - floats are displayed but the parameter behind them is an int16_t so your code has to convert the int to float
// the parameter field in the submenu initializer must point to an integer variable - when you edit the on screen value its this value you are changing
// the handler field in the submenu initializer must be either null or point to a function which is called when you edit the parameter
// Dec 2025 - minor mods for the Twisty 2 controller. ripped out all the file handling stuff because its not needed for this app
// April 2026 - simplified version for 4HPico 64x32 OLED display - one menu+parameter per screen

// these defs for 64x32 display with 8x10 font
#define SCREENWIDTH 64
#define SCREENHEIGHT 32
#define CHARS_X 8  // 8 char display
#define CHARS_Y 2   // 2 lines max
#define DISPLAY_CHAR_WIDTH 8 // character width in pixels - for bitmap displays
#define DISPLAY_CHAR_HEIGHT 10 // character height in pixels - note that some fonts have descenders that go below the 8x10 box
#define NAME_X 0   // pixel x position to display the parameter name
#define NAME_Y DISPLAY_CHAR_HEIGHT   // pixel y position to display the parameter name
#define PARAMETER_X 0   // pixel x position to display the parameter
#define PARAMETER_Y 29   // pixel y position to display the parameter

enum paramtype{TYPE_NONE,TYPE_INTEGER,TYPE_FLOAT, TYPE_TEXT,TYPE_SAMPLENAME}; // parameter display types
enum menumodes{PARAM_SELECT,PARAM_INPUT,WAITBUTTONRELEASE1,WAITBUTTONRELEASE2}; // UI state machine states
static int16_t menustate=PARAM_SELECT; // start out at first menu
int8_t menuindex=0; // current menu

// turn off the display to avoid burn in but keep the display buffer intact
// the next call to display.display() will automagically restore what was there plus whatver has been drawn since

void blankdisplay(void) {
  uint8_t tempbuf[SCREEN_BUFFER_SIZE];
  uint8_t * buf, * tbuf;
  buf=display.getBuffer();
  tbuf= tempbuf;
  memcpy(tbuf,buf,SCREEN_BUFFER_SIZE); // copy frame buffer to temp storage
  display.clearDisplay();
  display.display();  // turn off the OLEDs
  memcpy(buf,tbuf,SCREEN_BUFFER_SIZE); // restore the old frame buffer
}

// update the display and reset the display blanking timer
void updatedisplay(){
  display.display();
  displaytimer=millis();
}

// menu structure 
struct menu {
  const char *name; // display name
  int16_t min;  // min value of parameter
  int16_t max;  // max value of parameter
  int16_t step; // step size. if 0, don't print ie spacer
  enum paramtype ptype; // how its displayed
  const char ** ptext;   // points to array of text for text display
  int16_t *parameter1; // value to modify
  int16_t parameter2; // 2nd parameter to pass to function
  void (*handler)(int16_t,int16_t);  // function to call on value change - called with value of parameter1, parameter2
//  void (*exithandler)(void);  // function to call on exiting value change
};

int16_t nul1,nul2;    // dummy parameter and function for testing
void dummy(int16_t p1,int16_t p2) {
    Serial.printf("p1=%d p2=%d\n",p1,p2);
}

// ********** menu structs that build the menu system below *********

// text arrays used for submenu TYPE_TEXT fields
const char * textoffon[] = {"Off", "On"};
const char * textwaves[] = {"RAMP", "SINE", "EXPO", "QUARTIC", "RANDOM1", "PULSE"}; // wave shape

struct menu menus[] = {
  // displayed name,min,max,step,type,*textfield,*parameter1,parameter2,*handler
 // "Test",0,1000,10,TYPE_FLOAT,0,&nul1,99,dummy,
  "Wave 1",0,NWAVES-1,1,TYPE_TEXT,textwaves,&lfo[2].wave,0,0,  // when both output jacks are used the top row is the 2nd DAC
  "Level",0,32767,100,TYPE_INTEGER,0,&lfo[2].amplitude,0,0,

//  "Feedback",-1000,1000,10,TYPE_FLOAT,0,&fmix,0,0,

};


#define NUM_MENUS sizeof(menus)/ sizeof(menu)

// highlight the currently selected menu item as being edited
void draweditselector(int8_t index) {
  int8_t len=strlen(menus[index].name); // underline the parameter name to indicate edit mode
  int8_t space=DISPLAY_CHAR_WIDTH*(CHARS_X-len)/2; // determine where to start the line
  display.drawFastHLine(space,DISPLAY_CHAR_HEIGHT+2,len*DISPLAY_CHAR_WIDTH,WHITE); // draw underline under the name
  updatedisplay();
}

// draw centered text- makes text more readable on tiny OLED display
// cursor x must be 0 or this won't work correctly
void drawtextcentered(const char * text,int16_t x, int16_t y) {
  int8_t len=strlen(text);
  int8_t space=DISPLAY_CHAR_WIDTH*(CHARS_X-len)/2;
  display.setCursor(x+space,y) ; // set cursor 
  display.printf("%s",text); 
}

// draw parameter value - text is centered to make it look nicer on the small display
// 
void drawparametervalue(int8_t index) {
  // erase what's there - fonts have descenders which go below the 8x10 font box
  display.fillRect(PARAMETER_X,SCREENHEIGHT-(DISPLAY_CHAR_HEIGHT+3),DISPLAY_CHAR_WIDTH*CHARS_X,DISPLAY_CHAR_HEIGHT+3,BLACK); // printing spaces doesn't erase old text properly - Adafruit does OR drawing 
  if (menus[index].step !=0) { // step==0 means don't print 
    int16_t val=*menus[index].parameter1;  // fetch the parameter value
    char temp[CHARS_X+1];
    switch (menus[index].ptype) {
      case TYPE_INTEGER:   // print the value as an unsigned integer    
        sprintf(temp,"%d",val); // 
        drawtextcentered(temp,PARAMETER_X,PARAMETER_Y); 
        break;
      case TYPE_FLOAT:   // print the int value as a float  
        sprintf(temp,"%1.2f",(float)val/1000); // parameter variable must have an int value 1000 X the desired float range. ie -1000 to +1000 is is -1.0 to +1.0. your code has to do the conversion when the variable changes
        drawtextcentered(temp,PARAMETER_X,PARAMETER_Y); 
        break;
      case TYPE_TEXT:  // use the value to look up a string
        if (val > menus[index].max) val=menus[index].max; // sanity check
        if (val < 0) val=0; // min index is 0 for text fields
        if (menus[index].ptext[val]!=0) drawtextcentered(menus[index].ptext[val],PARAMETER_X,PARAMETER_Y); // check for valid char * - filename lists are not fixed in size
        break;
      case TYPE_SAMPLENAME:  // print the sample name of this value
      /*  if (val >= NUM_SAMPLES) val=NUM_SAMPLES-1; // sanity check
        if (val < 0) val=0; // min index is 0 for text fields
        strcpy(temp,sample[val].sname);
        temp[MAX_FILENAME_LENGTH]=0; // chop it off at 8 letters
        drawtextcentered(temp,PARAMETER_X,PARAMETER_Y); // check for valid char 
        */
        break;
      default:
      case TYPE_NONE:  // blank out the field
        drawtextcentered("        ",PARAMETER_X,PARAMETER_Y);
        break;
    } 
  }
}

// display a menu item and its value
// index is the index into the menu array
void drawmenu(int8_t index) {
  display.clearDisplay();
  drawtextcentered(menus[index].name,NAME_X,NAME_Y); // print the name text
  drawparametervalue(index); // print the value
  updatedisplay(); 
}

// menu handler is a run to completion state machine which never blocks 
// allows the rest of the application to run while parameters are adjusted
// April 2026 - simplified menu system with one screen per parameter for 4HPico menus, added button release states for debouncing encoder button

void domenus(void) {
  int16_t enc;     // not using ClickEncoder::button Clicked etc states here - encoder is more responsive because there is no delay waiting for doubleclick detection
  static int16_t debouncecounter;

  enc=menuenc.getValue();

  switch (menustate) {

    case PARAM_SELECT:  // 
      if (enc !=0 ) { // move through menu pages
        menuindex+=enc;
        if (menuindex <0) menuindex=NUM_MENUS-1;  // wrap menus around
        if (menuindex > (NUM_MENUS -1)) menuindex=0; 
        drawmenu(menuindex);  // redraw 
      } 
      if (!digitalRead(ENCSW_IN)) { // menu item has been selected so move to parameter input state
        draweditselector(menuindex); // show we are editing
        menustate=WAITBUTTONRELEASE1;  // intermediate state - wait for button release to avoid race condition
        debouncecounter=DEBOUNCE_CYCLES; 
      }  
      break;
    case WAITBUTTONRELEASE1:
      if (digitalRead(ENCSW_IN)) {
        --debouncecounter;
        if (debouncecounter==0) menustate=PARAM_INPUT;  // if button released move to parameter editing
      }
      break;
    case PARAM_INPUT:  // changing value of a parameter
      if (enc !=0 ) { // change value      
        int16_t temp1=*menus[menuindex].parameter1 + enc*menus[menuindex].step; // menu code uses ints - convert to floats when needed
        if (temp1 < (int16_t)menus[menuindex].min) temp1=menus[menuindex].min;
        if (temp1 > (int16_t)menus[menuindex].max) temp1=menus[menuindex].max;
        *menus[menuindex].parameter1=temp1;
       // if (menus[menuindex].handler != 0) (*menus[menuindex].handler)();  // call the handler function if there is one
        if (menus[menuindex].handler != 0) (*menus[menuindex].handler)(temp1,menus[menuindex].parameter2);  // call the handler function if there is one
        drawparametervalue(menuindex); 
        updatedisplay(); 
      }
      if (!digitalRead(ENCSW_IN)) { // stop changing parameter
        drawmenu(menuindex);
        debouncecounter=DEBOUNCE_CYCLES;
        menustate=WAITBUTTONRELEASE2;
        //if (sub[index].exithandler != 0) (*sub[index].exithandler)();  // call the exit handler function    
      }   
      break;
    case WAITBUTTONRELEASE2:
      if (digitalRead(ENCSW_IN)) {
        --debouncecounter;
        if (debouncecounter<=0) menustate=PARAM_SELECT;  // if button released move to parameter editing
      }
      break;
    default:
      menustate=PARAM_SELECT;
      break;
  } // end of case statement
}


