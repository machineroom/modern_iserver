/*
 *	@(#)fix.c	1.1
 *
 *	fix.c
 *
 *	Transtech Iserver
 *
 *	Fixes for PC version with no networking
 *
 *	Copyright 1993 Transtech Parallel Systems
 */

#if (defined( MSDOS ) && defined( NOCOMMS ))

int gethostname( char *name, int namelen )
{
	return -1;
}

int sleep( int seconds )
{
	return 0;
}

#endif

