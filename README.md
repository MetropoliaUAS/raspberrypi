# Raspberry Pi2


Functionality:
- Establish I2C Connection 
- Read out Light Sensor TSL2561
- Read out Environmental Sensor BME280
- Read out remaining sensor on Atmega
- Write sensor values in MySQL-Database


Compile PiProgramm2.c in terminal with:
`gcc -o test PiProgramm2.c TSL2561.c `mysql_config - - flags - -libs``
