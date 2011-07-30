/*
 * Program: pmic.cpp
 *
 * This program allows access to the registers of the DA905X pmic.
 *
 * Copyright Boundary Devices, Inc. 2011
 */

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/mfd/da9052/dev.h>
#include <linux/mfd/da9052/reg.h>

#define PMICDEV "/dev/da905x"

int main( int argc, char const **argv ) {
	char const *pmicDev = getenv("PMICDEV");
	if (0 == pmicDev)
		pmicDev=PMICDEV ;

	int const fd = open(pmicDev, O_RDWR);
	if (0 <= fd) {
		if (1 < argc) {
			unsigned regnum = strtoul(argv[1],0,16);
			if (0 == strcasecmp("all", argv[1])) {
				for (unsigned regnum=DA9052_PAGE0_REG_START ; regnum <= DA9052_PAGE0_REG_END; regnum++) {
					da90_reg_and_value_t rnv = MAKERNV(regnum,0);
					int retval = ioctl(fd,DA905X_GETREG,&rnv);
					if (0 == retval) {
						printf( "da90[%02x] == %02x\n", GETREGNUM(rnv), GETVAL(rnv));
					} else
						perror("DA905X_SETREG");
				}
			} else if ((DA9052_PAGE0_REG_START <= regnum)
				    &&
				   (DA9052_PAGE0_REG_END >= regnum)) {
				if (2 < argc) {
					unsigned value = strtoul(argv[2],0,16);
					if (256 > value) {
						da90_reg_and_value_t rnv = MAKERNV(regnum,value);
						int retval = ioctl(fd,DA905X_SETREG,&rnv);
						if (0 == retval) {
							printf( "da90[%02x] == %02x\n", GETREGNUM(rnv), GETVAL(rnv));
						} else
							perror("DA905X_SETREG");
					} else
						fprintf (stderr, "reg value %s is out of range [0..0xff]\n", argv[2]);
				} else {
					da90_reg_and_value_t rnv = MAKERNV(regnum,0);
					int retval = ioctl(fd,DA905X_GETREG,&rnv);
					if (0 == retval) {
						printf( "da90[%02x] == %02x\n", GETREGNUM(rnv), GETVAL(rnv));
					} else
						perror("DA905X_GETREG");
				} // read
			} else
				printf( "register %s out of range [0x%x..0x%x]\n",
					argv[1], DA9052_PAGE0_REG_START, DA9052_PAGE0_REG_END);
		}
		close(fd);
	} else
		perror(pmicDev);
        return 0 ;
}
