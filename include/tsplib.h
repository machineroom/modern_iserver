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
int tsp_open( char *device );
int tsp_close( int fd );
int tsp_reset( int fd );
int tsp_analyse( int fd );
int tsp_error( int fd );
int tsp_read( int fd, void *data, size_t length, int timeout );
int tsp_write( int fd, void *data, size_t length, int timeout );
int tsp_protocol( int fd, int protocol, int block_size );
int tsp_reset_config( int fd );
int tsp_read_config( int fd, void *data, size_t length, int timeout );
int tsp_write_config( int fd, void *data, size_t length, int timeout );
int tsp_connect( int fd, char n1, char l1, char n2, char l2 );
int tsp_mknod( char *device );

#endif

