#ifndef __PHYSMEM_H__
#define __PHYSMEM_H__ "$Id: physMem.h,v 1.2 2009-05-14 16:28:16 ericn Exp $"

/*
 * physMem.h
 *
 * This header file declares the physMem_t class,
 * which can be used to read [and write] a section
 * of physical memory (use with care!).
 *
 * Change History : 
 *
 * $Log: physMem.h,v $
 * Revision 1.2  2009-05-14 16:28:16  ericn
 * [physMem] Ensure munmap in destructor
 *
 * Revision 1.1  2008-06-25 01:20:47  ericn
 * -import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2007
 */

#include <fcntl.h>

class physMem_t {
public:
   physMem_t( unsigned long physAddr, unsigned long size, unsigned long mode = O_RDONLY );
   ~physMem_t( void );

   bool worked() const { return 0 <= fd_ ; }

   void *ptr() const { return mem_ ; }

   void invalidate();

private:
   int   fd_ ;
   void *map_ ;   // start of page
   void *mem_ ;   // start of data
   unsigned mapSize_ ;
};

#endif

