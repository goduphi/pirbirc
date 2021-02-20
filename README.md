# Programmable Bidirectional IR Remote-Control Interface
The purpose of this project was to design and implement remote control interface which has the ability to learn and play any NEC formatted IR signal.
In addition to the baseline project requirements, I have added IFTTT support.

### Hardware used:
Tiva C Series TM4C123GH6PM

### API's used:
- All API's used in this project were written based on the TM4C123GH6PM datasheet provided by Texas Instruments.

### Resource used:
Dr. Jason Losh's notes and programs
Director, Computer Engineering Undergraduate Program, The University of Texas at Arlington

### Compilation instructions:
I have used Code Composer Studio to compile the program and upload it to my Tiva C Series board. You should be able to download the source code and paste
it into a new CSS project and compile and run it.
  
### Bug Report 
- The EEPROM code requires some more work as sometimes the saved commands are not read in properly and hence cannot be played properly.
