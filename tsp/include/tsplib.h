/*
 *	@(#)tsplib.h	1.9
 *
 *	tsplib.h
 *
 *	Transtech SCSI Processor Library
 *
 *	Library header file
 *
 *	Copyright (c) 1993 Transtech Parallel Systems Limited
 */

#ifndef TSPLIB_H
#define TSPLIB_H

#include <stddef.h>

/* Link protocols */
#define TSP_RAW_PROTOCOL		0	/* Raw byte mode */
#define TSP_ISERVER_PROTOCOL		1	/* Iserver protocol */
#define TSP_BLOCK_PROTOCOL		2	/* Fixed size blocks */
#define TSP_MAX_PROTOCOL		3	/* Maximum protocol number */

/* Connection commands */
#define TSP_CONNECT_HOST	((char) -1)	/* Host link connection */
#define TSP_CONNECT_DISK	((char) -2)	/* Disk link connection */
#define TSP_CONNECT_NEW		((char) -3)	/* New session */
#define TSP_CONNECT_HOST_DISK	((char) -4)	/* Host-Disk connection */

/* Library function prototypes */

/*
NAME
     tsp_open - Open a host link connection
SYNOPSIS
     #include <tsplib.h>
     int tsp_open( char *device );
ARGUMENTS
     device
          Name of host link to open.
DESCRIPTION
     A connection to  a  host  link  is  created  by  a  call  to
     tsp_open().
     The link to connect to is specified by the device  parameter
     which  has  the format cmtnlo where m is the SCSI controller
     (or bus) used, n is the target SCSI id, and o  is  the  link
     number (0 - 3).  For example, c0t5l0 specifies link 0 on the
     target with id 5 on SCSI bus 0.

     For Linux port device is now sgX where X is the host output link.
     The host input link is assumed ot be X+4     
     
RETURN VALUES
     Returns a non-negative descriptor on success or -1 on error.
*/
int tsp_open( char *device );

/*
NAME
     tsp_close - Close a host link connection
SYNOPSIS
     #include <tsplib.h>
     int tsp_close( int fd );
ARGUMENTS
     fd   Descriptor returned by tsp_open().
DESCRIPTION
     A call to tsp_close() closes a connection  to  a  host  link
     that was created by a call to tsp_open().
RETURN VALUES
     Returns zero on success or -1 on error
*/
int tsp_close( int fd );

/*
NAME
     tsp_reset - Reset a host link connection
SYNOPSIS
     #include <tsplib.h>
     int tsp_reset( int fd );
ARGUMENTS
     fd   Descriptor returned by tsp_open().
DESCRIPTION
     A call to tsp_reset() resets the link and connected  devices
     of the host link connection specified by fd.
RETURN VALUES
     Returns zero on success or -1 on error.
*/     
int tsp_reset( int fd );

/*
NAME
     tsp_analyse - Reset a host link connection with analyse
SYNOPSIS
     #include <tsplib.h>
     int tsp_analyse( int fd );
ARGUMENTS
     fd   Descriptor returned by tsp_open().
DESCRIPTION
     A call to tsp_analyse() resets the link  and  (with  analyse
     asserted)  the connected devices of the host link connection
     specified by fd.
RETURN VALUES
     Returns zero on success or -1 on error.
*/
int tsp_analyse( int fd );

/*
NAME
     tsp_error - Return host link connection error status
SYNOPSIS
     #include <tsplib.h>
     int tsp_error( int fd );
ARGUMENTS
     fd   Descriptor returned by tsp_open().
DESCRIPTION
     A call to tsp_error() returns the state of the error line of
     the host link connection specified by fd.
RETURN VALUES
     Returns zero or one indicating the state of the  error  line
     on success or -1 on error.
*/
int tsp_error( int fd );

