// sequencer stuff

#define NUM_SEQUENCERS 2
#define NUM_STAGES 8
int16_t pitch_offset[NUM_SEQUENCERS][NUM_STAGES]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
int16_t accumulator[NUM_SEQUENCERS][NUM_STAGES]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
int16_t acc_max[NUM_SEQUENCERS]={12,12};  // accumulator limit upper
int16_t acc_min[NUM_SEQUENCERS]={-12,-12}; // accumulator limit upper
int16_t enabled[NUM_SEQUENCERS][NUM_STAGES]={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
int16_t pulses[NUM_SEQUENCERS][NUM_STAGES]={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
//int16_t enabled[NUM_SEQUENCERS][NUM_STAGES]={1,1,0,0,0,0,0,0,1,1,1,1,1,1,1,1};
//int16_t pulses[NUM_SEQUENCERS][NUM_STAGES]={4,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
int16_t gatemode[NUM_SEQUENCERS][NUM_STAGES]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
int16_t cvslew[NUM_SEQUENCERS]={0,0}; // menu value of CV slew rate

struct sequencer {
  int16_t index;    // index of stage we will be on next clock
  int16_t current;    // index of stage we are actually on
  int16_t direction;    // fwd, backward movement state
  int16_t stagemode;    // stage step mode  
  int16_t stages;   // number of stages
  int16_t stagecounter;   // stage counter
  int16_t pulsecounter;  // counts off pulses per stage
  int16_t totalpulses;   // total number of pulses
  int16_t maxpulses;    // maximum number of pulses
  int16_t clockdivider;   // clock rate divider - lookup via table
  int16_t clockcounter;   //  clock counter
  bool clocked;   // flag indicating stage was just clocked
  int16_t root;   // "root" note - note offsets are relative to this. 
  int16_t scale;  // quantizer scale
  int16_t CVmode;  // CV output mode
  int16_t gatelength; // gate length as a % of clock
} seq[NUM_SEQUENCERS] = {
  -1,    // index of stage we will be on next clock
  0,    // index of stage we are actually on  
  FWD,   //  fwd, backward movement state
  FORWARD,    // step mode 
  8,   // number of stages
  8,   // stage counter
  1,   //  pulse counter
  0,  // total number of pulses
  0,    // maximum number of pulses 0= use stage counter
  1,   // clock divider 
  1,   //  clock counter
  0, // clocked flag
  24,   // "root" note - note offsets are relative to this. 
  4,  // quantizer scale
  PITCH,  // CV output mode
  50,   // gate length as a % of clock

  -1,    // index of stage we will be on next clock
  0,    // index of stage we are actually on  
  FWD,   // fwd, backward movement state
  FORWARD,   // step mode
  8,   // number of stages
  8,   // stage counter
  1,   // pulse counter
  0,  // total number of pulses
  0,    // maximum number of pulses 0= use stage counter
  1,   // clock divider 
  1,   //  clock counter
  0, // clocked flag
  24,   // "root" note - note offsets are relative to this.
  4,  // quantizer scale
  PITCH,  // CV output mode
  50,   // gate length as a % of clock
};