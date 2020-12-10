// Infrared Emitter Library
// Sarker Nadir Afridi Azmi
// Contains functions that help in emitting a NEC transmission protocol IR signal

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL
// Target uC:       TM4C123GH6PM
// System Clock:    40MHz

// Hardware configuration:
// PWM module:
// PMW Module 0, Generator 0, Output 0 (M0PWM0) (PB6)
// Timer module:
// Timer 2, Periodic mode, Interrupt enabled

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "ir_emitter.h"
#include "speaker.h"
#include "wait.h"

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

uint8_t globalAddress = 0;
uint8_t globalData = 0;

bool emittingIr = true;
extern bool decoding;

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Initializes the PWM module and the Timer to modulate the PWM signal
void initIrEmitter(void)
{
    // Initialize PWM to modulate a 38.2kHz signal
    // Provide a clock to PWM module 0 and Timer 2
    SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R2;
    SYSCTL_RCGCPWM_R |= SYSCTL_RCGCPWM_R0;
    // Enable Port B as PB6 will be used to output the PWM signal
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R1;
    _delay_cycles(3);

    GPIO_PORTB_AFSEL_R |= IR_LED_MASK | SPEAKER_MASK;
    GPIO_PORTB_PCTL_R &= ~(GPIO_PCTL_PB6_M0PWM0 | GPIO_PCTL_PB7_M0PWM1);
    GPIO_PORTB_PCTL_R |= GPIO_PCTL_PB6_M0PWM0 | GPIO_PCTL_PB7_M0PWM1;
    GPIO_PORTB_DR2R_R |= IR_LED_MASK | SPEAKER_MASK;
    GPIO_PORTB_DIR_R  |= IR_LED_MASK | SPEAKER_MASK;
    GPIO_PORTB_DEN_R &= ~IR_LED_MASK;
    GPIO_PORTB_DEN_R |= SPEAKER_MASK;

    // reset PWM0 module
    SYSCTL_SRPWM_R = SYSCTL_SRPWM_R0;
    SYSCTL_SRPWM_R = 0;
    // Turn off the PWM module 0 gen 0
    PWM0_0_CTL_R = 0;
    PWM0_0_GENA_R |= PWM_0_GENA_ACTCMPAD_ZERO | PWM_0_GENA_ACTLOAD_ONE;
    // Speaker module
    PWM0_0_GENB_R = PWM_0_GENB_ACTCMPBD_ZERO | PWM_0_GENB_ACTLOAD_ONE;

    // 38.2kHz
    // (40e6/2) * (1/Load) = frequency
    PWM0_0_LOAD_R = 1047;
    PWM0_INVERT_R |= PWM_INVERT_PWM0INV;

    // Set to 50% duty cycle
    PWM0_0_CMPA_R = 523;

    PWM0_INVERT_R = PWM_INVERT_PWM1INV;
    PWM0_0_CMPB_R = 25000;

    PWM0_0_CTL_R = PWM_0_CTL_ENABLE;
    PWM0_ENABLE_R = PWM_ENABLE_PWM0EN | PWM_ENABLE_PWM1EN;

    // Configure Timer 2 to modulate NEC signal
    TIMER2_CTL_R &= ~TIMER_CTL_TAEN;                 // turn-off timer before reconfiguring
    TIMER2_CFG_R = TIMER_CFG_32_BIT_TIMER;           // configure as 32-bit timer (A+B)
    TIMER2_TAMR_R = TIMER_TAMR_TAMR_PERIOD;          // configure for periodic mode (count down)
    TIMER2_TAILR_R = 0;                              // set load value to 9ms for 1 Hz interrupt rate
    TIMER2_IMR_R &= ~TIMER_IMR_TATOIM;
    NVIC_EN0_R |= 1 << (INT_TIMER2A-16);             // turn-on interrupt 39 (TIMER2A)
}

// Modulates the IR signal
void irModulationIsr(void)
{
    static uint32_t modulationTimes[] = {360000, 180000, 22720};
    static uint8_t modulationIndex = 0;
    static uint8_t workingData = 0;
    static uint8_t bitIndex = 0;
    static uint8_t bitMask = 128;
    static uint8_t bit = 0;
    static uint8_t bitCount = 0;
    static bool whatIsTheBit = true;

    // clear interrupt flag
    TIMER2_ICR_R = TIMER_ICR_TATOCINT;

    if(modulationIndex >= 100)
    {
        GPIO_PORTB_DEN_R &= ~IR_LED_MASK;
        TIMER2_IMR_R &= ~TIMER_IMR_TATOIM;
        TIMER2_CTL_R &= ~TIMER_CTL_TAEN;

        // Reset all of the irModulationIsr variables
        modulationIndex = 0;
        whatIsTheBit = true;
        bit = 0;
        bitCount = 0;
        bitIndex = 0;
        emittingIr = false;
        return;
    }
    if(modulationIndex == 0)
        GPIO_PORTB_DEN_R |= IR_LED_MASK;
    else if(modulationIndex == 1)
    {
        GPIO_PORTB_DEN_R &= ~IR_LED_MASK;
    }
    // This is the range in which the address and data exists
    else if(modulationIndex >= 2 && modulationIndex <= 99)
    {
        if(modulationIndex == 2)
            workingData = globalAddress;
        else if(modulationIndex == 50)
            workingData = globalData;
        else if(modulationIndex == 98)
            workingData = 0;

        if(whatIsTheBit)
        {
            whatIsTheBit = false;
            if(workingData & (bitMask >> bitIndex))
                bit = 1;
            else
                bit = 0;
        }
        if(bit == 0)
        {
            if((bitCount % 2) == 0)
                GPIO_PORTB_DEN_R |= IR_LED_MASK;
            else
                GPIO_PORTB_DEN_R &= ~IR_LED_MASK;
            bitCount++;
        }
        else if(bit == 1)
        {
            if((bitCount % 4) == 0)
                GPIO_PORTB_DEN_R |= IR_LED_MASK;
            else
                GPIO_PORTB_DEN_R &= ~IR_LED_MASK;
            bitCount++;
        }
        if((bit == 0 && bitCount == 2) || (bit == 1 && bitCount == 4))
        {
            bitCount = 0;
            whatIsTheBit = true;
            bitIndex++;
        }
        if(bitIndex == 8)
        {
            bitIndex = 0;
            workingData = ~workingData;
            whatIsTheBit = true;
        }
    }
    if(modulationIndex <= 2)
    {
        TIMER2_CTL_R &= ~TIMER_CTL_TAEN;
        TIMER2_TAV_R = 0;
        TIMER2_TAILR_R = modulationTimes[modulationIndex];
        TIMER2_CTL_R |= TIMER_CTL_TAEN;
    }
    modulationIndex++;
}

// Non-blocking function that helps in modulating the specified address and data
// through the IR LED
void playCommand(uint8_t address, uint8_t data)
{
    // Get a copy of the data
    globalAddress = address;
    globalData = data;
    // Turn on Timer 2
    TIMER2_IMR_R = TIMER_IMR_TATOIM;
    TIMER2_CTL_R |= TIMER_CTL_TAEN;
    while(emittingIr);
    while(decoding);
    decoding = true;
    emittingIr = true;
}
