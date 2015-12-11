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

int main( int argc, char const * const argv[] )
{
	struct pollfd fd ;
	fd.fd = open(argv[1], O_RDONLY);
	fd.events = POLLIN ;
	
	int doit = 1 ;
	ioctl( fd.fd, O_NONBLOCK, &doit );

	int x ;
	int y ;
	int touched = 0 ;
	int device_id = -1 ;
	while (0 < poll(&fd, 1,- 1U)) {
		struct input_event event ;
		int numRead ;
		while (sizeof(event) == (numRead=read(fd.fd,&event,sizeof(event)))){
			printf( "%x:%x:%x\n", event.type, event.code, event.value);
			fflush(stdout);
			if (EV_ABS == event.type) {
			    if (ABS_X == event.code)
				    x = event.value ;
			    else if (ABS_Y == event.code) {
				    y = event.value ;
			    }
			    else if (ABS_PRESSURE == event.code) {
				    touched = (0 != event.value);
			    }
			    else {
				    printf( "ABS:%x:%x:%x\n", event.type, event.code, event.value);
				    fflush(stdout);
			    }
			}
			else if (EV_KEY == event.type) {
				if (BTN_TOUCH == event.code) {
					if (0 == event.value) {
						printf("release\n");
						fflush(stdout);
					} else {
						printf("touch(%d) %3d\t%3d\n", device_id, x, y );
						fflush(stdout);
					}
				} else{
					printf ("key(0x%x/0x%d)\n", event.code, event.value);
					fflush(stdout);
				}
			}
			else if (EV_SYN == event.type) {
				if (touched) {
					printf("touched(%d) %3d\t%3d\n", device_id, x, y );
					fflush(stdout);
				}
			} else{
				printf( "unrecognized event 0x%x\n", event.type);
				fflush(stdout);
			}
		}
	}

	return 0 ;
}

