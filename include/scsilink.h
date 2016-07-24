/*
 *	scsilink.h
 *
 *	Transtech SCSI Processor
 *
 *	Link driver header file
 *
 *	Copyright (c) 1993 Transtech Parallel Systems Limited
 */

#ifndef SCSILINK_H
#define SCSILINK_H

static char *ident_scsilink_h = "@(#)scsilink.h	1.17";

/* Standard SCSI commands */
#ifndef SCMD_TEST_UNIT_READY
#define SCMD_TEST_UNIT_READY		0x00	/* Test unit ready */
#define SCMD_REQUEST_SENSE		0x03	/* Request sense */
#define SCMD_RECEIVE			0x08	/* Receive */
#define SCMD_SEND			0x0a	/* Send */
#define SCMD_INQUIRY			0x12	/* Inquiry */
#define SCMD_SEND_DIAGNOSTIC		0x1d	/* Send diagnostic */
#define SCMD_WRITE_BUFFER		0x3b	/* Write buffer */
#define SCMD_READ_BUFFER		0x3c	/* Read buffer */
#endif

/* Vendor specific SCSI commands */
#define SCMD_READ_FLAGS			0x02	/* Read flags */
#define SCMD_WRITE_FLAGS		0x06	/* Write flags */
#define SCMD_READ_CONFIG		0x09	/* Read configuration link */
#define SCMD_WRITE_CONFIG		0x0c	/* Write configuration link */
#define SCMD_CONNECT			0x0d	/* Connect configuration */

/* Flags */
#define LFLAG_SUBSYSTEM_RESET		0x01	/* Subsystem reset */
#define LFLAG_SUBSYSTEM_ANALYSE		0x02	/* Subsystem analyse */
#define LFLAG_SUBSYSTEM_ERROR		0x04	/* Subsystem error */
#define LFLAG_RESET_LINK		0x10	/* Reset link */
#define LFLAG_RESET_CONFIG		0x20	/* Reset configuration link */

/* Link protocols */
#define LPROTO_RAW			0x00	/* Raw data protocol */
#define LPROTO_ISERVER			0x01	/* Iserver protocol */
#define LPROTO_BLOCK			0x02	/* Block protocol */
#define LPROTO_MAX			0x02	/* Maximum protocol number */

/* Other constants */
#define SCSILINK_BLOCK_SIZE		4096	/* Data block size */

#endif

