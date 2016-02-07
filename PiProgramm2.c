#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <linux/i2c-dev.h>
#include <fcntl.h>
#include "bme280.h"
#include <sys/ioctl.h>

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <unistd.h>
#include <my_global.h>
#include <mysql.h>
#include <stdint.h>

#include "TSL2561.h"

#define ADDRESS_TSM 0x39
#define ADDRESS_ATMEGA 0x18
#define ADDRESS_BME 0x76
#define BUFFER_SIZE 31
#define CO2 1
#define CO 2
#define VOLUME 3
#define LIGHT 4
#define ACCLERATION 5
#define TEMPERATURE 6
#define HUMIDITY 7
#define PRESSURE 8
#define SMOKE 9
#define BUZZER 10



// The i2c bus: This is for V2 pi's. For  V1 Model B you need i20-c 0
static const char *devName = "/dev/i2c-1";

int t_fine;

double BME280_compensate_P_double(int adc_P,int dig_P1, int dig_P2, int dig_P3, int dig_P4, int dig_P5, int dig_P6, int dig_P7, int dig_P8, int dig_P9)
{
	double var1, var2, p;
	var1 = ((double)t_fine/2.0) - 64000.0;
	var2 = var1 * var1 * ((double)dig_P6) / 32768.0;
	var2 = var2 + var1 * ((double)dig_P5) * 2.0;
	var2 = (var2/4.0)+(((double)dig_P4) * 65536.0);
	var1 = (((double)dig_P3) * var1 * var1 / 524288.0 + ((double)dig_P2) * var1) / 524288.0;
	var1 = (1.0 + var1 / 32768.0)*((double)dig_P1);
	if (var1 == 0.0)
	{
		return 0; // avoid exception caused by division by zero
	}
	p = 1048576.0 - (double)adc_P;
	p = (p - (var2 / 4096.0)) * 6250.0 / var1;
	var1 = ((double)dig_P9) * p * p / 2147483648.0;
	var2 = p * ((double)dig_P8) / 32768.0;
	p = p + (var1 + var2 + ((double)dig_P7)) / 16.0;
	return p;
}
double BME280_compensate_H_double(int adc_H, int dig_H1, int dig_H2, int dig_H3, int dig_H4, int dig_H5, int dig_H6)
{
	double var_H;
	var_H = (((double)t_fine) - 76800.0);
	var_H = (adc_H - (((double)dig_H4) * 64.0 + ((double)dig_H5) / 16384.0 * var_H)) * (((double)dig_H2) / 65536.0 * (1.0 + ((double)dig_H6) / 67108864.0 * var_H * (1.0 + ((double)dig_H3) / 67108864.0 * var_H)));
	var_H = var_H * (1.0 - ((double)dig_H1) * var_H / 524288.0);
	if (var_H > 100.0)
		var_H = 100.0;
	else if (var_H < 0.0)
		var_H = 0.0;
	return var_H;
}
double BME280_compensate_T_double(int adc_T, int dig_T1, int dig_T2, int dig_T3)
{
	double var1, var2, T;
	var1 = (((double)adc_T)/16384.0 - ((double)dig_T1)/1024.0) * ((double)dig_T2);
	var2 = ((((double)adc_T)/131072.0 - ((double)dig_T1)/8192.0) * (((double)adc_T)/131072.0 - ((double) dig_T1)/8192.0)) * ((double)dig_T3);
	t_fine = (int)(var1 + var2);
	T = (var1 + var2) / 5120.0;
	return T;
}

unsigned short int swap_bytes(unsigned short int input)
{
	return (((input & 0xff) << 8) | ((input >> 8) & 0xff));
}


void finish_with_error(MYSQL *con)
{
  fprintf(stderr, "%s\n", mysql_error(con));
  mysql_close(con);
  exit(1);
}



