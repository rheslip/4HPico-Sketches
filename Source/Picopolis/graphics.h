
// graphics related definitions and functions

#define FLASHINTERVAL 125  // cursor flash rate
// canvas is the part of the screen used for graphics

#define CANVAS_ORIGIN_X  0 // upper right corner
#define CANVAS_ORIGIN_Y  0 // upper right corner
#define CANVAS_HEIGHT 22 
#define CANVAS_WIDTH 64 
#define ROW_HEIGHT 11
#define CIRCLE_RADIUS 3
#define CROSS_LENGTH 2
#define ICON_SIZE (CANVAS_WIDTH/NUM_STAGES)


// locations of items on bottom menu
#define BOTTOM_FONTWIDTH 6
int16_t bottommenu_x[]={2,2+BOTTOM_FONTWIDTH*4,2+BOTTOM_FONTWIDTH*6,2+BOTTOM_FONTWIDTH*9};
int16_t bottommenu_width[]={BOTTOM_FONTWIDTH*3,BOTTOM_FONTWIDTH,BOTTOM_FONTWIDTH*2,BOTTOM_FONTWIDTH};

// plot a stage on the screen
// index = position 0-7
// seq = sequencer # 0-1
void drawstage(uint16_t seq,uint16_t stage) {
  int x=CANVAS_ORIGIN_X+ICON_SIZE*stage+ICON_SIZE/2;
  int y=CANVAS_ORIGIN_Y + ROW_HEIGHT*seq+ICON_SIZE/2; //
  switch (enabled[seq][stage]) {
    case ENABLED:
      display.drawCircle(x,y,CIRCLE_RADIUS,WHITE);
      break;
    case DISABLED:
      display.drawLine(x-CROSS_LENGTH,y-CROSS_LENGTH,x+CROSS_LENGTH,y+CROSS_LENGTH,WHITE);
      display.drawLine(x-CROSS_LENGTH,y+CROSS_LENGTH,x+CROSS_LENGTH,y-CROSS_LENGTH,WHITE);
      break;
    case SLIDE:
      display.drawLine(x-CROSS_LENGTH,y+CROSS_LENGTH,x+CROSS_LENGTH,y-CROSS_LENGTH,WHITE);
      break;
  }
  updatedisplay();
}

// erase a stage on the screen
// index = position 0-7
// seq = sequencer # 0-1
void undrawstage(uint16_t seq,uint16_t stage) {
  int x=CANVAS_ORIGIN_X+ICON_SIZE*stage;
  int y=CANVAS_ORIGIN_Y + ROW_HEIGHT*seq; //
  display.fillRect(x,y,ICON_SIZE,ICON_SIZE,BLACK);
  updatedisplay();
}

// draw all the stages
void drawstages(void) {
  for (int16_t stage=0;stage<NUM_STAGES;++stage)
    for (int16_t seq=0;seq<NUM_SEQUENCERS;++seq) drawstage(seq,stage);
}

// plot stage index on the screen
// seq = sequencer # 0-1
// index = position 0-7
void drawindex(uint16_t seq,uint16_t stage) {
  int x=CANVAS_ORIGIN_X+ICON_SIZE*stage+ICON_SIZE/2;
  int y=CANVAS_ORIGIN_Y + ROW_HEIGHT*seq+ICON_SIZE/2; //
  display.fillCircle(x,y,CIRCLE_RADIUS,WHITE);
  if (enabled[seq][stage]==SLIDE)display.drawLine(x-CROSS_LENGTH,y+CROSS_LENGTH,x+CROSS_LENGTH,y-CROSS_LENGTH,BLACK);
  updatedisplay();
}

void undrawindex(uint16_t seq,uint16_t stage) {
  int x=CANVAS_ORIGIN_X+ICON_SIZE*stage+ICON_SIZE/2;
  int y=CANVAS_ORIGIN_Y + ROW_HEIGHT*seq+ICON_SIZE/2; //
  display.fillCircle(x,y,CIRCLE_RADIUS,BLACK); //erase old index
  switch (enabled[seq][stage]) {
    case DISABLED: // should not happen but sometimes it does 
      display.drawLine(x-CROSS_LENGTH,y-CROSS_LENGTH,x+CROSS_LENGTH,y+CROSS_LENGTH,WHITE); // ***** with this in it somehow messes up the sequencer when in the secondary menus - WTF???
      display.drawLine(x-CROSS_LENGTH,y+CROSS_LENGTH,x+CROSS_LENGTH,y-CROSS_LENGTH,WHITE); // changing optimize to (-O) seems to fix it - compiler bug???
      break; 
    case SLIDE:
      display.drawLine(x-CROSS_LENGTH,y+CROSS_LENGTH,x+CROSS_LENGTH,y-CROSS_LENGTH,WHITE);
      break; 
    default:
    case ENABLED:
      display.drawCircle(x,y,CIRCLE_RADIUS,WHITE);
      break;
  } 
  updatedisplay();
}

