/*
 *	tsplink.c
 *
 *	Transtech SCSI Processor
 *
 *	Link interface
 *
 *	Copyright (c) 1993 Transtech Parallel Systems Limited
 */

static char *ident = "@(#)tsplink.c	1.11";

#include <stdio.h>
#include <errno.h>
#include "linkio.h"
#include "linkops.h"
#include "tsplib.h"

#define NULL_LINK          -1

#define TRUE               1
#define FALSE              0

typedef int LINK;
typedef char BYTE;

static LINK     ActiveLink = NULL_LINK;
static BYTE	ReadBuffer[4096];
static int	BytesBuffered = 0;
static int	BytePointer = 0;

static enum CommsModes LinkMode = COMMSclosed;

#define OK 0
#define NOT_OK -1

static int SetProtocol( LINK );

LINK 
TSPOpenLink(Name)
	BYTE           *Name;
{
	/* Already open ? */
	if (ActiveLink != NULL_LINK)
		return (ER_LINK_CANT);

	/* Find devices to open */
	if ((Name == NULL) || (*Name == 0))
		return (ER_NO_LINK);

	/* Open device */
	ActiveLink = tsp_open( Name );
	if (ActiveLink < 0)
	{
		ActiveLink = NULL_LINK;

		return ER_LINK_CANT;
	}

	/* Set link mode */
	LinkMode = COMMSsynchronous;

	return ActiveLink;
}


/* CloseLink --- close down the link connection */

int TSPCloseLink(LinkId)
LINK            LinkId;
{
	if (LinkId != ActiveLink)
		return (ER_LINK_BAD);

	tsp_close( LinkId );

	ActiveLink = NULL_LINK;

	/* Set link mode */
	LinkMode = COMMSclosed;

	return (SUCCEEDED);
}


/* ReadLink --- */

int
TSPReadLink(LinkId, Buffer, Count, Timeout)
	LINK            LinkId;
	char           *Buffer;
	unsigned int    Count;
	int             Timeout;
{
	int result;

	if (LinkId != ActiveLink)
		return (ER_LINK_BAD);

	if (Count < 1)
		return (ER_LINK_CANT);

	/* Set iserver protocol */
	SetProtocol( LinkId );

	/* Act on protocol */
	switch (LinkMode)
	{
		case COMMSclosed:
			return (ER_LINK_CANT);

		case COMMSsynchronous:
			/* Synchronous (raw) protocol */
			result = tsp_read( LinkId, Buffer, Count,
			 (Timeout + 9)/10 );
			if (result < 0)
				return ER_LINK_BAD;

			return result;

		case COMMSasynchronous:
			/* Continue below */
			break;
	}

	/* Asynchronous (iserver) protocol */
	if (BytesBuffered == 0)
	{
		BytePointer = 0;

		/* Read into buffer */
		result = tsp_read( LinkId, ReadBuffer, sizeof( ReadBuffer ),
		 (Timeout + 9)/10 );

		if (result < 0)
			return ER_LINK_CANT;
		else if (result == 0)
			return result;
		else if (result < Count)
			return ER_LINK_SYNTAX;
		else if (result == Count)
		{
			/* Read all bytes requested */
			memcpy( Buffer, ReadBuffer, Count );
			return Count;
		}
		else
		{
			/* Read more bytes than requested */
			BytesBuffered = result - Count;
			BytePointer = Count;
			memcpy( Buffer, ReadBuffer, Count );
			return Count;
		}
	}
	else if (Count >= BytesBuffered)
	{
		/* Requested at least all bytes buffered */
		memcpy( Buffer, ReadBuffer + BytePointer, BytesBuffered );
		result = BytesBuffered;
		BytesBuffered = 0;
		BytePointer = 0;
		return result;
	}
	else
	{
		/* Got more than enough bytes buffered */
		memcpy( Buffer, ReadBuffer + BytePointer, Count );
		BytesBuffered -= Count;
		BytePointer += Count;
		return Count;
	}
}


/* WriteLink --- */

int
TSPWriteLink(LinkId, Buffer, Count, Timeout)
	LINK            LinkId;
	char           *Buffer;
	unsigned int    Count;
	int             Timeout;
{
	int result;

	if (LinkId != ActiveLink)
		return (ER_LINK_BAD);

	if (Count < 1)
		return (ER_LINK_CANT);

	result = tsp_write( LinkId, Buffer, Count, (Timeout + 9)/10 );
	if (result < 0)
		return ER_LINK_BAD;

	return result;
}

/* ResetLink --- */

int
TSPResetLink(LinkId)
	LINK            LinkId;
{
	if (LinkId != ActiveLink)
		return (ER_LINK_BAD);

	/* Reset link mode */
	LinkMode = COMMSsynchronous;

	/* Perform reset */
	if (tsp_reset( LinkId ))
		return ER_LINK_CANT;

	return TRUE;
}


/* AnalyseLink --- */

int
TSPAnalyseLink(LinkId)
	LINK            LinkId;
{
	if (LinkId != ActiveLink)
		return (ER_LINK_BAD);

	/* Reset link mode */
	LinkMode = COMMSsynchronous;

	/* Perform analyse */
	if (tsp_analyse( LinkId ))
		return ER_LINK_CANT;

	return TRUE;
}


/* TestError --- */

int
TSPTestError(LinkId)
	LINK            LinkId;
{
	int result;

	if (LinkId != ActiveLink)
		return (ER_LINK_BAD);

	result = tsp_error( LinkId );

	if (result < 0)
		return -1;
	else if (result)
		return 1;
	else
		return 0;
}


/* TestRead --- */

int
TSPTestRead(LinkId)
	LINK            LinkId;
{
	/* Not implemented */
	return (ER_LINK_CANT);
}


/* TestWrite --- */

int
TSPTestWrite(LinkId)
	LINK            LinkId;
{
	/* Not implemented */
	return (ER_LINK_CANT);
}


/* SetProtocol */

static int
SetProtocol(LinkId)
	LINK            LinkId;
{
	int protocol;

	if (LinkId != ActiveLink)
		return (ER_LINK_BAD);

	/* Return if protocol not changed to iserver */
	if (OPScommsmode == LinkMode)
		return 1;

	/* Select new protocol */
	switch (OPScommsmode)
	{
		case COMMSsynchronous:
			/* Raw protocol */
			protocol = TSP_RAW_PROTOCOL;

			break;

		case COMMSasynchronous:
			/* Iserver protocol */
			protocol = TSP_ISERVER_PROTOCOL;

			break;

		default:
			LinkMode = COMMSsynchronous;

			return -1;
	}

	/* Set protocol */
	if (tsp_protocol( LinkId, protocol, sizeof( ReadBuffer ) ))
	{
		LinkMode = COMMSsynchronous;

		return -1;
	}

	LinkMode = OPScommsmode;

	/* Reset read buffer */
	BytesBuffered = 0;
	BytePointer = 0;

	return 1;
}

