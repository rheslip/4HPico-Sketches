**Picopolis - Dual Metropolis Inspired Sequencer for the 4HPico**

By Rich Heslip 

Metropolis is a 34HP module covered with buttons, switches and knobs. **TWO** Metropolis style sequencers in 4HP inevitably requires tradeoffs since Picopolis only has one encoder with a switch and a small OLED display. Its menu divey but not that bad once you understand the UI. It doesn't have every feature of the Metropolis and no doubt behaves differently in some cases.

first release Oct 2026

known issues:

ratcheting gets a bit ragged at high clock and ratchet rates

disabling stages sometimes causes wierd sequences - maybe thats a feature?

**Concept**

Picopolis is based on the Intelligel Metropolis which is itself based on the RK-185 sequencer. Picopolis implements some new features for generative sequencing. It has TWO 8 stage sequencers vs one sequencer in the Metropolis and the sequencers can modulate each other in interesting ways.
For a better understanding of the Metropolis style sequencer you should watch some Youtube videos which explain how stages, clocks, gate modes etc work. This user guide assumes a good understanding of Metropolis.


**Patching**

Top left jack - clock input

Top right jack - reset input

Middle left jack - Sequencer A Gate output

Middle right jack - Sequencer B Gate output

Bottom left jack - Sequencer A CV output

Bottom right jack - Sequencer B CV output

**The 4HPico module must be configured for CV output on the middle jacks** see the hardware docs for how to do this

**Main Screen**

On startup you will see the main screen which has two rows of circles and a bottom row with numbers and letters. The top row of circles represent the 8 stages of sequencer A, the second row of circles represent the 8 stages of sequencer B, and the bottom row controls pitch offset, clocks, gate mode and stage mode (L-R) respectively.


**Navigation**

Rotate the encoder to select a sequencer stage - the underscore indicates which stage is selected. Click to select a stage and a blinking underscore will appear under the last used item on the bottom menu. Long press and rotate the encoder to move between bottom menu items. Rotate the encoder to change the item's value. 
Click to save the value and return to stage selection.

To access the secondary menu, long press the encoder from the stage selection state. Once in the secondary menu, rotate the encoder to scroll through the menu items. Click the encoder to select an item - an underline will appear under the item value. Rotate the encoder to change the item's value and click to save the value. Long press the encoder to exit the secondary menu and return to the main screen.

**Bottom Menu** 

Left - Stage pitch offset value, range +-24 semitones. CV output is the pitch offset added to the root note set in the secondary menu.

2nd from Left - stage clocks, range 1-8. Stage will move to the next stage after this many clocks. 8 stages * 8 clocks = maximum of 64 clocks per sequence

2nd from Right - Gate mode

	1P - one gate will be output on the first clock regardless of what the stage clocks setting is
	
	NP - one gate will be output for every clock of this stage
	
	Hi - gate will be high for this stage ie a tie to the previous stage
	
	Lo - gate will be low for this stage ie this stage is a rest
	
	R2 - Two gates will be output for every clock of this stage ie ratchet x2
	
	R3 - Three gates will be output for every clock of this stage ie ratchet x3
	
	R4 - Four gates will be output for every clock of this stage ie ratchet x4
	
Right - Stage enable, disable, slide. "O" means the stage is enabled, "X" means it is disabled, and "/" means this stage will slide to pitch


**Secondary Menu**

The secondary menus are the same for sequencer A and B.

Length - sets the number of stages in the sequencer

ClkDiv - clock divider for this sequencer

Gate % - gate length as a % of the clock period

Mode - Sequencer advance modes - Forward, Reverse, PingPong, Brownian and Random

ClkLim - sets the maximum number of clocks in the sequence regardless of stage length and stage clock settings. 
		e.g. If you want the sequencer to run in sync with something else it will restart the sequence after this many clocks.

Root - sets the root note to which the stage's pitch offset is added.

Scale - sets the scale for the quantizer

CVMode

	Pitch - CV out = quantize (Root + stage pitch offset)
	
	Add  - CV out = quantize (Root + stage pitch offset + stage pitch offset of other sequencer). 
		 for example set sequencer B clock divide to 16 and its pitch offsets will transpose sequencer A every 16 clocks.
	
	Subtract -  CV out = quantize (Root + stage pitch offset - stage pitch offset of other sequencer).
	
	Add Acc - CV out = quantize (Root + stage pitch offset + accumulator). 
		Both sequencers have an accumulator for each stage. 
		The pitch offset of the other sequencer is added to the accumulator on each step of the other sequencer.
	
	Slew - sets the slew (portamento) rate  for slewed stages. 0=fast, 10=slow
	
	Acc Max - range 1 to 24 - sets the maximum value of the accumulators for this sequencer. 
		Use when you want to constrain the range of notes in Add Acc mode.
	
	Acc Min - range -24 to 0 - sets the minimum value of the accumulators for this sequencer. 
		Use when you want to constrain the range of notes in Add Acc mode.
	
	