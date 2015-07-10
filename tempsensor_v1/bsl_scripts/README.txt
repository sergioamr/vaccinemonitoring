MSP430_CDC.inf - Windows driver for Olimex MSP430-BSL board
--------------------------------------
To install the drivers point your Windows Device Manager driver installer manually to the folder containing the .inf file.

If you are using Windows 8 you need to disable the security affecting the installation of unsigned drivers. There is plenty of information around the world wide web on how this is done.



thermal_flash_script.txt - script to flash thermalcanyon with firmware
--------------------------------------
Requires BSL_Scripter.exe (available from TI website)

1) apply program_sel jumper
2) connect bsl board to jumper, match arrow with "pin 1"
3) turn on board
NOTE: edit thermal_flash_script.exe COM10 to COM port of bsl board (found under device manager)
4) open cmd @ BSL_Scripter root dir and run "BSL_Scripter.exe thermal_flash_script.txt"
5) wait for DONE confirmation on RX_DATA BLOCK command

firmware.txt
--------------------------------------
default firmware ti-txt file (currently MITCH branch)