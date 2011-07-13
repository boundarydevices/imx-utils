/*
 * LTC1960 test program
 *
 * Copyright (c) 2011  EMAC Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 */

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include "spidev.h"

#define CHARGE_BAT1	0x80
#define CHARGE_BAT2	0x40
#define CHARGE_CMD	0x0E

#define POWERPATH_CMD	0x06
#define POWER_BY_DC	0x20
#define POWER_BY_BAT2	0x40
#define POWER_BY_BAT1	0x80

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static void pabort(const char *s)
{
	perror(s);
	abort();
}

static const char * device = "/dev/spidev2.1";
static uint8_t mode;
static uint8_t bits = 8;
static uint32_t speed = 100000;
static uint16_t delay = 0;

int battery_sel = 0;
static struct itimerval tout_val;

int fd;

static void status()
{
	int ret;
	uint8_t tx[2];
	uint8_t rx[2];

	tx[0] = 0x04;
	tx[1] = 0x05;

	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)rx,
		.len = ARRAY_SIZE(tx),
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
	};

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1)
		pabort("can't send spi message");

	printf("Charging   = %x\n", (rx[1] >> 2) & 1);
	printf("Power Fail = %x\n", (rx[1] >> 3) & 1);
	printf("DCDIV      = %x\n", (rx[1] >> 4) & 1);
	printf("Low Power  = %x\n", (rx[1] >> 5) & 1);
	printf("Fault      = %x\n", (rx[1] >> 6) & 1);


}

static void charge(int battery)
{
	int ret;

	uint8_t tx[1];
	uint8_t rx[1];

	tx[0] = battery | CHARGE_CMD;

	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)rx,
		.len = ARRAY_SIZE(tx),
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
	};

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1)
		pabort("can't send spi message");

}

static void voltage_dac(double voltage)
{
	int ret;
	int vdac;
	int i;

	uint8_t tx[2];
	uint8_t rx[2];

	tx[0] = 0x01;
	tx[1] = 0x08;


	vdac = (int) floor(((voltage-.8)/32.752) * 2047);

	for(i=0;i<10;i++)
	{
		if(i<7)
			tx[0] |= (vdac & (1 << i)) ? (1 << (7-i)) : 0;
		else
			tx[1] |= (vdac & (1 << i)) ? (1 << (14-i)) : 0;
	}

	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)rx,
		.len = ARRAY_SIZE(tx),
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
	};

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1)
		pabort("can't send spi message");

	do
	{
		tx[0] = 0x01;
		struct spi_ioc_transfer tr = {
			.tx_buf = (unsigned long)tx,
			.rx_buf = (unsigned long)rx,
			.len = 1,
			.delay_usecs = delay,
			.speed_hz = speed,
			.bits_per_word = bits,
		};

		ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
		if (ret < 1)
			pabort("can't send spi message");
	} while(rx[0]==0xFF);

}

static void current_dac(double current, int low_current)
{
	int ret;
	int cdac;
	int i;

	uint8_t tx[2];
	uint8_t rx[2];

	tx[0] = 0x01;
	tx[1] = 0x00;

	cdac = (int) (((current)*1023*0.05)/0.1023);

	for(i=0;i<10;i++)
	{
		if(i<7)
			tx[0] |= (cdac & (1 << i)) ? (1 << (7-i)) : 0;
		else
			tx[1] |= (cdac & (1 << i)) ? (1 << (14-i)) : 0;
	}

	tx[1] |= (low_current) ? 0x10 : 0x00;

	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)rx,
		.len = ARRAY_SIZE(tx),
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
	};

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1)
		pabort("can't send spi message");

	do
	{
		tx[0] = 0x01;
		struct spi_ioc_transfer tr = {
			.tx_buf = (unsigned long)tx,
			.rx_buf = (unsigned long)rx,
			.len = 1,
			.delay_usecs = delay,
			.speed_hz = speed,
			.bits_per_word = bits,
		};

		ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
		if (ret < 1)
			pabort("can't send spi message");
	} while(rx[0]==0xFF);

}

void charge_timer(int i)
{
	if(battery_sel) charge(battery_sel);

	signal(SIGALRM, charge_timer);
}

void init_timer()
{
	tout_val.it_interval.tv_sec = 1; /* Next Value in seconds */
	tout_val.it_interval.tv_usec = 1; /* Next Value in microseconds */
	tout_val.it_value.tv_sec = 1; /* Current Value in seconds */
	tout_val.it_value.tv_usec = 1; /* Current Value in microseconds */
	setitimer(ITIMER_REAL, &tout_val, 0);
	signal(SIGALRM, charge_timer);
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int done = 0;
	int low_current = 0;
	char key;
	double voltage, current;

	fd = open(device, O_RDWR);
	if (fd < 0)
		pabort("Cannot open device");

	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("Cannot set bits per word");

	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("Cannot set max speed hz");


	init_timer();

	printf("LTC1960 Battery Charger Demo\n\n");

	current = 2.0;
	voltage = 12.8;

	voltage_dac(voltage);
	current_dac(current, low_current);

	while(!done)
	{

		status();

		printf("\n");

		printf("Battery 1 is %s.\n", (battery_sel==CHARGE_BAT1) ? "charging" : "not charging");
	        printf("Battery 2 is %s.\n", (battery_sel==CHARGE_BAT2) ? "charging" : "not charging");
		printf("Current = %lfA\n",current);
		printf("Voltage = %lfV\n",voltage);
		printf("Low Current Mode %s.\n", (low_current) ? "ON" : "OFF");

		printf("\n");

		printf("1: Set battery voltage.\n");
		printf("2: Set battery current.\n");
		printf("3: Charge battery 1. \n");
		printf("4: Charge battery 2. \n");
		printf("5: Toggle Low Current Mode. \n");
		printf("6: Stop Charging. \n");
		printf("7: Print smart battery info.\n");
		printf("q: Quit Program\n");

		printf("\nInput: ");

		do
		{
			scanf("%c", &key);
		} while(key == '\n');

		printf("\n\n");
		switch(key)
		{
			case '1': printf("Voltage = ");
				  scanf("%lf", &voltage);
				  voltage_dac(voltage);
				  break;
			case '2': printf("Current = ");
				  scanf("%lf", &current);
				  current_dac(current, low_current);
				  break;
			case '3': battery_sel = CHARGE_BAT1;
				  charge(battery_sel);
				  break;
			case '4': battery_sel = CHARGE_BAT2;
				  charge(battery_sel);
				  break;
			case '5': low_current ^=1;
				  current_dac(current, low_current);
				  break;
			case '6': battery_sel = 0;
				  sleep(1);
				  break;
			case '7': system("cat /sys/class/power_supply/battery/uevent");
				  break;
			case 'q': done = 1;
				  break;
			default: continue;
		}
	}

	close(fd);

	return ret;
}
