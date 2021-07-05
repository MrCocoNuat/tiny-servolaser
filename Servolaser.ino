//8MHz operation!

//Most hobby servomotors want a PWM control signal
//The period should be 20ms -> 50Hz, and the PW from 1ms to 2ms

const uint8_t highOCR = 138; //this value of OCR gives -90 degrees rotation (nominally 1ms)
const uint8_t lowOCR = 3; //this gives a 90 degrees rotation (nominally 2ms)
//these values are INVERTED, i.e. higher OCR values give shorter pulses

const uint8_t stabilityThreshold = 96; //how many consecutive identical readings of the directional pad ADC are needed

//how to squeeze out more bits of DAC resolution
/* Standard method:
 *  Problem: Drive 2 independent servomotors off of Timer1
 *  
 *  Solution: Use Compare matches A and B for 2 motors.
 *  PWM mode 2, or set on 0x00 clear on compare match. This makes the valid OCR values 15-32 or so.
 *  
 *  Problem: There are very very few servo steps possible, less than 20! This guarantees rough movement.
 *  
 *  Solution: Instead, pump the frequency up 8 times, and the OCR values 8 times. Then, 7 out of 8 cycles simply
 *  set the PWM output pins to input so they stay low. Do this setting by the Timer1 Overflow interrupt.
 *  Now there are almost 160 servo steps.
 *  
 *  Problem: this causes a glitch in the cycle immediately after the 1 out of 8, where once the new cycle
 *  starts, PWM mode drives it high microseconds before the interrupt disables the output, making a 
 *  unwanted glitch high.
 *  
 *  Solution: Set PWM mode to 3 instead, so clear on 0x00 and set on compare match. Then invert the OCR values
 *  x = (255 - x) so that at the start of a cycle the pin is driven low, then sometime in the middle it is high
 *  for the rest of the cycle. So, the glitch won't happen because at the beginning of the next cycle Timer will
 *  drive pin low anyway.
 *  
 *  Problem: Timer1 PWM no longer drives pin high right before it is set to input for most OCR values. But,
 *  when the OCR is 0, the next cycle's Timer still drives the pin high right before output disabled, leading to
 *  a input-high situation (voltage takes time to fall off without external pulldown).
 *  
 *  Solution: Make the lowest OCR value 1. Then Timer1 PWM always has time to drive pin low before it cuts off. 
 */

void setup() {
  cli(); 
  //globally disable interrupts during setup

  TCCR0A = (2 << COM0A0) | (3 << WGM00);
  //enable Timer0 Fast PWMA in set on 00, clear on OCR0A mode. 
  TCCR0B = (2 << CS00);
  //set prescaler to /8 for 1MHz timer ticks and 4kHz timer overflows. Practically flicker-free laser
  OCR0A = 0;
  //initial setting for OCR0A. This gets updated pretty much immediately.
  //A setting of 0 does not quite turn this pin off completely, but it is close enough.
  DDRB |= (1 << DDB0);
  //set pin0 to output so PWM can drive it.
    
  TIMSK =  (1 << TOIE1);
  //enable Timer1 Overflow Interrupt to select which timer cycle should give PWM output
  TCCR1 = (1 << PWM1A) | (3 << COM1A0) | (7 << CS10); 
  //enable Timer1 PWMA in clear on 00, set on OCR1A mode. Set prescaler to /64 
  GTCCR = (1 << PWM1B) | (3 << COM1B0);
  //enable Timer1 PWMB in clear on 00, set on OCR1B mode
  OCR1C = 255; 
  //a /64 prescaler makes Timer1 tick at 128kHz
  //if OCR1C = 255 then matches happen at 512Hz
  //only 1 out of 8 of these matches triggers an output pulse, pulses happen at 64Hz
  //that is close enough for a servomotor to operate
  OCR1A = 63;
  OCR1B = 75;
  //two PWM signals for both X and Y axis servomotors
  //the initial values are selected so that the servomotors point straight forward

  ADMUX = (1 << ADLAR) | (1 << MUX0);
  //Left-adjust result (maximum 8bit resolution needed anyway), and set ADC input to pin2
  ADCSRA = (1 << ADEN) | (4 << ADPS0);
  //Enable ADC, and set prescaler to /16 for 500kHz operation
  //The frequency is technically out of spec but since not all 10 bits of resolution are needed, it is OK.

  DIDR0 = (63 << AIN0D);
  //Disable all digital input buffers. Saves some power.
  PRR = (1 << PRUSI);
  //Disable the Universal Serial Interface. Saves some power.
  
  sei(); 
  //globally enable interrupts
}

