# tiny-servolaser
Control 2 independent servomotors with high resolution, and also PWM drive a laser diode while you're at it, all with an ATTINY85. Oh, and make the servos move the laser around, that would be cool!

Over 150 servo steps from Timer1! (copied from code)
/*
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
 *  Solution: Make the lowest OCR value > 1. Then Timer1 PWM always has time to drive pin low before it cuts off. 
 */

This principle would work equally as well to make a high-resolution DAC of any sort. The principle is to 
trade frequency for resolution.

Don't create this project with as powerful a laser if you don't have the safety requirements for handling powerful lasers. I warned you! Use an LED or something!


