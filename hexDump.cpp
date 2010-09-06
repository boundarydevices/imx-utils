/*
 * Module hexDump.cpp
 *
 * This module defines the methods of the hexDumper_t
 * class as declared in hexDump.h
 *
 *
 * Change History : 
 *
 * $Log: hexDump.cpp,v $
 * Revision 1.4  2005-05-25 16:25:04  ericn
 * -added address spec
 *
 * Revision 1.3  2004/07/04 21:31:47  ericn
 * -added dumpHex method
 *
 * Revision 1.2  2004/03/17 04:56:19  ericn
 * -updates for mini-board (no sound, video, touch screen)
 *
 * Revision 1.1  2002/11/11 04:30:45  ericn
 * -moved from boundary1
 *
 * Revision 1.2  2002/11/03 04:29:39  ericn
 * -added stand-alone program
 *
 * Revision 1.1  2002/09/10 14:30:44  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "hexDump.h"
#include <stdio.h>

static const char hexChars[] = { 
   '0', '1', '2', '3',
   '4', '5', '6', '7',
   '8', '9', 'A', 'B',
   'C', 'D', 'E', 'F' 
};

static char *byteOut( char *nextIn, unsigned char b )
{
   *nextIn++ = hexChars[ b >> 4 ];
   *nextIn++ = hexChars[ b & 0x0f ];
   return nextIn ;
}

static char *longOut( char *nextIn, unsigned long v )
{
   for( unsigned i = 0 ; i < 4 ; i++ )
   {
      unsigned char const byte = v >> 24 ;
      v <<= 8 ;
      nextIn = byteOut( nextIn, byte );
   }

   return nextIn ;
}

bool hexDumper_t :: nextLine( void )
{
   if( 0 < bytesLeft_ )
   {
      char *next = longOut( lineBuf_, (unsigned long)addr_ );
      *next++ = ' ' ;
      *next++ = ' ' ;
      *next++ = ' ' ;

      unsigned lineBytes = ( 16 < bytesLeft_ ) ? 16 : bytesLeft_ ;
      unsigned char *bytes = (unsigned char *)data_ ;

      for( unsigned i = 0 ; i < lineBytes ; i++ )
      {
         next = byteOut( next, *bytes++ );
         *next++ = ' ' ;
         if( 7 == i )
         {
            *next++ = ' ' ;
            *next++ = ' ' ;
         }
      }
      
      for( unsigned i = lineBytes ; i < 16 ; i++ )
      {
         *next++ = ' ' ;
         *next++ = ' ' ;
         *next++ = ' ' ;
         if( 7 == i )
         {
            *next++ = ' ' ;
            *next++ = ' ' ;
         }
      }

      *next++ = ' ' ;
      *next++ = ' ' ;

      bytes = (unsigned char *)data_ ;
      for( unsigned i = 0 ; i < lineBytes ; i++ )
      {
         unsigned char c = *bytes++ ;
         if( ( ' ' <= c ) && ( '\x7f' > c ) )
            *next++ = c ;
         else
            *next++ = '.' ;
      }

      *next = 0 ;

      data_       = bytes ;
      addr_      += lineBytes ;
      bytesLeft_ -= lineBytes ;
      return true ;
   }
   else
      return false ;
}

void dumpHex( char const *label, void const *data, unsigned size )
{
   printf( "---> %s\n", label );
   hexDumper_t dump( data, size );
   while( dump.nextLine() )
      printf( "%s\n", dump.getLine() );
}

#ifdef __STANDALONE__
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <stdio.h>

int main( int argc, char const * const argv[] )
{
   if( 2 == argc )
   {
      struct stat st ;
      int const statResult = stat( argv[1], &st );
      if( 0 == statResult )
      {
         int fd = open( argv[1], O_RDONLY );
         if( 0 <= fd )
         {
            off_t const fileSize = lseek( fd, 0, SEEK_END );
printf( "fileSize: %u\n", fileSize );
            lseek( fd, 0, SEEK_SET );
            void *mem = mmap( 0, fileSize, PROT_READ, MAP_PRIVATE, fd, 0 );
            if( MAP_FAILED != mem )
            {
               hexDumper_t dump( mem, fileSize );
               
               while( dump.nextLine() )
                  printf( "%s\n", dump.getLine() );

               char inBuf[256];
               fgets( inBuf, sizeof(inBuf), stdin );

               munmap( mem, fileSize );
      
            } // mapped file
            else
               fprintf( stderr, "Error %m mapping %s\n", argv[1] );

            close( fd );
         }
         else
            fprintf( stderr, "Error %m opening %s\n", argv[1] );
      }
      else
         fprintf( stderr, "Error %m finding %s\n", argv[1] );
   }
   else
      fprintf( stderr, "Usage : hexDump fileName\n" );

   return 0 ;
}
#endif
