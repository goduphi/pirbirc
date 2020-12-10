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

#ifndef IR_EMITTER_H_
#define IR_EMITTER_H_

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

#define IR_LED_MASK 64

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void initIrEmitter(void);
void irModulationIsr(void);
void playCommand(uint8_t address, uint8_t data);

#endif /* IR_EMITTER_H_ */
