/*
 *  This test file is used to verify that the header files associated with
 *  the callout are correct.
 *
 *  COPYRIGHT (c) 1989-1998. 1995, 1996.
 *  On-Line Applications Research Corporation (OAR).
 *  Copyright assigned to U.S. Government, 1994.
 *
 *  The license and distribution terms for this file may be
 *  found in the file LICENSE in this distribution or at
 *  http://www.OARcorp.com/rtems/license.html.
 *
 *  $Id$
 */

#include <signal.h>
 
void test( void )
{
  sigset_t        set;
  siginfo_t       info;
  struct timespec timeout;
  int             result;

  (void) sigemptyset( &set );

  result = sigtimedwait( &set, &info, &timeout );
}
