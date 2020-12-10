/*
 * This header file contains all the function definitions from Lab 6
 */

#ifndef NEC_IR_DECODER_H_
#define NEC_IR_DECODER_H_

// Bit-banded address
#define IR_INPUT                (*((volatile uint32_t *)(0x42000000 + (0x400053FC - 0x40000000)*32 + 0*4)))
#define GPO_LOGIC_ANALYZER      (*((volatile uint32_t *)(0x42000000 + (0x400053FC - 0x40000000)*32 + 2*4)))

// PortF masks
#define GPI_IR_SENSOR_MASK 1
#define GPO_LOGIC_ANALYZER_MASK 4

#define VALID_COMMAND           0x80000000

void initIrDecoder(void);
void edgeDetectIsr(void);
void irSignalSamplingIsr(void);
uint8_t getIrData(void);

#endif /* NEC_IR_DECODER_H_ */
