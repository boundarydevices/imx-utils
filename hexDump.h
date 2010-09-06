#ifndef __HEXDUMP_H__
#define __HEXDUMP_H__ "$Id: hexDump.h,v 1.3 2005-05-25 16:25:06 ericn Exp $"

/*
 * hexDump.h
 *
 * This header file declares the hexDumper_t 
 * class, which is used for displaying hunks of 
 * memory for examination.
 *
 *
 * Change History : 
 *
 * $Log: hexDump.h,v $
 * Revision 1.3  2005-05-25 16:25:06  ericn
 * -added address spec
 *
 * Revision 1.2  2004/07/04 21:31:45  ericn
 * -added dumpHex method
 *
 * Revision 1.1  2002/11/11 04:30:45  ericn
 * -moved from boundary1
 *
 * Revision 1.1  2002/09/10 14:30:44  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

class hexDumper_t {
public:
   hexDumper_t( void const   *data,
                unsigned long size,
                unsigned long addr = 0 )
      : data_( data ),
        addr_( addr ),
        bytesLeft_( size ){}

   //
   // returns true and fills in line if something left
   // use getLine() and getLineLength() to get the data
   //
   bool nextLine( void );

   char const *getLine( void ) const { return lineBuf_ ; }

private:
   void const     *data_ ;
   unsigned long   addr_ ;
   unsigned long   bytesLeft_ ;
   char            lineBuf_[ 81 ];
};

// dump to stdout
void dumpHex( char const *label, void const *data, unsigned size );

#endif

