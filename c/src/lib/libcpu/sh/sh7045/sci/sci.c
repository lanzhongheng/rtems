/*
 * /dev/sci[0|1] for Hitachi SH 704X
 *
 * The SH doesn't have a designated console device. Therefore we "alias" 
 * another device as /dev/console and revector all calls to /dev/console 
 * to this device.
 *
 * This approach is similar to installing a sym-link from one device to 
 * another device. If rtems once will support sym-links for devices files, 
 * this implementation could be dropped.
 *
 *  Author: Ralf Corsepius (corsepiu@faw.uni-ulm.de)
 *
 *  COPYRIGHT (c) 1997-1998, FAW Ulm, Germany
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 *
 *  COPYRIGHT (c) 1998.
 *  On-Line Applications Research Corporation (OAR).
 *  Copyright assigned to U.S. Government, 1994.
 *
 *  The license and distribution terms for this file may be
 *  found in the file LICENSE in this distribution or at
 *  http://www.OARcorp.com/rtems/license.html.
 *
 *  Modified to reflect sh7045 processor:
 *  John M. Mills (jmills@tga.com)
 *  TGA Technologies, Inc.
 *  100 Pinnacle Way, Suite 140
 *  Norcross, GA 30071 U.S.A.
 *
 *  This modified file may be copied and distributed in accordance
 *  the above-referenced license. It is provided for critique and
 *  developmental purposes without any warranty nor representation
 *  by the authors or by TGA Technologies.
 *
 *  $Id$
 */

#include <rtems.h>

#include <stdlib.h>

#include <rtems/libio.h>
#include <iosupp.h>
#include <rtems/score/sh_io.h>
#include <rtems/score/ispsh7045.h>
#include <rtems/score/iosh7045.h>
#include <sh/sh7_sci.h>
#include <sh/sh7_pfc.h>

#include <sh/sci.h>

#ifndef STANDALONE_EVB
#define STANDALONE_EVB 0
#endif

/*
 * NOTE: Some SH variants have 3 sci devices
 */
  
#define SCI_MINOR_DEVICES       2
  
/*
 * FIXME: sh7045 register names match Hitachi data book,
 *  but conflict with RTEMS sh7032 usage.
 */

#define SH_SCI_BASE_0   SCI_SMR0
#define SH_SCI_BASE_1   SCI_SMR1

#define SH_SCI_DEF_COMM_0   B9600 | CS8
#define SH_SCI_DEF_COMM_1   B38400 | CS8
/*  #define SH_SCI_DEF_COMM_1   B9600 | CS8 */

struct scidev_t {
  char *			name ;
  unsigned32			addr ;
  rtems_device_minor_number	minor ;
  unsigned short		opened ;
  tcflag_t			cflags ;
} sci_device[SCI_MINOR_DEVICES] =
{
  { "/dev/sci0", SH_SCI_BASE_0, 0, 0, SH_SCI_DEF_COMM_0 },
  { "/dev/sci1", SH_SCI_BASE_1, 1, 0, SH_SCI_DEF_COMM_1 }
} ;

/*  local data structures maintain hardware configuration */
#if UNUSED
static sci_setup_t sio_param[2];
#endif

/*  imported from scitab.rel */
extern int _sci_get_brparms(
  tcflag_t      cflag,
  unsigned char *smr,
  unsigned char *brr );

/* Translate termios' tcflag_t into sci settings */
static int _sci_set_cflags(
  struct scidev_t      *sci_dev,
  tcflag_t      c_cflag )
{
  unsigned8	smr ;
  unsigned8	brr ;
  
  if ( c_cflag & CBAUD )
  {
    if ( _sci_get_brparms( c_cflag, &smr, &brr ) != 0 )
      return -1 ;
  }
                    
  if ( c_cflag & CSIZE )
  {
    if ( c_cflag & CS8 )
      smr &= ~SCI_SEVEN_BIT_DATA;
    else if ( c_cflag & CS7 )
      smr |= SCI_SEVEN_BIT_DATA;
    else
      return -1 ;
  }

  if ( c_cflag & CSTOPB )
    smr |= SCI_STOP_BITS_2;
  else
    smr &= ~SCI_STOP_BITS_2;

  if ( c_cflag & PARENB )
    smr |= SCI_PARITY_ON ;
  else
    smr &= ~SCI_PARITY_ON ;

  if ( c_cflag & PARODD )
    smr |= SCI_ODD_PARITY ;
  else
    smr &= ~SCI_ODD_PARITY;
    
  write8( smr, sci_dev->addr + SCI_SMR );
  write8( brr, sci_dev->addr + SCI_BRR );
  
  return 0 ;
}

/*
 * local functions operate SCI ports 0 and 1
 * called from polling routines or ISRs
 */
