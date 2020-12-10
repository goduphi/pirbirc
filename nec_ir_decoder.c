// Infrared Signal Decoder Library
// Sarker Nadir Afridi Azmi
// Contains functions that help in emit a NEC transmission protocol IR signal

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
#include "uart0.h"
#include "nec_ir_decoder.h"
#include "eeprom.h"
#include "speaker.h"
#include "wait.h"
#include "common_terminal_interface.h"

#define COMMAND_META_DATA   0

#define BLUE_LED_MASK       4

uint8_t sampleVals[] = {0, 0, 0, 1, 1,
                        0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
                        0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1};

uint32_t sampleWaitTimes[4] = {154000, 60000, 71250, 22720};

uint8_t index = 0;
bool isInvalidBit = false;

int8_t zeroOneCounter = 0;
uint8_t dataBitCounter = 0;
uint8_t dataByte = 0;
uint8_t addressByte = 0;
uint8_t complementAddressByte = 0;
uint8_t complementDataByte = 0;
bool complement = false;
uint8_t isEndBit = 1;

extern bool learning;
extern char commandName[11];
extern uint8_t ruleCounter;

uint16_t eepromAddressOffset = 1;
uint8_t commandsInsideOfEeprom = 0;

bool decoding = true;

// Initialize Hardware
void initIrDecoder(void)
{
    // Enable clocks for the timer
    SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R1;
    // Enable clocks for Port B
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R1 | SYSCTL_RCGCGPIO_R5;
    _delay_cycles(3);

    GPIO_PORTF_DIR_R |= 4 | 8 | 2;
    GPIO_PORTF_DR2R_R |= 4 | 8 | 2;
    GPIO_PORTF_DEN_R |= 4 | 8 | 2;

    // Make PB0 an input and PB2 an output
    GPIO_PORTB_DIR_R |= GPO_LOGIC_ANALYZER_MASK;
    GPIO_PORTB_DIR_R &= ~GPI_IR_SENSOR_MASK;
    GPIO_PORTB_DR2R_R |= GPO_LOGIC_ANALYZER_MASK;
    GPIO_PORTB_DEN_R |= GPI_IR_SENSOR_MASK | GPO_LOGIC_ANALYZER_MASK;

    GPIO_PORTB_DATA_R &= ~GPO_LOGIC_ANALYZER_MASK;

    // Configure the interrupt for the GPIO pin
    GPIO_PORTB_IM_R &= ~GPI_IR_SENSOR_MASK;            // Turn off interrupts for PB0
    GPIO_PORTB_IS_R &= ~GPI_IR_SENSOR_MASK;            // Configure the pin to detect edges
    GPIO_PORTB_IBE_R &= ~GPI_IR_SENSOR_MASK;           // Let the Interrupt Event register interrupt generation
    GPIO_PORTB_ICR_R |= GPI_IR_SENSOR_MASK;            // Clear the interrupt as per the documentation
    GPIO_PORTB_IEV_R &= ~GPI_IR_SENSOR_MASK;           // Detect falling edges
    NVIC_EN0_R |= (1 << (INT_GPIOB - 16));              // Enable the master interrupt control

    // Configure Timer 1 as the time base
    TIMER1_CTL_R &= ~TIMER_CTL_TAEN;                    // turn-off timer before reconfiguring
    TIMER1_CFG_R = TIMER_CFG_32_BIT_TIMER;              // configure as 32-bit timer (A+B)
    TIMER1_TAMR_R = TIMER_TAMR_TAMR_PERIOD;             // configure for periodic mode (count down)
    NVIC_EN0_R |= (1 << (INT_TIMER1A - 16));            // Enable the master interrupt control

    // GPIO_PORTB_IM_R |= GPI_IR_SENSOR_MASK;             // Enable the interrupt
}

void edgeDetectIsr(void)
{
    GPIO_PORTB_ICR_R |= GPI_IR_SENSOR_MASK;
    GPIO_PORTB_IM_R &= ~GPI_IR_SENSOR_MASK;
    TIMER1_TAILR_R = 90000;
    TIMER1_IMR_R |= TIMER_IMR_TATOIM;
    TIMER1_CTL_R |= TIMER_CTL_TAEN;

    // Reset everything for the next button input
    index = 0;
    isInvalidBit = false;

    zeroOneCounter = 0;
    dataBitCounter = 0;
    dataByte = 0;
    complement = false;
    complementDataByte = 0;

    isEndBit = 1;
}

