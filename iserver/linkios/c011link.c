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
#include <unistd.h>

typedef int LINK;

LINK
C011OpenLink(char *Name)
{
	c011_init();
	c011_set_byte_mode();
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
	// 100 10ths of a second in a millisecond
	result = c011_read_bytes( Buffer, Count, Timeout*100 );
	if (result >= 0) {
	    return result;
	} else {
	    return ER_LINK_CANT;
	}
}

int
C011WriteLink(LINK LinkId, char *Buffer, unsigned Count, int Timeout)
{
	int result;
	// 100 10ths of a second in a millisecond
	result = c011_write_bytes (Buffer, Count, Timeout * 100);
	if (result >= 0) {
	    return result;
	} else {
	    return ER_LINK_CANT;
	}
}

int
C011ResetLink(LINK LinkId)
{
	c011_reset();
	sleep(2);	//WX reset delay
    return SUCCEEDED;
}


int
C011AnalyseLink(LINK LinkId)
{
    c011_analyse ();
    return SUCCEEDED;
}


int
C011TestError(LINK LinkId)
{
    return c011_read_error_pin();
}

int
C011TestRead(LINK LinkId)
{
    return ((c011_read_input_status() & 0x01) == 0x01);
}

int
C011TestWrite(LINK LinkId)
{
    return ((c011_read_output_status() & 0x01) == 0x01);
}