rtems_boolean wrtSCI0(unsigned char ch)
{
  unsigned8 temp;
  rtems_boolean result=FALSE;

  if ((read8(SCI_SSR0) & SCI_TDRE) != 0x00) {
    /* Write the character to the TDR */
    write8(ch, SCI_TDR0);
    /* Clear the TDRE bit */
    temp = read8(SCI_SSR0) & ~SCI_TDRE;
    write8(temp, SCI_SSR0);
    result = TRUE;
  }
  return result;
} /* wrtSCI0 */

rtems_boolean wrtSCI1(unsigned char ch)
{
  unsigned8 temp;
  rtems_boolean result=FALSE;

  if ((read8(SCI_SSR1) & SCI_TDRE) != 0x00) {
     /* Write the character to the TDR */
     write8(ch, SCI_TDR1);
     /* Clear the TDRE bit */
     temp = read8(SCI_SSR1) & ~SCI_TDRE;
     write8(temp, SCI_SSR1);
     result = TRUE;
  }
  return result;
} /* wrtSCI1 */

/* polled output steers byte to selected port */
void sh_sci_outbyte_polled(
  rtems_device_minor_number  minor,
  char ch )
{
	if (minor == 0) /* blocks until port ready */
		while (wrtSCI0(ch) != TRUE); /* SCI0*/
	else
		while (wrtSCI1(ch) != TRUE); /* SCI1*/
} /* sh_sci_outbyte_polled */

/*
 * Initial version calls polled output driver and blocks
 */
void outbyte(
  rtems_device_minor_number  minor,
  char ch)
{
	sh_sci_outbyte_polled(minor, (unsigned char)ch);
} /* outbyte */

rtems_boolean rdSCI0(unsigned char *ch)
{
  unsigned8 temp;
  rtems_boolean result=FALSE;

  if ((read8(SCI_SSR0) & SCI_RDRF) != 0x00) {
    /* read input */
    *ch = read8(SCI_RDR0);
    /* Clear RDRF flag */
    temp = read8(SCI_SSR0) & ~SCI_RDRF;
    write8(temp, SCI_SSR0);
    result = TRUE;
  }
  return result;
} /* rdSCI0 */

rtems_boolean rdSCI1(unsigned char *ch)
{
  unsigned8 temp;
  rtems_boolean result=FALSE;

  if ((read8(SCI_SSR1) & SCI_RDRF) != 0x00) {
    /* read input */
    *ch = read8(SCI_RDR1);
    /* Clear RDRF flag */
    temp= read8(SCI_SSR1) & ~SCI_RDRF;
    write8(temp, SCI_SSR1);
    result = TRUE;
    }
  return result;
} /* rdSCI1 */


/* initial version pulls byte from selected port */
char sh_sci_inbyte_polled(
    rtems_device_minor_number  minor )
{
	char ch;
	
	if (minor == 0) /* blocks until char.ready */
		while (rdSCI0(&ch) != TRUE); /* SCI0 */
	else
		while (rdSCI1(&ch) != TRUE); /* SCI1 */
	return ch;
} /* sh_sci_inbyte_polled */

/* Initial version calls polled input driver */
char inbyte(
  rtems_device_minor_number  minor )
{
	char ch;

	ch = sh_sci_inbyte_polled(minor);
	return ch;
} /* inbyte */


/*  sh_sci_initialize
 *
 *  This routine initializes (registers) the sh_sci IO drivers.
 *
 *  Input parameters: ignored
 *
 *  Output parameters:  NONE
 *
 *  Return values: RTEMS_SUCCESSFUL
 *   if all sci[...] register, else calls
 *   rtems_fatal_error_occurred(status)
 *
 */

rtems_device_driver sh_sci_initialize(
  rtems_device_major_number  major,
  rtems_device_minor_number  minor,
  void                      *arg )
{
  rtems_device_driver status ;
  rtems_device_minor_number     i;
  
  /*
   * register all possible devices.
   * the initialization of the hardware is done by sci_open
   */

  for ( i = 0 ; i < SCI_MINOR_DEVICES ; i++ )
  {
    status = rtems_io_register_name(
      sci_device[i].name,
      major,
      sci_device[i].minor );
    if (status != RTEMS_SUCCESSFUL)
      rtems_fatal_error_occurred(status);
  }

  /* non-default hardware setup occurs in sh_sci_open() */

  return RTEMS_SUCCESSFUL;
}


/*
 *  Open entry point
 *   Sets up port and pins for selected sci.
 *   SCI0 setup is conditional on STANDALONE_EVB == 1
 */

