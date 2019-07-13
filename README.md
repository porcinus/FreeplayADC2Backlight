# FreeplayADC2Backlight
This program is design to update screen backlight using a potentiometer connected to a MCP3021A ADC. 

# History
- 0.1a : Initial release, PreAlpha stage.

# Provided scripts :
- compile.sh : Compile cpp file (run this first), require libi2c-dev.
- install.sh : Install service (need restart).
- remove.sh : Remove service.

Note: Don't miss to edit compile.sh and then nns-freeplay-adc-backlight-daemon.service set path and others arguments correctly.
compile.sh will run the program in test mode, this way, no backlight data will be wrote on the file system.

# Todo

# Issues