int main(void)
{
	int file;
	// int addr=0x76;		// address of BME280
	int x;
	int adc_T,dig_T1,dig_T2,dig_T3;
	int adc_H,dig_H1,dig_H2,dig_H3,dig_H4,dig_H5,dig_H6;
	int adc_P,dig_P1,dig_P2,dig_P3,dig_P4,dig_P5,dig_P6,dig_P7,dig_P8,dig_P9;
	//float temperature,humidity,pressure;
	int temperature,humidity,pressure,co2,co,volume,light,accleration,smoke,buzzer;
	char query[100];
	int SensorId;
	char buf[BUFFER_SIZE];
	char cmd[1];
	int rc;
	uint16_t broadband, ir;
	uint32_t lux=0;

	// Init connection to DB
	MYSQL *con = mysql_init(NULL);
    	if (con == NULL)
  	{
      		fprintf(stderr, "%s\n", mysql_error(con));
      		exit(1);
  	}

  	if (mysql_real_connect(con, "localhost", "root", "admin",
          "ISDP", 0, NULL, 0) == NULL)
  	{
      		finish_with_error(con);
  	}

	// BME280
	if((file=open(devName,O_RDWR))<0)	// open i2c-bus
	{
		perror("cannot open i2c-1");
		exit(1);
	}
	if(ioctl(file, I2C_SLAVE, ADDRESS_BME)<0)	// open slave
	{
		perror("cannot open slave address");
		exit(1);
	}
	i2c_smbus_write_byte_data(file,0xf2,0x01);	// activate humidity measurement
	i2c_smbus_write_byte_data(file,0xf4,0x27);	// activate temperature measurement

	dig_P1=i2c_smbus_read_word_data(file, 0x8e);
	dig_P2=i2c_smbus_read_word_data(file, 0x90);
	dig_P3=i2c_smbus_read_word_data(file, 0x92);
	dig_P4=i2c_smbus_read_word_data(file, 0x94);
	dig_P5=i2c_smbus_read_word_data(file, 0x96);
	dig_P6=i2c_smbus_read_word_data(file, 0x98);
	dig_P7=i2c_smbus_read_word_data(file, 0x9a);
	dig_P8=i2c_smbus_read_word_data(file, 0x9c);
	dig_P9=i2c_smbus_read_word_data(file, 0x9f);
	dig_T1=i2c_smbus_read_word_data(file, 0x88);
	dig_T2=i2c_smbus_read_word_data(file, 0x8a);
	dig_T3=i2c_smbus_read_word_data(file, 0x8c);
	dig_H1=i2c_smbus_read_byte_data(file, 0xa1);
	dig_H2=i2c_smbus_read_word_data(file, 0xe1);
	dig_H3=i2c_smbus_read_byte_data(file, 0xe3);
	dig_H4=((i2c_smbus_read_byte_data(file, 0xe4))<<4)+(i2c_smbus_read_byte_data(file, 0xe5)&0x0f);
	dig_H5=((i2c_smbus_read_byte_data(file, 0xe5)&0xf0)>>4)+(i2c_smbus_read_byte_data(file, 0xe6)>>4);
	dig_H6=i2c_smbus_read_byte_data(file, 0xe7);
	/*printf("dig_H1=%i, dig_H2=%i, dig_H3=%i, dig_H4=%i, dig_H5=%i, dig_H6=%i\n",dig_H1,dig_H2,dig_H3,dig_H4,dig_H5,dig_H6);
	printf("dig_P1=%i, dig_P2=%i, dig_P3=%i, dig_P4=%i, dig_P5=%i, dig_P6=%i, dig_P7=%i, dig_P8=%i, dig_P=%i\n",dig_P1,dig_P2,dig_P3,dig_P4,dig_P5,dig_P6,dig_P7,dig_P8,dig_P9);
	printf("dig_T1=%i, dig_T2=%i, dig_T3=%i\n",dig_T1,dig_T2,dig_T3);*/

	adc_P=((swap_bytes(i2c_smbus_read_word_data(file, 0xF7)))<<4)+(((i2c_smbus_read_byte_data(file,0xF9))>>4)&0x0F);
	adc_T=((swap_bytes(i2c_smbus_read_word_data(file, 0xFA)))<<4)+(((i2c_smbus_read_byte_data(file,0xFC))>>4)&0x0F);
	adc_H=swap_bytes(i2c_smbus_read_word_data(file, 0xFD));

	/*printf("0x%0x\n",adc_T);
	printf("0x%0x\n",adc_H);
	printf("0x%0x\n",adc_P);*/

	temperature=(int)BME280_compensate_T_double(adc_T,dig_T1,dig_T2,dig_T3);
	humidity=(int)BME280_compensate_H_double(adc_H,dig_H1,dig_H2,dig_H3,dig_H4,dig_H5,dig_H6);
	pressure=(int)(BME280_compensate_P_double(adc_P,dig_P1,dig_P2,dig_P3,dig_P4,dig_P5,dig_P6,dig_P7,dig_P8,dig_P9)/100); //Pressure in hPa

	printf("Temperatur:  %i C\n",temperature);
	printf("Luftfeuchte: %i %%rH\n",humidity);
	printf("Luftdruck:   %i hPa\n",pressure);


	SensorId = TEMPERATURE; // Temperature
	sprintf(query, "INSERT INTO samplings (sensor_id, sampled, created_at) VALUES(%i,%d,CURRENT_TIMESTAMP)",SensorId,temperature);
	if (mysql_query(con, query)) {
      		finish_with_error(con);
  	}

	SensorId = HUMIDITY; // Humidity
	sprintf(query, "INSERT INTO samplings (sensor_id, sampled, created_at) VALUES(%i,%d,CURRENT_TIMESTAMP)",SensorId,humidity);
	if (mysql_query(con, query)) {
      		finish_with_error(con);
  	}

	SensorId = PRESSURE; // Pressure
	sprintf(query, "INSERT INTO samplings (sensor_id, sampled, created_at) VALUES(%i,%d,CURRENT_TIMESTAMP)",SensorId,pressure);
	if (mysql_query(con, query)) {
      		finish_with_error(con);
  	}

	close(file);

	//Acquire bus ATMEGA
	if((file=open(devName,O_RDWR))<0)	// open i2c-bus
	{
		perror("cannot open i2c-1");
		exit(1);
	}
	if(ioctl(file, I2C_SLAVE, ADDRESS_ATMEGA)<0)	// open slave
	{
		perror("cannot open slave address");
		exit(1);
	}


		cmd[0] = CO2; // Readout carbon dioxide CO2 value
		if (write(file, cmd, 1) == 1)
		{
			usleep(10000);	// wait for answer

			if ( read(file, buf, BUFFER_SIZE) == BUFFER_SIZE)
			{

			co2 = buf[1] | buf[0] << 8;
			printf("%i: %i\n", CO2,co2);
			sprintf(query, "INSERT INTO samplings (sensor_id, sampled, created_at) VALUES(%i,%i,CURRENT_TIMESTAMP)",CO2,co2);
				if (mysql_query(con, query)) {
      				finish_with_error(con);
  				}
			}
		}

		cmd[0] = CO; // Readout carbon monoxide CO value
		if (write(file, cmd, 1) == 1)
		{
			usleep(10000);	// wait for answer

			if ( read(file, buf, BUFFER_SIZE) == BUFFER_SIZE)
			{

			co = buf[1] | buf[0] << 8;
			printf("%i: %i\n", CO,co);
			sprintf(query, "INSERT INTO samplings (sensor_id, sampled, created_at) VALUES(%i,%i,CURRENT_TIMESTAMP)",CO,co);
				if (mysql_query(con, query)) {
      				finish_with_error(con);
  				}
			}
		}

		cmd[0] = VOLUME; // Readout volume value
		if (write(file, cmd, 1) == 1)
		{
			usleep(10000);	// wait for answer

			if ( read(file, buf, BUFFER_SIZE) == BUFFER_SIZE)
			{

			volume = buf[1] | buf[0] << 8;
			printf("%i: %i\n", VOLUME,volume);
			sprintf(query, "INSERT INTO samplings (sensor_id, sampled, created_at) VALUES(%i,%i,CURRENT_TIMESTAMP)",VOLUME,volume);
				if (mysql_query(con, query)) {
      				finish_with_error(con);
  				}
			}
		}


		cmd[0] = SMOKE; // Readout smoke value
		if (write(file, cmd, 1) == 1)
		{
			usleep(10000);	// wait for answer

			if ( read(file, buf, BUFFER_SIZE) == BUFFER_SIZE)
			{

			smoke = buf[1] | buf[0] << 8;
			printf("%i: %i\n", SMOKE,smoke);
			sprintf(query, "INSERT INTO samplings (sensor_id, sampled, created_at) VALUES(%i,%i,CURRENT_TIMESTAMP)",SMOKE,smoke);
				if (mysql_query(con, query)) {
      				finish_with_error(con);
  				}
			}
		}


	close(file);

	// Light Sensor
	// prepare the sensor
	// (the first parameter is the raspberry pi i2c master controller attached to the TSL2561, the second is the i2c selection jumper)
	// The i2c selection address can be one of: TSL2561_ADDR_LOW, TSL2561_ADDR_FLOAT or TSL2561_ADDR_HIGH
	TSL2561 light1 = TSL2561_INIT(1, ADDRESS_TSM);

	// initialize the sensor
	rc = TSL2561_OPEN(&light1);
	if(rc != 0) {
		fprintf(stderr, "Error initializing TSL2561 sensor (%s). Check your i2c bus (es. i2cdetect)\n", strerror(light1.lasterr));
		// you don't need to TSL2561_CLOSE() if TSL2561_OPEN() failed, but it's safe doing it.
		TSL2561_CLOSE(&light1);
		return 1;
	}

	// set the gain to 1X (it can be TSL2561_GAIN_1X or TSL2561_GAIN_16X)
	// use 16X gain to get more precision in dark ambients, or enable auto gain below
	rc = TSL2561_SETGAIN(&light1, TSL2561_GAIN_1X);

	// set the integration time
	// (TSL2561_INTEGRATIONTIME_402MS or TSL2561_INTEGRATIONTIME_101MS or TSL2561_INTEGRATIONTIME_13MS)
	// TSL2561_INTEGRATIONTIME_402MS is slower but more precise, TSL2561_INTEGRATIONTIME_13MS is very fast but not so precise
	rc = TSL2561_SETINTEGRATIONTIME(&light1, TSL2561_INTEGRATIONTIME_101MS);

	// sense the luminosity from the sensor (lux is the luminosity taken in "lux" measure units)
	// the last parameter can be 1 to enable library auto gain, or 0 to disable it
	rc = TSL2561_SENSELIGHT(&light1, &broadband, &ir, &lux, 1);
	printf("lux: %i\n", lux);
	light = (int) lux;
	sprintf(query, "INSERT INTO samplings (sensor_id, sampled, created_at) VALUES(%i,%i,CURRENT_TIMESTAMP)",LIGHT,light);
	if (mysql_query(con, query))
        {finish_with_error(con);}

	TSL2561_CLOSE(&light1);

	mysql_close(con);

	return 0;
}