rtems_device_driver sh_sci_open(
  rtems_device_major_number major,
  rtems_device_minor_number minor,
  void                    * arg )
{
  unsigned8 temp8;
  unsigned16 temp16;
  
  unsigned 	a ;
  
 /* check for valid minor number */
   if(( minor > ( SCI_MINOR_DEVICES -1 )) || ( minor < 0 ))
   {
     return RTEMS_INVALID_NUMBER;
   }
 
  /* device already opened */
  if ( sci_device[minor].opened > 0 )
  {
    sci_device[minor].opened++ ;
    return RTEMS_SUCCESSFUL ;
  }
    
  /* set PFC registers to enable I/O pins */

  if ((minor == 0) && (STANDALONE_EVB == 1)) {
    temp16 = read16(PFC_PACRL2);         /* disable SCK0, DMA, IRQ */
    temp16 &= ~(PA2MD1 | PA2MD0);
    temp16 |= (PA_TXD0 | PA_RXD0);       /* enable pins for Tx0, Rx0 */
    write16(temp16, PFC_PACRL2);
    
  } else if (minor == 1) {  
    temp16 = read16(PFC_PACRL2);          /* disable SCK1, DMA, IRQ */
    temp16 &= ~(PA5MD1 | PA5MD0);
    temp16 |= (PA_TXD1 | PA_RXD1);        /* enable pins for Tx1, Rx1 */
    write16(temp16, PFC_PACRL2);

  } /* add other devices and pins as req'd. */

  /* set up SCI registers */
  if ((minor != 0) || (STANDALONE_EVB == 1)) {
    write8(0x00, sci_device[minor].addr + SCI_SCR);	 /* Clear SCR */
                                                   /* set SMR and BRR */
    _sci_set_cflags( &sci_device[minor], sci_device[minor].cflags );

    for(a=0; a < 10000L; a++) {                      /* Delay */
      asm volatile ("nop");
    }

    write8((SCI_RE | SCI_TE),              /* enable async. Tx and Rx */
	   sci_device[minor].addr + SCI_SCR);
    temp8 = read8(sci_device[minor].addr + SCI_RDR);   /* flush input */
    /* Clear RDRF flag */
    temp8= read8(sci_device[minor].addr + SCI_SSR) & ~SCI_RDRF;
    write8(temp8, sci_device[minor].addr + SCI_SSR);
    write8(0x00, sci_device[minor].addr + SCI_TDR);    /* force output */
     /* Clear the TDRE bit */
     temp8 = read8(sci_device[minor].addr + SCI_SSR) & ~SCI_TDRE;
     write8(temp8, sci_device[minor].addr + SCI_SSR);
    
    /* add interrupt setup if required */

  }

  sci_device[minor].opened++ ;

  return RTEMS_SUCCESSFUL ;
}
 
/*
 *  Close entry point
 */

rtems_device_driver sh_sci_close(
  rtems_device_major_number major,
  rtems_device_minor_number minor,
  void                    * arg
)
{
  /* FIXME: Incomplete */
  if ( sci_device[minor].opened > 0 )
    sci_device[minor].opened-- ;
  else
    return RTEMS_INVALID_NUMBER ;
    
  return RTEMS_SUCCESSFUL ;
}

/*
 * read bytes from the serial port. We only have stdin.
 */

rtems_device_driver sh_sci_read(
  rtems_device_major_number major,
  rtems_device_minor_number minor,
  void                    * arg
)
{
  rtems_libio_rw_args_t *rw_args;
  char *buffer;
  int maximum;
  int count = 0;
 
  rw_args = (rtems_libio_rw_args_t *) arg;

  buffer = rw_args->buffer;
  maximum = rw_args->count;

  for (count = 0; count < maximum; count++) {
    buffer[ count ] = inbyte(minor);
    if (buffer[ count ] == '\n' || buffer[ count ] == '\r') {
      buffer[ count++ ]  = '\n';
      break;
    }
  }

  rw_args->bytes_moved = count;
  return (count >= 0) ? RTEMS_SUCCESSFUL : RTEMS_UNSATISFIED;
}

/*
 * write bytes to the serial port. Stdout and stderr are the same. 
 */

rtems_device_driver sh_sci_write(
  rtems_device_major_number major,
  rtems_device_minor_number minor,
  void                    * arg
)
{
  int count;
  int maximum;
  rtems_libio_rw_args_t *rw_args;
  char *buffer;

  rw_args = (rtems_libio_rw_args_t *) arg;

  buffer = rw_args->buffer;
  maximum = rw_args->count;

  for (count = 0; count < maximum; count++) {
    if ( buffer[ count ] == '\n') {
      outbyte(minor, '\r');
    }
    outbyte( minor, buffer[ count ] );
  }

  rw_args->bytes_moved = maximum;
  return 0;
}

/*
 *  IO Control entry point
 */

rtems_device_driver sh_sci_control(
  rtems_device_major_number major,
  rtems_device_minor_number minor,
  void                    * arg
)
{
  /* Not yet supported */
  return RTEMS_SUCCESSFUL ;
}
