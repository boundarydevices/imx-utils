#ifndef __DEBUGPRINT_H__
#define __DEBUGPRINT_H__ "$Id: debugPrint.h,v 1.5 2008-04-01 18:55:27 ericn Exp $"

/*
 * debugPrint.h
 *
 * This header file declares the debugPrint() routine,
 * which is used to print debug information if the DEBUGPRINT
 * macro is set.
 *
 *
 * Change History : 
 *
 * $Log: debugPrint.h,v $
 * Revision 1.5  2008-04-01 18:55:27  ericn
 * -make NODEBUGPRINT easier
 *
 * Revision 1.4  2005/11/05 20:22:32  ericn
 * -fix compiler warnings
 *
 * Revision 1.3  2004/07/28 14:27:27  ericn
 * -prevent linking of empty debugPrint
 *
 * Revision 1.2  2004/07/04 21:33:16  ericn
 * -added debugHex() routine
 *
 * Revision 1.1  2003/11/24 19:42:42  ericn
 * -polling touch screen
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#ifdef ANDROID
	#include <utils/Log.h>
#else
	#include <stdarg.h>
#endif

inline int noDebugPrint( char const *, ... )
{
   return 0 ;
}

#ifdef DEBUGPRINT
    #include <stdio.h>
    #include <stdarg.h>

    #ifdef ANDROID
        #define debugPrint LOGD
    #else
        inline int debugPrint( char const *fmt, ... )
        {
           va_list ap;
           va_start( ap, fmt );
           return vfprintf( stdout, fmt, ap );
        }
    #endif
#else
    #define debugPrint noDebugPrint
#endif

#ifdef ANDROID
    #define ERRMSG LOGE
#else
    inline int errmsg( char const *fmt, ... )
    {
       va_list ap;
       va_start( ap, fmt );
       return vfprintf( stderr, fmt, ap );
    }
    #define ERRMSG errmsg
#endif

#endif

