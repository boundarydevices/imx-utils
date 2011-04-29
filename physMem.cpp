/*
 * Module physMem.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: physMem.cpp,v $
 * Revision 1.2  2009-05-14 16:28:16  ericn
 * [physMem] Ensure munmap in destructor
 *
 * Revision 1.1  2008-06-25 01:20:47  ericn
 * -import
 *
 *
 * Copyright Boundary Devices, Inc. 2007
 */


#include "physMem.h"
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>

#define MAP_SHIFT 12
#define MAP_SIZE (1<<MAP_SHIFT)
#define MAP_MASK ( MAP_SIZE - 1 )

physMem_t::physMem_t( unsigned long physAddr, unsigned long size, unsigned long mode )
   : fd_( open( "/dev/mem", mode ) )
   , map_(0)
   , mem_(0)
   , mapSize_(0)
{
   if( !worked() )
      return ;
   unsigned mapMode = (O_RDONLY == mode)
                      ? PROT_READ
                      : PROT_READ | PROT_WRITE ;
   unsigned startPage = physAddr >> MAP_SHIFT ;
   unsigned lastPage = (physAddr+size-1) >> MAP_SHIFT ;
   unsigned mapSize = (lastPage-startPage+1)<<MAP_SHIFT ;

   map_ = mmap(0, mapSize, mapMode, MAP_SHARED, fd_, physAddr & ~MAP_MASK );
   if( MAP_FAILED == map_ ){
      map_ = 0 ;
      return ;
   }
   mem_ = (unsigned char *)map_ + (physAddr & MAP_MASK);
   mapSize_ = mapSize ;
}

physMem_t::~physMem_t( void )
{
      if( mem_ ){
         munmap(mem_, mapSize_);
         mem_ = 0 ;
      }
      if( 0 <= fd_ ){
         close(fd_);
         fd_ = -1 ;
      }
}

void physMem_t::invalidate(void){
	int rval = msync(mem_,mapSize_, MS_SYNC|MS_INVALIDATE);
	if (0 != rval)
		perror("msync");
}

#ifdef STANDALONE
#include <stdio.h>
#include <stdlib.h>
#include "hexDump.h"
#include <ctype.h>

static bool deposit = 0 ;
static bool binary = 0 ;
static unsigned long value = 0 ;

static void parseArgs( int &argc, char const **argv )
{
	for( unsigned arg = 1 ; arg < argc ; arg++ ){
		if( '-' == *argv[arg] ){
			char const *param = argv[arg]+1 ;
			if( 'd' == tolower(*param) ){
				deposit = true ;
				value = strtoul(param+1,0,16);
			} else if( 'b' == tolower(*param) ){
				binary = true ;
				fflush(stdout);
			}
			else
				printf( "unknown option %s\n", param );

			// pull from argument list
			for( int j = arg+1 ; j < argc ; j++ ){
				argv[j-1] = argv[j];
			}
			--arg ;
			--argc ;
		}
	}
}

int main( int argc, char const *argv[] )
{
   parseArgs(argc,argv);
   if( 1 < argc ){
      unsigned long address = strtoul( argv[1], 0, 0 );
      unsigned length ;
      if( 2 < argc )
         length = strtoul( argv[2], 0, 0 );
      else
         length = 512 ;

      physMem_t phys(address, length, deposit ? O_RDWR : O_RDONLY );
      if( phys.worked() )
      {
         if( deposit ){
            unsigned long *longs = (unsigned long *)phys.ptr();
            printf( "depositing 0x%08lx\n", value );
            while (0 < length) {
               *longs++ = value ;
               length -= sizeof(*longs);
            }
         }
	 if(!binary){
		 hexDumper_t dump( phys.ptr(), length, address );
		 while( dump.nextLine() )
		    printf( "%s\n", dump.getLine() );
	 }
	 else {
		 write(1, phys.ptr(), length);
		 fflush(stdout);
	 }
      }
      else
         perror( "map" );
   }
   else
      fprintf( stderr, "Usage: %s address [length=512]\n", argv[0] );
   return 0 ;
}
#endif
