# Raspberry Pi 2


### Functionality:
- Establish I2C Connection 
- Read out Light Sensor TSL2561
- Read out Environmental Sensor BME280
- Read out remaining sensors on Atmega
- Alert message through buzzer if defined threshold values for sensors are reached
- Write sensor values in MySQL-Database


### Compilation in Terminal
Compile PiProgramm2.c in terminal and save executable file under the name ISDP:

```gcc -o ISDP PiProgramm2.c TSL2561.c  `mysql_config - - cflags - -libs````

### Setup Autostart for ISDP Programm with rc.local
1. in Terminal type: ```sudo nano /etc/rc.local```
2. add after comments: ```/home/pi/IDSP  # run ISDP```
3. save file with: STRG+O, then press Return, then press STRG+X to exit file
