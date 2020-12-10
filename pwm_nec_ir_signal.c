/*
 * Sarker Nadir Afridi Azmi
 */

#include <stdint.h>
#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "uart0.h"
#include "common_terminal_interface.h"
#include "nec_ir_decoder.h"
#include "ir_emitter.h"
#include "eeprom.h"
#include "speaker.h"
#include "wait.h"

char commandName[11];
bool learning = false;

extern void printUB10Number(uint8_t x);
extern void fetchDevAction(char* dev, char* action, uint16_t* offset);
extern uint16_t eepromAddressOffset;
extern uint8_t commandsInsideOfEeprom;

uint8_t ruleCounter = 0;

void initHw(void)
{
    // Configure HW to work with 16 MHz XTAL, PLL enabled, sysdivider of 5, creating system clock of 40 MHz
    SYSCTL_RCC_R = SYSCTL_RCC_XTAL_16MHZ | SYSCTL_RCC_OSCSRC_MAIN | SYSCTL_RCC_USESYSDIV | (4 << SYSCTL_RCC_SYSDIV_S);

    // Set GPIO ports to use APB (not needed since default configuration -- for clarity)
    SYSCTL_GPIOHBCTL_R = 0;
}

void showHelp()
{
    putsUart0("\nhelp - shows all available commands.\n");
    putsUart0("\ndecode - shows all available commands.\n");
}

uint8_t fetchCommandName(char fetchCmdName[], uint16_t* offset)
{
    uint8_t i = 0;
    uint8_t nameAddressOffset = 0;
    uint32_t name = 0;
    uint8_t retVal = (*offset);
    // Concatenates together the 3 address that hold the name of the command
    while(nameAddressOffset < 3)
    {
        name = readEeprom((*offset)++);
        uint8_t j = 0;
        for(j = 0; j < 4; j++)
        {
            char c = ((name >> (j << 3)) & 0xFF);
            fetchCmdName[i++] = c;
        }
        nameAddressOffset++;
    }
    return retVal;
}

void writeStringEeprom(char* dev, char* action, uint16_t* stringAddressOffset)
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
    // Write the device name with a max length of 7 characters
    for(i = 1; i <= 8; i++)
    {
       if(i <= 7 && dev[i - 1] != '\0')
       {
           currentChar = dev[i - 1];
       }
       else
       {
           currentChar = '\0';
       }

       name |= (currentChar << (shiftValue++ << 3));

       if((i % 4) == 0)
       {
           writeEeprom((*stringAddressOffset)++, name);
           name = 0;
           shiftValue = 0;;
       }
    }
    name = 0;
    shiftValue = 0;
    // Write the action name with a max length of 3 characters
    for(i = 1; i <= 4; i++)
    {
       if(i <= 3 && action[i - 1] != '\0')
       {
           currentChar = action[i - 1];
       }
       else
       {
           currentChar = '\0';
       }

       name |= (currentChar << (shiftValue++ << 3));
    }
    writeEeprom((*stringAddressOffset)++, name);
}

void writeDataEeprom(uint8_t addr, uint8_t data, uint16_t* intAddrOffset)
{
    uint32_t dataAddress = addr;
    dataAddress = ((dataAddress << 8) | data);
    dataAddress |= VALID_COMMAND;
    writeEeprom((*intAddrOffset)++, dataAddress);
}