/*
NAME
     tsp_read - Read data from a host link
SYNOPSIS
     #include <tsplib.h>
     int tsp_read( int fd, void *data, size_t length,
      int timeout );
ARGUMENTS
     fd   Descriptor returned by tsp_open().
     data Pointer to memory into which data is read.
     length
          The maximum amount of data to be read.
     timeout
          Timeout in seconds.  Zero for no timeout.
DESCRIPTION
     A call to tsp_read() requests  the  target  to  read  up  to
     length bytes of data from the host link.
     By default a raw link protocol is used in which  the  target
     attempts to input length bytes of data (in 4096 byte blocks)
     until a timeout occurs.  If iserver link protocol  is  used,
     an  iserver packet of total length of up to length (or 4096)
     bytes containing a two byte count and count bytes of data is
     input.   In  block link protocol mode, an attempt is made to
     read a block of data of length length.
     The  link   protocol   and   block   size   is   set   using
     tsp_protocol().   In protocol modes other than raw mode, the
     length of data requested must be the same as the block  size
     specified by the function tsp_protocol().
     If a timeout occurs during a read, it is an error to  subse-
     quently  request  a  lesser  amount of data until either the
     original request is satisfied or  until  the  connection  is
     reset using tsp_reset().
RETURN VALUES
     Returns the number of bytes read (which  may  be  less  than
     that requested) on success or -1 on error.
*/
int tsp_read( int fd, void *data, size_t length, int timeout );

/*
NAME
     tsp_write - Write data to a host link
SYNOPSIS
     #include <tsplib.h>
     int tsp_write( int fd, void *data, size_t length,
      int timeout );
ARGUMENTS
     fd   Descriptor returned by tsp_open().
     data Pointer to data to write.
     length
          The number of bytes to write.
     timeout
          Timeout in seconds.  Zero for no timeout.
DESCRIPTION
     A call to tsp_write() sends length bytes of data to be  out-
     put on a host link.
     If a write timeout occurs, it is an error to attempt further
     writes  until  the  connection  is  reset using tsp_reset(),
     tsp_analyse() or tsp_reset_config().
     Note that as write buffers are used  to  increase  the  host
     link  bandwidth,  then  tsp_write() may return without error
     before the data is actually output on the host link.  If the
     data cannot be output on the host link then subsequent calls
     to tsp_write() will return an error.
RETURN VALUES
     Returns the number of bytes written  on  success  or  -1  on
     timeout or other error
BUGS
     There is a built in maximum timeout of five seconds.
*/     
int tsp_write( int fd, void *data, size_t length, int timeout );

/*
ARGUMENTS
     fd:         Descriptor returned by tsp_open().
     protocol:   Link protocol.
     block_size: Block size
     The supported values of protocol are:
     TSP_RAW_PROTOCOL
          Raw byte mode protocol.
     TSP_ISERVER_PROTOCOL
          Iserver mode protocol.
     TSP_BLOCK_PROTOCOL
          Fixed block size protocol.
     The maximum value of block_size is 4096.
DESCRIPTION
     A call to tsp_protocol() sets the  link  protocol  used  for
     host link reads using tsp_read().  This allows the target to
     attempt to perform reads before  they  are  requested  using
     tsp_read() thus improving link bandwidth.
     With raw byte mode protocol, read request lengths correspond
     to  the  number  of  bytes of data to receive.  The value of
     block_size is ignored.
     With iserver protocol, read request lengths must be  set  to
     the value of block_size and correspond to the maximum number
     of bytes to receive in the form of an iserver  packet  which
     consists  of  a  two  byte  count followed by count bytes of
     data.
     In fixed block size  protocol,  all  reads  must  of  length
     block_size.
     The link protocol is set to raw byte mode when the host link
     is   reset  by  a  call  to  tsp_reset(),  tsp_analyse()  or
     tsp_reset_config().  After being set by tsp_protocol()  this
     protocol persists until the link is reset.
RETURN VALUES
     Returns zero on success or -1 on error.
*/
int tsp_protocol( int fd, int protocol, int block_size );
int tsp_reset_config( int fd );
int tsp_read_config( int fd, void *data, size_t length, int timeout );
int tsp_write_config( int fd, void *data, size_t length, int timeout );
int tsp_connect( int fd, char n1, char l1, char n2, char l2 );
int tsp_mknod( char *device );

#endif