/*/
void undrawindex(uint16_t seq,uint16_t stage) {
  undrawstage(seq,stage);
  drawstage(seq,stage);
}
*/
// update stage position
// seq = sequencer # 0-1
// added seq[].current because index change the clock BEFORE the clock that sends gate out
void updatestagepositions(void) {
  static int16_t lastindex[NUM_SEQUENCERS]={0,0};
  for (int16_t i=0; i< NUM_SEQUENCERS; ++i) {
    if (lastindex[i] != seq[i].current) {
      undrawindex(i,lastindex[i]);
      lastindex[i]= seq[i].current;
      drawindex(i,lastindex[i]);
    }
  }
}

void drawunderscore(uint16_t index) {
  int x=CANVAS_ORIGIN_X+ICON_SIZE*(index%NUM_STAGES);
  int y=CANVAS_ORIGIN_Y + ROW_HEIGHT*(index/NUM_STAGES)+ICON_SIZE+2; //  
  if (y>=SCREEN_HEIGHT) y=y-1;
  display.drawLine(x,y,x+ICON_SIZE,y,WHITE);
  updatedisplay();
}

void undrawunderscore(uint16_t index) {
  int x=CANVAS_ORIGIN_X+ICON_SIZE*(index%NUM_STAGES);
  int y=CANVAS_ORIGIN_Y + ROW_HEIGHT*(index/NUM_STAGES)+ICON_SIZE+2; //  
  if (y>=SCREEN_HEIGHT) y=y-1;
  display.drawLine(x,y,x+ICON_SIZE,y,BLACK);
  updatedisplay();
}

void showoffsetvalue(int16_t val) {
  int16_t x=bottommenu_x[0];
  display.setCursor(x,CANVAS_HEIGHT+1);
  display.printf("%3d",val);
  updatedisplay();
}

void showpulses(int16_t val) {
  int16_t x=bottommenu_x[1];
  display.setCursor(x,CANVAS_HEIGHT+1);
  display.printf("%d",val);
  updatedisplay();
}

void showgatemode(int16_t val) {
  int16_t x=bottommenu_x[2];
  display.setCursor(x,CANVAS_HEIGHT+1);
  display.printf("%s",menugatemodes[val]);
  updatedisplay();
}

void showenabled(int16_t val) {
  int16_t x=bottommenu_x[3];
  display.setCursor(x,CANVAS_HEIGHT+1);
  display.printf("%s",menuenabled[val]);
  updatedisplay();
}

void drawbottommenu(uint16_t seq,uint16_t stage) {
  display.setFont(); // use default 5x7 font which fits better
  showoffsetvalue(pitch_offset[seq][stage]);
  showpulses(pulses[seq][stage]);
  showgatemode(gatemode[seq][stage]);
  showenabled(enabled[seq][stage]);
  display.setFont(&Monospaced_bold_12); // 
  updatedisplay();
}

void drawbottomunderscore(uint16_t index) {
  int16_t x=bottommenu_x[index];
  int16_t y=SCREEN_HEIGHT-1;
  int16_t width=bottommenu_width[index];
  display.drawLine(x,y,x+width,y,WHITE);
  updatedisplay();
}

void undrawbottomunderscore(uint16_t index) {
  int16_t x=bottommenu_x[index];
  int16_t y=SCREEN_HEIGHT-1;
  int16_t width=bottommenu_width[index];
  display.drawLine(x,y,x+width,y,BLACK);
  updatedisplay();
}

void flashbottomunderscore(uint16_t index) {
 static uint32_t flashtimer=0;
 static bool state;
 if ((millis()-flashtimer) > FLASHINTERVAL) {
  if (state) {
    undrawbottomunderscore(index);
  }
  else {
    drawbottomunderscore(index);
  }
  state=!state;
  flashtimer=millis();
 }
}

void flashunderscore(uint16_t index) {
 static uint32_t flashtimer=0;
 static bool state;
 if ((millis()-flashtimer) > FLASHINTERVAL) {
  if (state) {
    undrawunderscore(index);
  }
  else {
    drawunderscore(index);
  }
  state=!state;
  flashtimer=millis();
 }
}
