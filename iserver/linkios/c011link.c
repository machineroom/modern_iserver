/*
 *	c011link.c
 *
 *	macihenroomfiddling@gmail.com
 */

#include <stdio.h>
#include <errno.h>
#include "linkio.h"
#include "linkops.h"
#include "c011.h"
#include <string.h>

typedef int LINK;

LINK
C011OpenLink(char *Name)
{
	c011_init();
    return 1;
}

int 
C011CloseLink(LINK LinkId)
{
    return SUCCEEDED;
}

int
C011ReadLink(LINK LinkId, char *Buffer, unsigned Count, int Timeout)
{
	int result;

	/* Read into buffer */
	result = c011_read_bytes( Buffer, Count, Timeout );
    return result;
}


int
C011WriteLink(LINK LinkId, char *Buffer, unsigned Count, int Timeout)
{
	int result;
	c011_write_bytes (Buffer, Count);

	return Count;
}

int
C011ResetLink(LINK LinkId)
{
    c011_reset();
	return 1;
}


int
C011AnalyseLink(LINK LinkId)
{
	return 1;
}


int
C011TestError(LINK LinkId)
{
    return 0;
}

int
C011TestRead(LINK LinkId)
{
	/* Not implemented */
	return (ER_LINK_CANT);
}

int
C011TestWrite(LINK LinkId)
{
	/* Not implemented */
	return (ER_LINK_CANT);
}



