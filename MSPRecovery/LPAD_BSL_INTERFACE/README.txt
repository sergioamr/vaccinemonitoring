/*****************************************************************************
* README.txt
* SLAA535 - MSP430 Launchpad UART BSL Interface associated files
* v.2.1.0
*****************************************************************************/

This package contains the collateral/associated files of 
SLAA 535 - Launchpad- Based MSP430 UART BSL Interface. 

The directory structures of this package is explained as follows:

LPAD_BSL_INTERFACE
 |
 |- Firmware
 |   |
 |   |- Projects
 |   |   |
 |   |   |- Binary: TI-TXT and HEX file for the MSP430G2231 firmware 

 |   |   |
 |   |   |- CCS: project files for CCSTUDIO v5.2
 |   |   | 
 |   |   |- IAR: project files for IAR EWB 5.40
 |   |   
 |   |- Source: source code of Launchpad BSL interface
 |  
 |- TestScripts: containing folders with test scripts for various MSP430 
                 target devices and also the ZIP file containing the CCS 
                 project for creating the binary TI TXT file used together 
                 with the test scripts. 
                 To run a test script, go to a device specific device directory,
                 adjust the COM PORT setting in the scripts (for BSL_Scripter 
                 related - 5xx, CC430 devices) or in the bat file  (for 
                 BSLDEMO2 related - 1xx, 2xx, 4xx devices), and execute the 
                 test.bat batch file.

When importing the CCS project file, it is important not to click the
'Copy projects into workspace' check button, because the source code is
statically linked into the CCS project and will not be copied when
trying to copy the CCS project.



 
 