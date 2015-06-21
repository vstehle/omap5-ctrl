/* omap5-ctrl
 *
 * mpsse mode for controlling power on/off/reset and checking status of
 * LEDS on omap5432 based PandaBoard5
 *
 * Author: David Anders <x0132446@ti.com>
 * Copyright (C) 2012 David Anders
 * Copyright (C) 2012 Texas Instruments
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <unistd.h>
#include <ftdi.h>

#define WR_LO_BANK	0x80
#define RD_LO_BANK	0x81
#define WR_HI_BANK	0x82
#define RD_HI_BANK	0x83
#define GPIO_DIR	0x20
#define GPIO_PWR_HI	0x20
#define GPIO_PWR_LO	0x00

static int set_power_button(struct ftdi_context ftdic, int level)
{
	unsigned char buf[3];
	int f;

	buf[0] = WR_HI_BANK;
	if (level)
		buf[1] = GPIO_PWR_HI;
	else
		buf[1] = GPIO_PWR_LO;

	buf[2] = GPIO_DIR;

	f = ftdi_write_data(&ftdic, buf, sizeof(buf));
	if (f < 0) {
		fprintf(stderr, "write failed on channel 2 for ");
		fprintf(stderr, " 0x%x, error %d (%s)\n", buf[0], f,
			ftdi_get_error_string(&ftdic));
		return f;
	}

	return 0;
}

static int check_pwr_led(struct ftdi_context ftdic, int expect)
{
	unsigned char buf[3], buf_return[3];
	int i;
	buf[0] = RD_HI_BANK;

	for (i = 0; i < 15 ; i++) {
		ftdi_write_data(&ftdic, buf, 1);
		ftdi_read_data(&ftdic, buf_return, 1);
		buf_return[0] &= (1<<1);
		if (buf_return[0] == expect)
			break;
		sleep(1);
	}

	return 0;

}

static int power_ctrl(struct ftdi_context ftdic, int status)
{

	set_power_button(ftdic, 1);

	set_power_button(ftdic, 0);

	if (status) {
		printf("waiting for power on confirmation...\n");
		sleep(1);
		set_power_button(ftdic, 1);
		check_pwr_led(ftdic, 0x00);
		printf("done!\n");
	} else {
		printf("waiting for power off confirmation...\n");
		check_pwr_led(ftdic, 0x02);
		set_power_button(ftdic, 1);
		printf("done!\n");
	}

	return 0;
}

static void show_help(void)
{
	printf("Usage: omap5-ctrl -p value -r -l\n");
	printf("Where -p value   = power the board 0 for off 1 for on\n");
	printf("      -r         = perform a board reset\n");
	printf("      -d descr   = select usb device by description\n");
	printf("                   For example: 'd:003/009'\n");
	printf("                   Refer to libftdi documentation\n");
}


int main(int argc, char **argv)
{
	struct ftdi_context ftdic;
	int f;
	int opt;
	int req_pattern = -1;
	char *descr_string = NULL;

	while ((opt = getopt(argc, argv, "hrp:d:")) != -1) {
		switch (opt) {
		case 'd':
			descr_string = optarg;
			break;
		case 'p':
			req_pattern = atoi(optarg);
			break;
		case 'r':
			req_pattern = 2;
			break;
		case 'h':
			show_help();
			return 0;
		default:
			exit(EXIT_FAILURE);
		}
	}

	if (req_pattern < 0) {
		show_help();
		return 0;
	}

	if (ftdi_init(&ftdic) < 0) {
		fprintf(stderr, "ftdi_init failed\n");
		return EXIT_FAILURE;
	}
	ftdi_set_interface(&ftdic, INTERFACE_B);

	if (descr_string) {
		// Device string given.
		f = ftdi_usb_open_string(&ftdic, descr_string);

	} else {
		// Open the first device by VID:PID.
		f = ftdi_usb_open(&ftdic, 0x0403, 0x6010);
	}

	if (f < 0 && f != -5) {
		fprintf(stderr, "unable to open ftdi device: %d (%s)\n",
		f, ftdi_get_error_string(&ftdic));
		exit(-1);
	}

	ftdi_set_bitmode(&ftdic, 0x00, BITMODE_MPSSE);

	power_ctrl(ftdic, req_pattern);

	ftdi_usb_close(&ftdic);
	ftdi_deinit(&ftdic);

	return 0;
}