void loop() {
  ADMUX &= ~(1 << MUX1); //change ADC input from pin3 to pin2, read laser brightness potentiometer
  ADCSRA |= (1 << ADSC); //start conversion
  while (ADCSRA & (1 << ADSC)){} //wait for conversion to finish
  OCR0A = ADCH; //set the laser brightness to the potentiometer setting

  ADMUX |= (1 << MUX1); //change ADC input from pin2 to pin3 to read directional pad DAC
  uint8_t unstable = stabilityThreshold; //Once 32 consecutive readings return identical values, the reading is considered stable
  uint8_t reading = 0; //these keep track of what the program thinks the ADC reading really is
  uint8_t oldReading = reading;
  while (unstable){
    ADCSRA |= (1 << ADSC); //start conversion
    while (ADCSRA & (1 << ADSC)){} //wait for conversion to finish
    reading = ADCH >> 3;  //take only top 5 bits, natural noise rejection
    unstable--; //count down unstable
    if (reading - oldReading > 1 || reading - oldReading < -1){ //too unstable, signal is still changing 
      unstable = stabilityThreshold; //reading changed! not stable yet
      oldReading = reading; //update
    }
    for(volatile uint8_t i = 1; i++;) {} //delay 255 clock cycles, about 16us
  }
  
  uint8_t newA = OCR1A; //buffer changes in OCR1A/B
  uint8_t newB = OCR1B;
  switch(reading){
  /*on a 3.85V power supply, the actual voltage readings for the directional pad DAC are:
   *   Direction    Voltage (V)   ADCH/8
   * no direction:     0.00        0.00
   * up:               0.44        3.65
   * up+right:         1.33       11.05                                                        
   * right:            0.85        7.06
   * right+down:       2.53       21.03
   * down:             1.66       13.80
   * down+left:        1.92       15.96
   * left:             0.22        1.82
   * left+up:          0.70        5.82
   * These voltages are measured using a scope with 1M input impedance, unloaded they will be slightly higher
   * To correct this in the final product simply insert an external 1M resistor as a dummy load
   * 
   * These readings can all be differentiated from each other by the 5 bit ADC
   * the missing ADC values are impossible combinations like left+right or up+right+down;
   * complementary buttons on a dpad can't be pressed simultaneously (ask the SMB1 TAS people)
   * 
   * To actually move whatever the servomotors point at up, right, down, left, change these:
   *                                                    A+    B+    A-    B-
   *                  These are of course dependent on the physical wiring and construction
   */
  case 1: case 2: //left
    newB--; break;
  case 3: case 4: case 5: //up
    newA++; break;
  case 6: //left+up
    newB--; newA++; break;
  case 7: case 8: //right
    newB++; break;
  case 10: case 11: case 12: //up+right
    newA++; newB++; break;
  case 13: case 14: //down
    newA--; break;
  case 15: case 16: case 17: //down+left
    newA--; newB--; break;
  case 20: case 21: case 22: //right+down
    newB++; newA--; break;
  }

  OCR1A = (lowOCR <= newA && newA <= highOCR)? newA : OCR1A; //constrain OCR1A/B
  OCR1B = (lowOCR <= newB && newB <= highOCR)? newB : OCR1B;
}



ISR(TIMER1_OVF_vect){
  static uint8_t cycleCounter = 7; //something different happens 1 out of every 8 cycles:
  DDRB &= ~(1 << DDB4 | 1 << DDB1); //normally set pins1 and 4 to input
  if (!(--cycleCounter)){ //counting down, on 1 out of 8,
    DDRB |= (1 << DDB4) | (1 << DDB1); //set them to output
    cycleCounter = 7; //reset counter
  }
}
