/*
 * Speaker is connected to Port B pin 7
 */

#include <stdint.h>
#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "speaker.h"

// Initialize Hardware
void initSpeaker(void)
{
    // Enable clocks
    SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R0;
    _delay_cycles(3);

    // Configure Timer 1 as the time base
    TIMER0_CTL_R &= ~TIMER_CTL_TAEN;                 // turn-off timer before reconfiguring
    TIMER0_CFG_R = TIMER_CFG_32_BIT_TIMER;           // configure as 32-bit timer (A+B)
    TIMER0_TAMR_R = TIMER_TAMR_TAMR_PERIOD;          // configure for periodic mode (count down)
    TIMER0_TAILR_R = 0;
    TIMER0_IMR_R = TIMER_IMR_TATOIM;                 // turn-on interrupts
    NVIC_EN0_R |= 1 << (INT_TIMER0A-16);             // turn-on interrupt 37 (TIMER1A)
}

void speakerIsr(void)
{
    setFrequency(600);
    waitMicrosecond(WAIT_TIME);
    setFrequency(800);
    waitMicrosecond(WAIT_TIME);
    setFrequency(0);
    TIMER0_ICR_R = TIMER_ICR_TATOCINT;
    TIMER0_CTL_R &= ~TIMER_CTL_TAEN;
}

void setFrequency(uint32_t frequency)
{
    // Turn off PWM0 Generator 1
    PWM0_0_CTL_R = 0;
    PWM0_0_LOAD_R = 20000000 / frequency;
    // Invert outputs so duty cycle increases with increasing compare values
    PWM0_INVERT_R = PWM_INVERT_PWM1INV;
    PWM0_0_CMPB_R = ((20000000 / frequency) >> 1);
    // turn-on PWM0 generator 0
    PWM0_0_CTL_R = PWM_0_CTL_ENABLE;
}
