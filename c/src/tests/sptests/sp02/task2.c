/*  Task_2
 *
 *  This routine serves as a test task.  It sleeps for 1 minute but
 *  does not expect to wake up.  Task 1 should suspend then delete it
 *  so that it appears to never wake up.
 *
 *  Input parameters:
 *    argument - task argument
 *
 *  Output parameters:  NONE
 *
 *  COPYRIGHT (c) 1989-1998.
 *  On-Line Applications Research Corporation (OAR).
 *  Copyright assigned to U.S. Government, 1994.
 *
 *  The license and distribution terms for this file may be
 *  found in the file LICENSE in this distribution or at
 *  http://www.OARcorp.com/rtems/license.html.
 *
 *  $Id$
 */

#include "system.h"

rtems_task Task_2(
  rtems_task_argument argument
)
{
  rtems_status_code status;

  puts( "TA2 - rtems_task_wake_after - sleep 1 minute" );
  status = rtems_task_wake_after( 60*TICKS_PER_SECOND );
  directive_failed( status, "rtems_task_wake_after in TA2" );
}