int main()
{
    initHw();
    initEeprom();

    uint32_t name = 0;
    // Read in the last stored number of commands
    // and where the commands are stored
    name = readEeprom(0);
    eepromAddressOffset = name & 0xFFFF;
    commandsInsideOfEeprom = (name >> 16) & 0xFF;

    name = 0;

    // I decided to store the rules starting from address 500
    uint16_t ruleAddressOffset = RULE_ADDRESS_START;

    name = readEeprom(RULE_META_DATA);
    ruleAddressOffset = name & 0xFFFF;
    ruleCounter = (name >> 16) & 0xFF;

    name = 0;

    initIrDecoder();
    initIrEmitter();
    initUart0();
    initSpeaker();

    // Endless loop
    while(true)
    {
        USER_DATA data;
        data.fieldCount = 0;
        putsUart0("Please enter you command: ");
        getsUart0(&data);
        parseField(&data);

        // The first address is reserved for meta data of the commands
        uint16_t cmdOffset = 1;
        char cmd[11];
        uint8_t commandIndex = 0;
        uint32_t dataAddress = 0;
        name = 0;

        if(isCommand(&data, "decode", 0))
        {
            // Turn on the edge triggered interrupt
            GPIO_PORTB_IM_R |= GPI_IR_SENSOR_MASK;
        }
        else if(isCommand(&data, "learn", 1))
        {
            uint8_t i = 0;
            for(i = 0; i < 12; i++)
            {
                commandName[i] = '\0';
            }
            // Get a deep copy of the buffer data to be used later
            // by other parts of the program
            strCpy(getFieldString(&data, 1), commandName);

            learning = true;
            putsUart0("Learning ...\n");
        }
        // To prove that the IR decoder can read any address and data
        else if(isCommand(&data, "play", 0))
        {
            // A janky software fix for the PWM issue mentioned in class
            PWM0_0_CTL_R = 0;
            PWM0_0_LOAD_R = 1047;
            PWM0_0_CTL_R = PWM_0_CTL_ENABLE;

            if((data.fieldCount == 3) &&
              (((data.fieldType[1] == 'a') && (data.fieldType[2] == 'n')) ||
               ((data.fieldType[1] == 'n') && (data.fieldType[2] == 'a'))))
            {
                putsUart0("\nPlease specify what command you want to play.\nplay <cmd1/integer> <cmd2/integer> ...\n\n");
                continue;
            }
            else if(data.fieldType[1] == 'n' && data.fieldType[2] == 'n')
            {
                int32_t arg1ptr = getFieldInteger(&data, 1);
                int32_t arg2ptr = getFieldInteger(&data, 2);

                playCommand(getInteger(&data, arg1ptr), getInteger(&data, arg2ptr));
                continue;
            }

            if((data.fieldCount >= 2) && (data.fieldType[1] == 'a'))
            {
                // Basically, select a play argument, find it, and play it
                uint8_t playArgCounter = 1;
                for(playArgCounter = 1; (playArgCounter < MAX_FIELDS) && (playArgCounter < data.fieldCount); playArgCounter++)
                {
                    char* currentPlayCommand = getFieldString(&data, playArgCounter);
                    cmdOffset = 1;
                    commandIndex = 0;
                    while(commandIndex < commandsInsideOfEeprom)
                    {
                        fetchCommandName(cmd, &cmdOffset);
                        if(stringCompare(cmd, currentPlayCommand))
                        {
                            // A janky software fix for the PWM issue mentioned in class
                            // The load value needs to be changed to that of the IR emitter
                            // because it gets changed by the speaker module
                            PWM0_0_CTL_R = 0;
                            PWM0_0_LOAD_R = 1047;
                            PWM0_0_CTL_R = PWM_0_CTL_ENABLE;

                            dataAddress = readEeprom(cmdOffset);
                            if(dataAddress & VALID_COMMAND)
                            {
                                // Play command if the interrupt is enabled
                                if(GPIO_PORTB_IM_R & GPI_IR_SENSOR_MASK)
                                {
                                    playCommand(((dataAddress >> 8) & 0xFF), (dataAddress & 0xFF));
                                    waitMicrosecond(WAIT_TIME);
                                }
                            }
                            else
                            {
                                putcUart0('\n');
                                putsUart0(currentPlayCommand);
                                putsUart0(": The command is invalid.\n");
                            }
                            break;
                        }
                        cmdOffset++;
                        commandIndex++;
                    }
                    // Again, a pretty janky solution
                    if(commandIndex == commandsInsideOfEeprom)
                        putsUart0("\nThe command does not exist.\n");
                }
            }
        }
        else if(isCommand(&data, "rule", 3) || isCommand(&data, "rule", 1) || isCommand(&data, "list", 1))
        {
            char* lcmd = getFieldString(&data, 0);
            bool list = false;
            if(stringCompare(lcmd, "list"))
            {
                char* rcmd = getFieldString(&data, 1);
                if(stringCompare(rcmd, "rules"))
                {
                    list = true;
                }
                else
                {
                    putsUart0("\nInvalid command.\n");
                    continue;
                }
            }
            while(commandIndex < commandsInsideOfEeprom)
            {
                fetchCommandName(cmd, &cmdOffset);
                if(stringCompare(cmd, getFieldString(&data, 1)) || list)
                {
                    uint32_t ruleAddressData = readEeprom(cmdOffset);
                    if(data.fieldCount == 4)
                    {
                        writeStringEeprom(getFieldString(&data, 2), getFieldString(&data, 3), &ruleAddressOffset);
                        writeDataEeprom(((ruleAddressData >> 8) & 0xFF), (ruleAddressData & 0xFF), &ruleAddressOffset);
                        ruleCounter++;
                    }
                    else if(data.fieldCount == 2 || list)
                    {
                        char dev[8];
                        char action[4];
                        uint8_t r = 0;
                        uint16_t listRuleAddrOffer = RULE_ADDRESS_START;
                        uint32_t searchAddrData = 0;
                        while(r < ruleCounter)
                        {
                            fetchDevAction(dev, action, &listRuleAddrOffer);
                            searchAddrData = readEeprom(listRuleAddrOffer++);
                            if((searchAddrData & 0xFF) == (ruleAddressData & 0xFF) &&
                              ((searchAddrData >> 8) & 0xFF) == ((ruleAddressData >> 8) & 0xFF)
                              && (ruleAddressData & VALID_COMMAND))
                            {
                                putsUart0("rule: ");
                                putsUart0(cmd);
                                putcUart0(' ');
                                putsUart0(dev);
                                putcUart0(' ');
                                putsUart0(action);
                                putcUart0('\n');
                                break;
                            }
                            r++;
                        }
                        if(r == ruleCounter && !list)
                        {
                            putsUart0("\nRule does not exist.\n");
                        }
                    }
                    if(!list)
                        break;
                }
                cmdOffset++;
                commandIndex++;
            }
            if(commandIndex == commandsInsideOfEeprom && !list)
                putsUart0("\nThe command does not exist.\n");
            uint32_t ruleMetaData = ruleCounter;
            ruleMetaData <<= 16;
            ruleMetaData |= ruleAddressOffset;
            writeEeprom(RULE_META_DATA, ruleMetaData);
        }
        else if(isCommand(&data, "info", 1) ||
                isCommand(&data, "erase", 1) ||
                isCommand(&data, "list", 0))
        {
            // A pretty janky work around to listing commands
            bool list = false;
            if(stringCompare(getFieldString(&data, 0), "list"))
            {
                list = true;
                if(commandsInsideOfEeprom == 0)
                {
                    putsUart0("\nEEPROM currently empty.\n\n");
                    continue;
                }
            }

            char* infoCmd = getFieldString(&data, 1);
            cmdOffset = 1;
            commandIndex = 0;
            while(commandIndex < commandsInsideOfEeprom)
            {
                uint8_t eraseParameterOffset = fetchCommandName(cmd, &cmdOffset);
                if(stringCompare(cmd, infoCmd) || list)
                {
                    dataAddress = readEeprom(cmdOffset);

                    // Erases the command
                    if(stringCompare(getFieldString(&data, 0), "erase"))
                    {
                        dataAddress &= ~VALID_COMMAND;
                        writeEeprom(cmdOffset, dataAddress);
                        dataAddress = 0;
                        writeEeprom(eraseParameterOffset, dataAddress);
                        putcUart0('\n');
                        putsUart0(cmd);
                        putsUart0(": Command has been erased.");
                    }
                    else if(!(dataAddress & VALID_COMMAND) && !list)
                        putsUart0("Command has been deleted.");
                    else if(dataAddress & VALID_COMMAND)
                    {
                        putsUart0("\nName of command:              ");
                        putsUart0(cmd);
                        putsUart0("\nAddress read from EEPROM:     ");
                        printUB10Number((dataAddress >> 8) & 0xFF);
                        putsUart0("\nData read from EEPROM:        ");
                        printUB10Number(dataAddress & 0xFF);
                        putcUart0('\n');
                    }

                    if(!list)
                        break;
                }
                cmdOffset++;
                commandIndex++;
            }
            // Again, a pretty janky solution
            if(commandIndex == commandsInsideOfEeprom && !list)
                putsUart0("\nThe command does not exist.\n");
        }
        else if(isCommand(&data, "alert", 2))
        {
            uint32_t alert = readEeprom(0);
            if(stringCompare(getFieldString(&data, 1), "good"))
            {
                if(stringCompare(getFieldString(&data, 2), "on"))
                {
                    alert |= 0x80000000;
                }
                else if(stringCompare(getFieldString(&data, 2), "off"))
                {
                    alert &= ~0x80000000;
                }
                writeEeprom(0, alert);
            }
            else if(stringCompare(getFieldString(&data, 1), "bad"))
            {
                if(stringCompare(getFieldString(&data, 2), "on"))
                {
                    alert |= 0x40000000;
                }
                else if(stringCompare(getFieldString(&data, 2), "off"))
                {
                    alert &= ~0x40000000;
                }
                writeEeprom(0, alert);
            }
            else
            {
                putsUart0("\nInvalid argument.\n");
            }
        }
        else if(isCommand(&data, "resetEeprom", 0))
        {
            eepromAddressOffset = 1;
            commandsInsideOfEeprom = 0;
            ruleAddressOffset = RULE_ADDRESS_START;
            ruleCounter = 0;
            writeEeprom(0, 1);
            putsUart0("\nEEPROM has been eradicated.\n");
        }
        else
        {
            putsUart0("\nInvalid command.\n");
        }

        putcUart0('\n');
    }
}