// This function takes an unsigned 8 bit number, and prints it to a terminal
// This is meant for debugging
void printUB10Number(uint8_t x)
{
    int lastDigit = x % 10;
    x /= 10;
    int secondDigit = x % 10;
    x /= 10;
    int firstDigit = x % 10;
    putcUart0('0' + firstDigit);
    putcUart0('0' + secondDigit);
    putcUart0('0' + lastDigit);
}

void fetchDevAction(char* dev, char* action, uint16_t* offset)
{
    uint8_t i = 0, j = 0;
    uint8_t nameAddressOffset = 0;
    uint32_t name = 0;
    while(nameAddressOffset < 2)
    {
        name = readEeprom((*offset)++);
        for(j = 0; j < 4; j++)
        {
            char c = ((name >> (j << 3)) & 0xFF);
            dev[i++] = c;
        }
        nameAddressOffset++;
    }
    name = readEeprom((*offset)++);
    i = 0;
    for(j = 0; j < 4; j++)
    {
        char c = ((name >> (j << 3)) & 0xFF);
        action[i++] = c;
    }
}

void irSignalSamplingIsr(void)
{
    if((index == 103 && !isEndBit) || isInvalidBit)
    {
#ifdef DEBUG
        putcUart0('\n');
        putsUart0("Data: ");
        printUB10Number(dataByte);
        putcUart0(' ');
        putsUart0("Address: ");
        printUB10Number(addressByte);
        putcUart0(' ');
        printUB10Number(complementAddressByte);
        putcUart0('\n');
        // Do stuff with the button code
        if(!(dataByte & complementDataByte))
        {
            if(addressByte == 0)
            {
                switch(dataByte)
                {
                case 48:
                    GPIO_PORTF_DATA_R ^= 4;
                    break;
                case 24:
                    GPIO_PORTF_DATA_R ^= 2;
                    break;
                case 122:
                    GPIO_PORTF_DATA_R ^= 8;
                    break;
                }
            }
        }
#endif
        // Alert good
        uint32_t alertFlag = readEeprom(COMMAND_META_DATA);

        if(!(dataByte & complementDataByte) && !isInvalidBit)
        {
            uint32_t dummy = readEeprom(0);
            putcUart0('\n');
            printUB10Number((dummy >> 16) & 0xFF);
            putcUart0(' ');
            printUB10Number((dummy & 0xFF));
            putcUart0('\n');

            putsUart0("\nAddress: ");
            printUB10Number(addressByte);
            putcUart0(' ');
            putsUart0("Data: ");
            printUB10Number(dataByte);
            putcUart0('\n');

            // Rule command section - Start
            char dev[8];
            char action[4];
            uint8_t rule = 0;
            uint16_t ruleAddressOffset = RULE_ADDRESS_START;
            uint32_t ruleDataAddress = 0;
            while(rule < ruleCounter)
            {
                fetchDevAction(dev, action, &ruleAddressOffset);
                ruleDataAddress = readEeprom(ruleAddressOffset);

                // Define all the bindings here
                if(((ruleDataAddress >> 8) & 0xFF) == addressByte && (ruleDataAddress & 0xFF) == dataByte)
                {
                    if(stringCompare(dev, "bled") && stringCompare(action, "on"))
                    {
                        GPIO_PORTF_DATA_R |= BLUE_LED_MASK;
                    }
                }

                if(((ruleDataAddress >> 8) & 0xFF) == addressByte && (ruleDataAddress & 0xFF) == dataByte)
                {
                    if(stringCompare(dev, "bled") && stringCompare(action, "off"))
                    {
                        GPIO_PORTF_DATA_R &= ~BLUE_LED_MASK;
                    }
                }
                rule++;
                ruleAddressOffset++;
            }
            // Rule command section - End

            if(alertFlag & 0x80000000)
            {
                TIMER0_CTL_R |= TIMER_CTL_TAEN;
            }
            if(learning)
            {
                // Process the command name into 32 bits
                uint32_t name = 0;
                uint8_t i = 0;
                uint8_t shiftValue = 0;
                // This variable is probably unnecessary
                uint32_t currentChar;
                // The maximum allowed characters are 11
                // So, write 11 characters maximum to the name buffer
                // Replace the last character with a null terminator
                for(i = 1; i <= 12; i++)
                {
                    if(i <= 11 && commandName[i - 1] != '\0')
                    {
                        currentChar = commandName[i - 1];
                    }
                    else
                    {
                        currentChar = '\0';
                    }

                    name |= (currentChar << (shiftValue++ << 3));

                    if((i % 4) == 0)
                    {
                        writeEeprom(eepromAddressOffset++, name);
                        name = 0;
                        shiftValue = 0;;
                    }
                }
                uint32_t dataAddress = addressByte;
                dataAddress = ((dataAddress << 8) | dataByte);
                dataAddress |= VALID_COMMAND;
                writeEeprom(eepromAddressOffset++, dataAddress);
                commandsInsideOfEeprom++;
                dataAddress = 0;

                // Store meta data about the commands
                // Let's reuse this variable
                // Lower 16 bits contains where we should start saving new commands
                // The next 8 bits contains the number of commands inside of the eeporm
                // 00000000 <- 8 bits for number of commands -> <- 16 bits for address offset ->
                // 0x00FF0000 - for clearing the number of commands
                // 0x0000FFFF - for clearing the adress offset
                uint32_t tempForClearingCommands = 0;
                dataAddress = readEeprom(COMMAND_META_DATA);
                dataAddress &= ~0x00FFFFFF;
                tempForClearingCommands |= commandsInsideOfEeprom;
                tempForClearingCommands <<= 16;
                dataAddress |= tempForClearingCommands;
                tempForClearingCommands = 0;
                tempForClearingCommands |= eepromAddressOffset;
                dataAddress |= eepromAddressOffset;

                writeEeprom(COMMAND_META_DATA, dataAddress);

                learning = false;
            }
        }
        else if((dataByte & complementDataByte) && isInvalidBit)
        {
            if(alertFlag & 0x40000000)
            {
                setFrequency(600);
                waitMicrosecond(WAIT_TIME);
                setFrequency(400);
                waitMicrosecond(WAIT_TIME);
                setFrequency(0);
            }
        }
        // clear interrupt flag
        TIMER1_ICR_R = TIMER_ICR_TATOCINT;
        // Turn off Timer 1
        TIMER1_CTL_R &= ~TIMER_CTL_TAEN;
        // Turn off interrupts
        TIMER1_IMR_R &= ~TIMER_IMR_TATOIM;
        // Turn on GPI interrupt
        GPIO_PORTB_IM_R |= GPI_IR_SENSOR_MASK;
        decoding = false;
        return;
    }

#ifdef DEBUG
    putcUart0('0' + IR_INPUT);
    putcUart0(' ');
    GPO_LOGIC_ANALYZER ^= 1;
#endif
    if(index <= 4 && IR_INPUT^sampleVals[index])
    {
        isInvalidBit = true;
    }

    if(index >= 2 && index <= 5)
    {
        TIMER1_CTL_R &= ~TIMER_CTL_TAEN;
        TIMER1_TAV_R = 0;
        TIMER1_TAILR_R = sampleWaitTimes[index - 2];
        TIMER1_CTL_R |= TIMER_CTL_TAEN;
    }

    // Sample the address and data
    // Address range:   5 - 52
    // Data rage:       53 - 100
    if(index >= 5 && index <= 102)
    {
        if(!(IR_INPUT | 0))
            zeroOneCounter++;
        else if(IR_INPUT & 1)
            zeroOneCounter--;

        // Every 8 bits, change to getting the bits for the complement address/data
        if((zeroOneCounter == 0) && (dataBitCounter == 8 || dataBitCounter == 24))
        {
            complement = true;
        }

        if(zeroOneCounter == 0)
        {
            if(index > 100)
                isEndBit = 0;
            else if(complement)
                complementDataByte <<= 1;
            else
                dataByte <<= 1;
            dataBitCounter++;
        }
        else if(zeroOneCounter == -2)
        {
            zeroOneCounter = 0;
            if(complement)
                complementDataByte |= 1;
            else
                dataByte |= 1;
        }
    }

    if(index == 52)
    {
       if(dataByte & complementDataByte)
       {
           isInvalidBit = true;
       }

       addressByte = dataByte;
       complementAddressByte = complementDataByte;

       dataByte = 0;
       complementDataByte = 0;
       complement = false;
    }

    index++;

    // clear interrupt flag
    TIMER1_ICR_R = TIMER_ICR_TATOCINT;
}
