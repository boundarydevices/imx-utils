/*
 * read data from TekPower TP4000ZC
 *
 * See http://xuth.net/programming/tp4000zc/tp4000zc.py for details.
 * Also http://xuth.net/programming/tp4000zc/tp4000zc_serial_protocol.jpg
 *
 */
#include <stdio.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>

static bool volatile doExit = false ;

static void ctrlcHandler( int signo ) {
	printf( "<ctrl-c>\r\n" );
        doExit = true ;
}

struct value_to_char {
	unsigned char v;
	char c ;
};

static struct value_to_char translations[] = {
	{0x05, '1'}
,	{0x5b, '2'}
,	{0x1f, '3'}
,	{0x27, '4'}
,	{0x3e, '5'}
,	{0x7e, '6'}
,	{0x15, '7'}
,	{0x7f, '8'}
,	{0x3f, '9'}
,	{0x7d, '0'}
,	{0x68, 'L'}
,	{0x00, ' '}
};

static unsigned num_translations = sizeof(translations)/sizeof(translations[0]);

static char translate(unsigned char v) {
	for (int i = 0; i < num_translations; i++) {
		if (translations[i].v == v) {
			return translations[i].c;
		}
	}
	return '?';
}

static void process(unsigned char const *inData, unsigned char len)
{
#if 0
	printf("%2u: ", len);
	for(int i=0; i < len; i++) {
		printf("%02x ", inData[i]);
	}
	printf("\n");
	fflush(stdout);
#endif
	if (14 == len) {
		/* sanity check sequence numbers */
		for (int i=0; i < len; i++) {
			if (i+1 != (inData[i]>>4)) {
				printf("invalid seq\n");
				return;
			}
		}
#if 0
		bool flags[4];
		unsigned char digits[4];
		for (int i=0; i < 4; i++) {
			flags[i]  = (0 != (inData[2*i+1]&0x08));
			digits[i] = ((inData[2*i+1]&0x07)<<4)
			          | (inData[2*i+2]&0x0F);
		}
		for (int i=0; i < 4; i++) {
			if (flags[i])
				fputc('.',stdout);
			fputc(translate(digits[i]),stdout);
		}
		printf("\n"); fflush(stdout);
#endif
		char outBuf[80];
		int outLen = 0;
		for (int i=0; i < 4; i++) {
			if(0 != (inData[2*i+1]&0x08))
				outBuf[outLen++] = '.';
			unsigned char v = ((inData[2*i+1]&0x07)<<4)
                                         | (inData[2*i+2]&0x0F);
			outBuf[outLen++] = translate(v);
		}
		outBuf[outLen] = '\0';
		int start;
		for (start = 0; start < outLen; start++) {
			if ('0' != outBuf[start])
				break;
		}
		double dub ;
		if (1 == sscanf(outBuf+start,"%lf",&dub)) {
			printf("%.2lf\n", dub);
			fflush(stdout);
		}
//		printf("%s\n",outBuf+start); fflush(stdout);
	}
}

int main( int argc, char const * const argv[])
{
	if (1 == argc) {
		fprintf(stderr, "Usage: %s device\n",
			argv[0]);
		return -1;
	}
	int const fdSerial = open(argv[1], O_RDWR);
	if(0 > fdSerial) {
		perror(argv[1]);
		return -1;
	}
	fcntl(fdSerial, F_SETFD, FD_CLOEXEC);
	fcntl(fdSerial, F_SETFL, O_NONBLOCK);
	struct termios oldSerialState;

	tcgetattr(fdSerial, &oldSerialState);
	struct termios newState = oldSerialState;
	newState.c_cc[VMIN] = 1;
	cfsetispeed(&newState, B2400);
	cfsetospeed(&newState, B2400);
	newState.c_cc[VTIME] = 0;
	newState.c_cflag &= ~(PARENB|CSTOPB|CSIZE|CRTSCTS);
	newState.c_cflag |= (CLOCAL | CREAD |CS8);                       // Select 8 data bits
	newState.c_lflag &= ~(ICANON | ECHO );                           // set raw mode for input
	newState.c_iflag &= ~(IXON | IXOFF | IXANY|INLCR|ICRNL|IUCLC);   //no software flow control
	newState.c_oflag &= ~OPOST;                      //raw output
	tcsetattr(fdSerial, TCSANOW, &newState);
	
	pollfd fds[1]; 
	fds[0].fd = fdSerial ;
	fds[0].events = POLLIN | POLLERR ;
	
	signal(SIGINT, ctrlcHandler);

	int timeout= -1;
	unsigned char inBuf[256];
	unsigned char inLength = 0;

	while( !doExit ) {
		int const numReady = ::poll(fds, 2, timeout);
		if( 0 < numReady ) {
			if (fds[0].revents & POLLIN) {
				int numRead = read(fds[0].fd, 
						   inBuf+inLength, 
						   sizeof(inBuf)-inLength);
				if (0 < numRead) {
					inLength += numRead;
					if (inLength == sizeof(inBuf)) {
						process(inBuf,inLength);
						inLength = 0;
					}
					timeout = 1;
				}
			}
			else if(inLength){
				process(inBuf,inLength);
				inLength = 0;
				timeout=-1;
			}
			if (fds[0].revents & POLLERR)
				printf("err\n");
		}
		else if(inLength){
			process(inBuf,inLength);
			inLength = 0;
			timeout=-1;
		}
	}
	
	tcsetattr( fdSerial, TCSANOW, &oldSerialState );
	 
	close( fdSerial );
	return 0;
}
