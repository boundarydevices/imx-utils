#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <linux/types.h>
#include <linux/input.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#define ABS_MT_TOUCH_MAJOR	0x30	/* Major axis of touching ellipse */
#define ABS_MT_TOUCH_MINOR	0x31	/* Minor axis (omit if circular) */
#define ABS_MT_WIDTH_MAJOR	0x32	/* Major axis of approaching ellipse */
#define ABS_MT_WIDTH_MINOR	0x33	/* Minor axis (omit if circular) */
#define ABS_MT_ORIENTATION	0x34	/* Ellipse orientation */
#define ABS_MT_POSITION_X	0x35	/* Center X ellipse position */
#define ABS_MT_POSITION_Y	0x36	/* Center Y ellipse position */
#define ABS_MT_TOOL_TYPE	0x37	/* Type of touching device */
#define ABS_MT_BLOB_ID		0x38	/* Group a set of packets as a blob */
#define ABS_MT_TRACKING_ID	0x39	/* Unique ID of initiated contact */
#define ABS_MT_PRESSURE		0x3a	/* Pressure on contact area */
#define SYN_MT_REPORT		2

static char const *typeName(unsigned type)
{
	if (EV_SYN==type) {
		return " SYN" ;
	} else if (EV_ABS==type) {
		return " ABS" ;
	} else {
		static char temp[8];
		snprintf(temp,sizeof(temp),"0x%02x",type);
		return temp ;
	}
}

static char const *codeName(unsigned type, unsigned code) {
	if (EV_ABS == type) {
		if (ABS_MT_TOUCH_MAJOR == code) {
			return "MAJOR" ;
		}
		else if (ABS_MT_TOUCH_MINOR == code) {
			return "MINOR" ;
		}
		else if (ABS_MT_WIDTH_MAJOR == code) {
			return "WIDTH_MAJOR" ;
		}
		else if (ABS_MT_WIDTH_MINOR == code) {
			return "WIDTH_MINOR" ;
		}
		else if (ABS_MT_ORIENTATION == code) {
			return "ORIENTATION" ;
		}
		else if (ABS_MT_POSITION_X == code) {
			return "POS_X" ;
		}
		else if (ABS_MT_POSITION_Y == code) {
			return "POS_Y" ;
		}
		else if (ABS_MT_TOOL_TYPE == code) {
			return "TOOL_TYPE" ;
		}
		else if (ABS_MT_BLOB_ID == code) {
			return "BLOB_ID" ;
		}
		else if (ABS_MT_TRACKING_ID == code) {
			return "TRACKING_ID" ;
		}
	} else if (EV_SYN == type) {
		if (SYN_MT_REPORT==code) {
			return "MT_REPORT" ;
		} else if (SYN_REPORT==code) {
			return "SYNC" ;
		}
	}

	static char temp[8];
	snprintf(temp,sizeof(temp),"0x%04x", code);
	return temp ;
}

int main( int argc, char const * const argv[] )
{
	struct pollfd fd ;
	fd.fd = open(argv[1], O_RDONLY);
	fd.events = POLLIN ;
	
	int doit = 1 ;
	ioctl( fd.fd, O_NONBLOCK, &doit );

	while (0 < poll(&fd, 1,- 1U)) {
		struct input_event event ;
		int numRead ;
		while (sizeof(event) == (numRead=read(fd.fd,&event,sizeof(event)))){
			printf("type %s\tcode %s\tvalue 0x%04x\n", typeName(event.type),codeName(event.type,event.code),event.value);
		}
	}

	return 0 ;
}

