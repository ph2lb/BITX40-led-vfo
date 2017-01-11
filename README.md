# BITX40-led-vfo summary
BITX40 VFO based on a Arduino with AD9833 and MAX7219   
 

# BITX40 VFO background info
The BITX40 uses a VFO in the range from 4.8Mhz to 5Mhz allowing to tune a 200Khz piece of a selected band.
 
# This project
Contains the code for a Arduino based external VFO for the BITX40 with AD9833 DDS to generate the frequency and a MAX7219 LED display as user interface. Allowing the user to change frequency and step size . The bandplan is defined in a struct in the main source file (default PA country full licensed bandplan).

# Hardware used : 
- Arduino Pro Mini  
- AD9833 DDS
- MAX7219 led display (8 digit) 
- Simpel rotary encoder (gray code encoder)

# External project page 
- http://www.ph2lb.nl/blog/index.php?page=bitx40-module

# Note
The MAX7219 is a real noise generator so a good seperate powersupply is needed (use aditional 1mH inductors and power line and ground line).  
