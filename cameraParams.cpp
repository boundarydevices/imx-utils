/*
 * Module cameraParams.cpp
 *
 * This module defines the methods of the cameraParams_t class
 * as declared in cameraParams.h
 *
 * Copyright Boundary Devices, Inc. 2010
 */

#include "cameraParams.h"
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "fourcc.h"
#include <linux/videodev2.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

static bool parseIP(char const *ip_and_port, unsigned &targetAddr, unsigned short &targetPort)
{
	printf( "parsing IP and port: %s\n", ip_and_port);
	char const *colon = strchr(ip_and_port,':');
	char ip_address[256];
	if(colon &&((colon-ip_and_port)<sizeof(ip_address))){
		unsigned len = colon-ip_and_port ;
		memcpy(ip_address,ip_and_port,len);
		ip_address[len] = '\0' ;
                struct in_addr addr ;
                if(inet_aton(ip_address, &addr )){
			unsigned port = strtoul(colon+1,0,0);
			if(port && (port < 0x10000) ){
				targetAddr = addr.s_addr ;
				targetPort = port ;
				return true ;
			}
		}
	}
	return false ;
}

cameraParams_t::cameraParams_t( int &argc, char const **&argv )
: inwidth(480)
, inheight(272)
, rotation(camera_t::ROTATE_NONE)
, fps(30)
, fourcc(V4L2_PIX_FMT_YUV420)
, x(0)
, y(0)
, outwidth(480)
, outheight(272)
, transparency(0)
, color_key(0xFFFFFFFF)
, cameraDevName("/dev/video0")
, previewDevName("/dev/video16")
, saveFrame(-1)
, iterations(-1)
, broadcastAddr(0)
, broadcastPort(0)
{
	for ( int arg = 1 ; arg < argc ; arg++ ) {
		if ( '-' == *argv[arg] ) {
			char const *param = argv[arg]+1 ;
			char const cmdchar = tolower(*param);
			if ( 'o' == cmdchar ) {
				char const second = tolower(param[1]);
				if ('w' == second) {
					outwidth = strtoul(param+2,0,0);
				}
				else if ('h'==second) {
					outheight = strtoul(param+2,0,0);
				}
				else
					printf( "unknown output option %c\n",second);
			}
			else if ( 'i' == cmdchar ) {
				char const second = tolower(param[1]);
				if ('w' == second) {
					inwidth = strtoul(param+2,0,0);
				}
				else if ('h'==second) {
					inheight = strtoul(param+2,0,0);
				}
				else if (isdigit(second))
					iterations=strtol(param+1,0,0);
				else
					printf( "unknown output option %c\n",second);
			}
			else if( 'f' == tolower(*param) ){
            			fps = strtol(param+1,0,0);
			}
			else if( '4' == *param ) {
                                unsigned fcc ; 
                                if(supported_fourcc(param+1,fcc)){
                                    fourcc = fcc ;
                                } else {
                                    fprintf(stderr, "Invalid format %s\n", param+1 );
                                    fprintf(stderr, "supported formats include:\n" );
                                    unsigned const *formats ; unsigned num_formats ;
                                    supported_fourcc_formats(formats,num_formats);
                                    while( num_formats-- ){
                                        fprintf(stderr, "\t%s\n", fourcc_str(*formats));
                                        formats++ ;
                                    }
                                    exit(1);
                                }
			}
			else if ( 'd' == cmdchar ) {
				cameraDevName = param+1 ;
			}
			else if ( 'x' == cmdchar ) {
				x = strtoul(param+1,0,0);
			}
			else if ( 'y' == cmdchar ) {
				y = strtoul(param+1,0,0);
			}
			else if ( 'k' == cmdchar ) {
				color_key = strtoul(param+1,0,0);
			}
			else if ( 't' == cmdchar ) {
				transparency = strtoul(param+1,0,0);
			}
			else if ( 'b' == cmdchar ) {
				if(!parseIP(param+1,broadcastAddr,broadcastPort))
					fprintf(stderr, "Invalid broadcast spec (%s), use form 127.0.0.1:2020\n", param+1);
			}
			else if ( 'r' == cmdchar ) {
				switch (tolower(param[1])) {
					case 'v': rotation = camera_t::FLIP_VERTICAL ; break ;
					case 'h': rotation = camera_t::FLIP_HORIZONTAL ; break ;
					case 'b': rotation = camera_t::FLIP_BOTH ; break ;
					case 'l': rotation = camera_t::ROTATE_90_LEFT ; break ;
					case 'r': rotation = camera_t::ROTATE_90_RIGHT ; break ;
					case 'n': rotation = camera_t::ROTATE_NONE ; break ;
					default: {
							printf("Invalid rotation character %c\n"
							       "Valid options are:\n"
							       "    v   - flip vertical\n"
							       "    h   - flip horizontal\n"
							       "    b   - flip both\n"
							       "    l   - rotate 90 degrees left (counter-clockwise)\n"
							       "    r   - rotate 90 degrees right (clockwise)\n"
							       , param[1]);
						}
				}
			}
			else if ( 's' == cmdchar ) {
				saveFrame = strtol(param+1,0,0);
			}
			else if ( '?' == cmdchar ) {
				printf( "Usage: %s [option]\n"
					"\t-iw480        - set input width to 480\n"
					"\t-ih272        - set input height to 272\n"
					"\t-f30          - set camera frames per second to 30\n"
					"\t-4I420        - set camera fourcc to I420\n"
					"\t-x10          - set preview x position to 10\n"
					"\t-y10          - set preview y position to 10\n"
					"\t-ow480        - set preview width to 480\n"
					"\t-oh272        - set preview height to 272\n"
					"\t-t128         - set transparency 0(opaque) to 255(transparent)\n"
					"\t-k0x0000      - set color key to RGB16\n"
					"\t-b10.0.0.1:2020 - set broadcast target\n"
					"\t-rX           - set rotation to X. Choices are:\n"
					"                      v   - flip vertical\n"
					"                      h   - flip horizontal\n"
					"                      b   - flip both\n"
					"                      l   - rotate 90 degrees left (counter-clockwise)\n"
					"                      r   - rotate 90 degrees right (clockwise)\n"
					"\t-d/dev/blah   - set camera device to /dev/blah\n"
					, argv[0]);
				exit(-1);
			}
			else
				printf( "unknown option %s\n", param );

			// pull from argument list
			for ( int j = arg+1 ; j < argc ; j++ ) {
				argv[j-1] = argv[j];
			}
			--arg ;
			--argc ;
		}
	}
}

void cameraParams_t::dump(void) const {
	printf( "camera parameters: \n"
		"	inwidth == %u\n"
		"	inheight == %u\n"
		"	rotation == %u\n"
		"	fps == %u\n"
		"	fourcc == %s\n"
		"	x == %u\n"
		"	y == %u\n"
		"	outwidth == %u\n"
		"	outheight == %u\n"
		"	transparency == %u\n"
		"	cameraDevName == %s\n"
		"	previewDevName == %s\n"
		"	saveFrame == %d\n"
		"	iterations == %d\n"
		, inwidth
		, inheight
		, rotation
		, fps
		, fourcc_str(fourcc)
		, x
		, y
		, outwidth
		, outheight
		, transparency
		, cameraDevName
		, previewDevName
		, saveFrame
		, iterations );
	if (0 != getBroadcastAddr()) {
                struct in_addr in ;
		in.s_addr = getBroadcastAddr();
		printf( "	broadcast to %s:0x%04x\n", inet_ntoa(in), getBroadcastPort());
	}
}
