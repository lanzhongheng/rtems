/**
 * @file
 *
 * @brief Allocate and Initialize Per CPU Structures
 * @ingroup PerCPU
 */

/*
 *  COPYRIGHT (c) 1989-2011.
 *  On-Line Applications Research Corporation (OAR).
 *
 *  The license and distribution terms for this file may be
 *  found in the file LICENSE in this distribution or at
 *  http://www.rtems.com/license/LICENSE.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <rtems/score/percpu.h>
#include <rtems/score/assert.h>
#include <rtems/score/smpimpl.h>
#include <rtems/config.h>
#include <rtems/fatal.h>

#if defined(RTEMS_SMP)

static SMP_lock_Control _Per_CPU_State_lock = SMP_LOCK_INITIALIZER;

static void _Per_CPU_State_busy_wait(
  const Per_CPU_Control *per_cpu,
  Per_CPU_State new_state
)
{
  Per_CPU_State state = per_cpu->state;

  switch ( new_state ) {
    case PER_CPU_STATE_REQUEST_START_MULTITASKING:
      while (
        state != PER_CPU_STATE_READY_TO_START_MULTITASKING
          && state != PER_CPU_STATE_SHUTDOWN
      ) {
        _CPU_SMP_Processor_event_receive();
        state = per_cpu->state;
      }
      break;
    case PER_CPU_STATE_UP:
      while (
        state != PER_CPU_STATE_REQUEST_START_MULTITASKING
          && state != PER_CPU_STATE_SHUTDOWN
      ) {
        _CPU_SMP_Processor_event_receive();
        state = per_cpu->state;
      }
      break;
    default:
      /* No need to wait */
      break;
  }
}

static Per_CPU_State _Per_CPU_State_get_next(
  Per_CPU_State current_state,
  Per_CPU_State new_state
)
{
  switch ( current_state ) {
    case PER_CPU_STATE_INITIAL:
      switch ( new_state ) {
        case PER_CPU_STATE_READY_TO_START_MULTITASKING:
        case PER_CPU_STATE_SHUTDOWN:
          /* Change is acceptable */
          break;
        default:
          new_state = PER_CPU_STATE_SHUTDOWN;
          break;
      }
      break;
    case PER_CPU_STATE_READY_TO_START_MULTITASKING:
      switch ( new_state ) {
        case PER_CPU_STATE_REQUEST_START_MULTITASKING:
        case PER_CPU_STATE_SHUTDOWN:
          /* Change is acceptable */
          break;
        default:
          new_state = PER_CPU_STATE_SHUTDOWN;
          break;
      }
      break;
    case PER_CPU_STATE_REQUEST_START_MULTITASKING:
      switch ( new_state ) {
        case PER_CPU_STATE_UP:
        case PER_CPU_STATE_SHUTDOWN:
          /* Change is acceptable */
          break;
        default:
          new_state = PER_CPU_STATE_SHUTDOWN;
          break;
      }
      break;
    default:
      new_state = PER_CPU_STATE_SHUTDOWN;
      break;
  }

  return new_state;
}

void _Per_CPU_State_change(
  Per_CPU_Control *per_cpu,
  Per_CPU_State new_state
)
{
  SMP_lock_Control *lock = &_Per_CPU_State_lock;
  SMP_lock_Context lock_context;
  Per_CPU_State next_state;

  _Per_CPU_State_busy_wait( per_cpu, new_state );

  _SMP_lock_ISR_disable_and_acquire( lock, &lock_context );

  next_state = _Per_CPU_State_get_next( per_cpu->state, new_state );
  per_cpu->state = next_state;

  if ( next_state == PER_CPU_STATE_SHUTDOWN ) {
    uint32_t ncpus = rtems_configuration_get_maximum_processors();
    uint32_t cpu;

    for ( cpu = 0 ; cpu < ncpus ; ++cpu ) {
      Per_CPU_Control *other_cpu = _Per_CPU_Get_by_index( cpu );

      if ( per_cpu != other_cpu ) {
        switch ( other_cpu->state ) {
          case PER_CPU_STATE_UP:
            _SMP_Send_message( cpu, SMP_MESSAGE_SHUTDOWN );
            break;
          default:
            /* Nothing to do */
            break;
        }

        other_cpu->state = PER_CPU_STATE_SHUTDOWN;
      }
    }
  }

  _CPU_SMP_Processor_event_broadcast();

  _SMP_lock_Release_and_ISR_enable( lock, &lock_context );

  if (
    next_state == PER_CPU_STATE_SHUTDOWN
      && new_state != PER_CPU_STATE_SHUTDOWN
  ) {
    rtems_fatal( RTEMS_FATAL_SOURCE_SMP, SMP_FATAL_SHUTDOWN );
  }
}

#else
  /*
   * On single core systems, we can efficiently directly access a single
   * statically allocated per cpu structure.  And the fields are initialized
   * as individual elements just like it has always been done.
   */
  Per_CPU_Control_envelope _Per_CPU_Information[1];
#endif
