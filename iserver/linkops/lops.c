/*------------------------------------------------------------------------------
--
-- File    : lops.c
--     linkops functions implemented using (meta) linkio library
--
-- Copyright 1990, 1991 INMOS Limited
--
-- Date    : 4/11/91
-- Version : 1.3
--
-- 05/07/90 - dave edwards
--   originated
-- 04/10/90 - dave edwards
--   updated to return error messages into lnkops_errorstring rather
--     than print out
-- 08/11/90 - dave edwards
--   vaxified
-- 05/02/91 - dave edwards
--   added localmethod parameter to LOPS_Open
-- 06/03/91 - dave edwards
--   altered LOPS_GetRequest so it calls heartbeat function, METAmethod now is an int
-- 19/03/91 - dave edwards 
--   added sccs id stuff
-- 11/04/91 - dave edwards
--   fixed OPS_GetRequest timeout bug. if not heartbeat polls to test error flag
--
------------------------------------------------------------------------------*/

/* CMSIDENTIFIER */
static char *CMS_Id = "LINKOPS_H:LOPS_C.AAAA-FILE;1(15-JAN-92)";

#include <stdio.h>
#include <stdint.h>
#include "types.h"
#include "linkops.h"
#include "hbeat.h"
#include "conlib.h"
#include "debug.h"
#include "opserror.h"
#include "lmethod.h"

#include "linkio.h"

#ifdef MSDOS
#include <conio.h>
#endif

/**
 ** imports
 **/

/* from metalnkio.c */
extern bool METAsearch_database;
extern int METAmethod;

/* link level timeouts (in 10ths of second) */
#define IOBIG_TIMEOUT   30 
#define IOSHORT_TIMEOUT 30

#define POKE_TRANSPUTER    0
#define PEEK_TRANSPUTER    1

/***
**** linkops routines implemented in terms of linkio
***/

/*
  modifies : con_id, link_name, (server name is not modified - it is for tops.c)
*/
unsigned char LOPS_Open (resource_name, server_name, buffer_depth, con_id, localmethod, link_name)
char *resource_name;
char *server_name;
int buffer_depth;
long int *con_id;
int *localmethod;                                                                       
char *link_name;
{
  unsigned char result;
  int linkid;
  DebugMessage (fprintf (stderr, "[LOPS_Open\n") );
  
  METAsearch_database = false;
  linkid = OpenLink (resource_name);
  METAsearch_database = true;
  
  *localmethod = METAmethod;
  
  if (linkid >= 0) {
    *con_id =  (long int) linkid;
    (void) CM_GetLinkName (link_name);
    result = STATUS_NOERROR;
  } else {
    /* couldnt open, try another resource */
    DebugMessage (fprintf (stderr, "]LOPS_Open\n") );
    switch (linkid) {
      case ER_LINK_SOFT:
        result = STATUS_BAD_OPERATION;
        break;
        
      case ER_LINK_BUSY:
        result = STATUS_TARGET_UNAVAILABLE;
        break;
        
      default:
        result = STATUS_BAD_OPERATION;
        break;
    }
  }
  DebugMessage (fprintf (stderr, "]LOPS_Open\n") );
  return (result);
}

/*
  modifies : nothing
*/
unsigned char LOPS_Close (con_id)
long int con_id;
{
  
  DebugMessage (fprintf (stderr, "[LOPS_Close\n") );
  /* try & close link */       
  if (CloseLink((int) con_id) < 0) {
    (void) sprintf (lnkops_errorstring, " : module [lops.c], function [LOPS_Close]\n -> can't closelink\n");
    return (STATUS_LINK_FATAL);
  }
  DebugMessage (fprintf (stderr, "]LOPS_Close\n") );
  return (STATUS_NOERROR);
}
    
/* 
  modifies : commsmode
*/
unsigned char LOPS_CommsAsynchronous (con_id, commsmode, errormode)
long int con_id;          
enum CommsModes *commsmode;
enum ErrorModes errormode;
{
  int result;
  
  DebugMessage (fprintf (stderr, "[LOPS_Asynchronous\n") );
  *commsmode = COMMSasynchronous;
  if (errormode == ERRORdetect) {
    result = TestError ((int) con_id);
    if (result < 0) {
      (void) sprintf (lnkops_errorstring, " : module [lops.c], function [LOPS_CommsAsynchronous]\n -> testerror failed\n");
      return (STATUS_LINK_FATAL);
    }
    if (result == 1) {
      return (STATUS_TARGET_ERROR);
    }
  }
  DebugMessage (fprintf (stderr, "]LOPS_Asynchronous\n") );
  return (STATUS_NOERROR);
}

/*
  modifies : commsmode
*/
unsigned char LOPS_CommsSynchronous (con_id, commsmode, errormode)
long int con_id;
enum CommsModes *commsmode;
enum ErrorModes errormode;
{
  int result;
  
  DebugMessage (fprintf (stderr, "[LOPS_CommsSynchronous\n") );
  *commsmode = COMMSsynchronous;
  if (errormode == ERRORdetect) {
    result = TestError ((int) con_id);
    if (result < 0) {
      (void) sprintf (lnkops_errorstring, " : module [lops.c], function [LOPS_CommsSynchronous]\n -> testerror failed\n");
      return (STATUS_LINK_FATAL);
    }
    if (result == 1) {
      return (STATUS_TARGET_ERROR);
    }
  };
  DebugMessage (fprintf (stderr, "]LOPS_Synchronous\n") );
  return (STATUS_NOERROR);
}

/*
  modifies : errormode
*/
unsigned char LOPS_ErrorDetect (con_id, errormode)
long int con_id;
enum ErrorModes *errormode;
{
  DebugMessage (fprintf (stderr, "[LOPS_ErrorDetect\n") );
  *errormode = ERRORdetect;
  DebugMessage (fprintf (stderr, "]LOPS_ErrorDetect\n") );
  return (STATUS_NOERROR);
}

/*
  modifies : errormode
*/
unsigned char LOPS_ErrorIgnore (con_id, errormode)
long int con_id;
enum ErrorModes *errormode;
{
  DebugMessage (fprintf (stderr, "[LOPS_ErrorIgnore\n") );
  *errormode = ERRORignore;  
  DebugMessage (fprintf (stderr, "]LOPS_ErrorIgnore\n") );
  return (STATUS_NOERROR);
}

/*
  modifies : nothing
*/
unsigned char LOPS_BootWrite (con_id, buffer, num_bytes, errormode)
long int con_id;
unsigned char *buffer;
unsigned long num_bytes;
enum ErrorModes errormode;
{
  unsigned long bytes_to_go;
  unsigned int bytes_to_write;
  int sent, result;
  
#define LINKIOFRAGSIZE 32767
  /* fragment into several write operations because of b016 linkio bugs */
  DebugMessage (fprintf (stderr, "[LOPS_BootWrite\n") ); 
  
  bytes_to_go = num_bytes;
  while (bytes_to_go > 0L) {
    if (bytes_to_go > LINKIOFRAGSIZE) {
      bytes_to_write = LINKIOFRAGSIZE;
    } else {
      bytes_to_write = (unsigned int) bytes_to_go;
    }
    DebugMessage ( fprintf (stderr, "Debug       : writing [%d] bytes\n", bytes_to_write) );
    sent = WriteLink ((int) con_id, (char *) buffer, bytes_to_write, IOBIG_TIMEOUT);
    if ((unsigned long) sent != bytes_to_write) {
      (void) sprintf (lnkops_errorstring, " : module [lops.c], function [LOPS_BootWrite]\n -> writelink failed = [%d]\n", sent);
      return (STATUS_LINK_FATAL);
    }else {
      bytes_to_go -= sent;
    }
  }
  
  if (errormode == ERRORdetect) {
    result = TestError ((int) con_id);
    if (result < 0) {
      (void) sprintf (lnkops_errorstring, " : module [lops.c], function [LOPS_BootWrite]\n -> testerror failed\n");
      return (STATUS_LINK_FATAL);
    }
    if (result == 1) {
      return (STATUS_TARGET_ERROR);
    }
  }
  DebugMessage (fprintf (stderr, "]LOPS_BootWrite\n") );
  return (STATUS_NOERROR);
}

unsigned char LOPS_Reset (con_id, proc_id)
long int con_id;
long int proc_id;
{
  int result;
  DebugMessage (fprintf (stderr, "[LOPS_Reset\n") );
  
  result = ResetLink ((int) con_id);
  if (result < 0) {
    (void) sprintf (lnkops_errorstring, " : module [lops.c], function [LOPS_Reset]\n -> resetlink failed=[%d]\n", result);
    return (STATUS_LINK_FATAL);
  } else {
     DebugMessage (fprintf (stderr, "]LOPS_Reset\n") );
    return (STATUS_NOERROR);
  }
}

unsigned char LOPS_Analyse (con_id, proc_id)
long int con_id;
long int proc_id;
{
  int result;
  
  DebugMessage (fprintf (stderr, "[LOPS_Analyse\n") );
  
  result = AnalyseLink ((int) con_id);
  if (result < 0) {
    (void) sprintf (lnkops_errorstring, " : module [lops.c], function [LOPS_Analyse]\n -> analyselink failed\n");
    return (STATUS_LINK_FATAL);
  } else {
    DebugMessage (fprintf (stderr, "]LOPS_Analyse\n") );
    return (STATUS_NOERROR);
  }
}

unsigned char LOPS_Poke16 (con_id, proc_id, pokesize, address, buffer)
long int con_id;       
long int proc_id;
unsigned long pokesize;
short int address;       
unsigned char *buffer;
{
  short int addr;
  int words_poked, result;
  unsigned char *bufptr;
  unsigned char poke_command[5];
  union {
    uint16_t as_short;
    unsigned char as_bytes[2];
  } tmp;
  
  DebugMessage (fprintf (stderr, "[LOPS_Poke16\n") );
  
  addr = address;
  bufptr = (unsigned char *) &buffer[0];
  poke_command[0] = POKE_TRANSPUTER;
  
  for (words_poked = 0; words_poked < (int) pokesize; words_poked++) {
    tmp.as_short = addr;
#ifdef BIG_ENDIAN
    /* fill in address */
    poke_command[2] = tmp.as_bytes[0];
    poke_command[1] = tmp.as_bytes[1];
#else
    /* fill in address */
    poke_command[2] = tmp.as_bytes[1];
    poke_command[1] = tmp.as_bytes[0];
#endif
    /* fill in data */
    poke_command[3] = *bufptr++;
    poke_command[4] = *bufptr++;
    result = WriteLink ((int) con_id,(char *) poke_command, 5, IOBIG_TIMEOUT);
    
    if (result != 5) {                               
      (void) sprintf (lnkops_errorstring, " : module [lops.c], function [LOPS_Poke16]\n -> writelink failed = [%d]\n", result);
      return (STATUS_LINK_FATAL);
    }
    addr = addr + 2;
  }
  DebugMessage (fprintf (stderr, "]LOPS_Poke16\n") );
  return (STATUS_NOERROR);
}

unsigned char LOPS_Poke32 (con_id, proc_id, pokesize, address, buffer)
long int con_id;
long int proc_id;
unsigned long pokesize;
long int address;       
unsigned char *buffer;
{
  long addr;
  int words_poked, result;
  unsigned char *bufptr;
  unsigned char poke_command[9];
  union {
    uint32_t as_long;
    unsigned char as_bytes[4];
  } tmp;
  
  DebugMessage (fprintf (stderr, "[LOPS_Poke32\n") );
  
  addr = address;
  bufptr = (unsigned char *) &buffer[0];
  poke_command[0] = POKE_TRANSPUTER;
  
  
  for (words_poked = 0; words_poked < (int) pokesize; words_poked++) {
    tmp.as_long = addr;
#ifdef BIG_ENDIAN
    /* fill in address */
    poke_command[4] = tmp.as_bytes[0];
    poke_command[3] = tmp.as_bytes[1];
    poke_command[2] = tmp.as_bytes[2];
    poke_command[1] = tmp.as_bytes[3];
#else
    /* fill in address */
    poke_command[4] = tmp.as_bytes[3];
    poke_command[3] = tmp.as_bytes[2];
    poke_command[2] = tmp.as_bytes[1];
    poke_command[1] = tmp.as_bytes[0];
#endif
    /* fill in data */
    poke_command[5] = *bufptr++;
    poke_command[6] = *bufptr++;
    poke_command[7] = *bufptr++;
    poke_command[8] = *bufptr++;
    result = WriteLink ((int) con_id, (char *) poke_command, 9, IOBIG_TIMEOUT);
    if (result != 9) {
      (void) sprintf (lnkops_errorstring, " : module [lops.c], function [LOPS_Poke32]\n -> writelink failed = [%d]\n", result);
      return (STATUS_LINK_FATAL);
    }
    addr = addr + 4;
  }
  DebugMessage (fprintf (stderr, "]LOPS_Poke32\n") );
  return (STATUS_NOERROR);
}

unsigned char LOPS_Peek16 (con_id, proc_id, peeksize, address, buffer)
long int con_id;
long int proc_id;
unsigned long peeksize;
short int address;
unsigned char *buffer;
{
  int result;
  int words_peeked;
  int index;
  unsigned char peek_command[3];
  
  union {
    uint16_t as_short;
    unsigned char as_bytes[2];
  } tmp_address;
  
  DebugMessage (fprintf (stderr, "[LOPS_Peek16\n") );
  
  peek_command[0] = PEEK_TRANSPUTER;
  words_peeked = 0;
  index = 0;
  
  while (words_peeked != (int) peeksize) {
    tmp_address.as_short = address;
#ifdef BIG_ENDIAN 
    peek_command[2] = tmp_address.as_bytes[0];     
    peek_command[1] = tmp_address.as_bytes[1];     
#else
    peek_command[2] = tmp_address.as_bytes[1];
    peek_command[1] = tmp_address.as_bytes[0];     
#endif
    /* send peek command */
    result = WriteLink ((int) con_id, (char *) peek_command, 3, IOBIG_TIMEOUT);
    if (result != 3) {                                        
      (void) sprintf (lnkops_errorstring, " : module [lops.c], function [LOPS_Peek16]\n -> writelink failed = [%d]\n", result);
      return (STATUS_LINK_FATAL);
    }
    /* read peeked word back */
    result = ReadLink ((int) con_id, (char *) &buffer[index], 2, IOBIG_TIMEOUT);
    if (result != 2) {                                         
      (void) sprintf (lnkops_errorstring, " : module [lops.c], function [LOPS_Peek16]\n -> readlink failed = [%d]\n", result);
      return (STATUS_LINK_FATAL);
    }
    words_peeked = words_peeked + 1;
    address = address + 2;      
    index = index + 2;       
  }
  DebugMessage (fprintf (stderr, "]LOPS_Peek16\n") );
  return (STATUS_NOERROR);
}

unsigned char LOPS_Peek32 (con_id, proc_id, peeksize, address, buffer)
long int con_id;
long int proc_id;
unsigned long peeksize;
long int address;
unsigned char *buffer;
{
  int result;
  int words_peeked;
  int index;
  unsigned char peek_command[5];
  
  union {
    uint32_t as_long;
    unsigned char as_bytes[4];
  } tmp_address;
  
  DebugMessage (fprintf (stderr, "[LOPS_Peek32\n") );
  
  peek_command[0] = PEEK_TRANSPUTER;
  words_peeked = 0;
  index = 0;
  
  while (words_peeked != (int) peeksize) {
    tmp_address.as_long = address;               
#ifdef BIG_ENDIAN 
    peek_command[4] = tmp_address.as_bytes[0];
    peek_command[3] = tmp_address.as_bytes[1];     
    peek_command[2] = tmp_address.as_bytes[2];     
    peek_command[1] = tmp_address.as_bytes[3];     
#else
    peek_command[4] = tmp_address.as_bytes[3];
    peek_command[3] = tmp_address.as_bytes[2];     
    peek_command[2] = tmp_address.as_bytes[1];     
    peek_command[1] = tmp_address.as_bytes[0];     
#endif
    /* send peek command */                          
    result = WriteLink ((int) con_id, (char *) peek_command, 5, IOBIG_TIMEOUT);
    if (result != 5) {
      (void) sprintf (lnkops_errorstring, " : module [lops.c], function [LOPS_Peek32]\n -> writelink failed = [%d]\n", result);
      return (STATUS_LINK_FATAL);
    }
    /* read peeked word back */
    result = ReadLink ((int) con_id, (char *) &buffer[index], 4, IOBIG_TIMEOUT);
    if (result != 4) {
      (void) sprintf (lnkops_errorstring, " : module [lops.c], function [LOPS_Peek32]\n -> readlink failed = [%d]\n", result);
      return (STATUS_LINK_FATAL);
    }
    words_peeked = words_peeked + 1;
    address = address + 4;
    index = index + 4;       
  }
  DebugMessage (fprintf (stderr, "]LOPS_Peek32\n") );
  return (STATUS_NOERROR);
}

unsigned char LOPS_GetRequest (con_id, buffer, timeout, heartbeat_fn, errormode)
long int con_id;
unsigned char *buffer;
long int timeout;
unsigned char (*heartbeat_fn) ();
enum ErrorModes errormode;
{
  int result, size, bytes_read, bytes_to_read, wait_time;
  int nread;
  bool got_header = false;
  bool do_heartbeat;
  
  DebugMessage (fprintf (stderr, "[LOPS_GetRequest\n") );
  
  if (heartbeat_fn == NULL) {
    DebugMessage (fprintf (stderr, "Debug       : no heartbeat function\n") );
    do_heartbeat = false;
    wait_time = IOBIG_TIMEOUT;
  } else {
    do_heartbeat = true;
    wait_time = ((int) (timeout / 100000L));
    if (wait_time <  1) {
      wait_time = 1;
    }
  }
  
  while (got_header == false) {
    /* test error status */
    if (errormode == ERRORdetect) {
      result = TestError ((int) con_id);
      if (result < 0) {             
        (void) sprintf (lnkops_errorstring, " : module [lops.c], function [LOPS_GetRequest]\n -> 1st testerror failed\n");
        return (STATUS_LINK_FATAL);
      }
      if (result == 1) {
        return (STATUS_TARGET_ERROR);
      }
    }
  
    /* read minimum size sp packet (8 bytes) */
    DebugMessage (fprintf (stderr, "Debug       : reading sp header\n") );
    bytes_read = ReadLink ((int) con_id, (char *) buffer, 1, wait_time);
    switch (bytes_read) {
      case 0:
        /* nothing read */
        if (do_heartbeat == true) {
          DebugMessage (fprintf (stderr, "Debug       : calling heartbeat function\n") );
          switch ((*heartbeat_fn) ()) {
            case DO_HEARTBEAT:          
              /* no key read */
              DebugMessage (fprintf (stderr, "Debug       : keep heartbeat going\n") );
              break;
              
            case KILL_HEARTBEAT:
              do_heartbeat = false;
              DebugMessage (fprintf (stderr, "Debug       : disable heartbeat\n") );
              break;
              
            case FATAL_ERROR:
              (void) sprintf (lnkops_errorstring, " : module [lops.c], function [LOPS_GetRequest]\n -> heartbeat function returned error\n");
              return (STATUS_BAD_OPERATION);
              break;
              
            default:
              (void) sprintf (lnkops_errorstring, " : module [lops.c], function [LOPS_GetRequest]\n -> heartbeat function returned unexpected value\n");
              return (STATUS_BAD_OPERATION);
              break;
          }
        }
#ifdef MSDOS
	/* Allow break */
	kbhit();
#endif
        break;
        
      case 1:
        /* 1 byte read ok, read rest of header */
        nread = ReadLink ((int) con_id, (char *) &buffer[1], (unsigned int) (8 - 1), IOSHORT_TIMEOUT);

        /* test error status */
        if (errormode == ERRORdetect) {
          result = TestError ((int) con_id);
          if (result < 0) {             
            (void) sprintf (lnkops_errorstring, " : module [lops.c], function [LOPS_GetRequest]\n -> 2nd testerror failed\n");
            return (STATUS_LINK_FATAL);
          }
          if (result == 1) {
            return (STATUS_TARGET_ERROR);
          }
        }
  
        if (nread < 0) {
          (void) sprintf (lnkops_errorstring, " : module [lops.c], function [LOPS_GetRequest]\n -> 2nd readlink failed = [%d]\n", nread);
          return (STATUS_LINK_FATAL);
        }                       
        bytes_read = bytes_read + nread;
        if (bytes_read == 8) {
          /* header read correctly */
          got_header = true;
        } else {
          (void) sprintf (lnkops_errorstring, " : module [lops.c], function [LOPS_GetRequest]\n -> got [%d] bytes for header\n", bytes_read);
          return (STATUS_SP_ERROR);
        }
        break;
        
      case ER_LINK_NOSYNC:
        /* timed out with nosync, ignore this for b016 */
        break;
        
      default:
        (void) sprintf (lnkops_errorstring, " : module [lops.c], function [LOPS_GetRequest]\n -> 1st readlink failed = [%d]\n", bytes_read);
        return (STATUS_LINK_FATAL);
        break;
    }
  }
  
  /* got header so test error status */
  if (errormode == ERRORdetect) {
    result = TestError ((int) con_id);
    if (result < 0) {
      (void) sprintf (lnkops_errorstring, " : module [lops.c], function [LOPS_GetRequest]\n -> 3rd testerror failed\n");
      return (STATUS_LINK_FATAL);
    }
    if (result == 1) {
      return (STATUS_TARGET_ERROR);
    }
  }
  
  /* calculate header size & see if should read more */
  size = buffer[0] + (buffer[1]*256);
  if (size > SPMAXHEADERVALUE) {
    (void) sprintf (lnkops_errorstring, " : module [lops.c], function [LOPS_GetRequest]\n -> sp header too large = [%d]\n", size);
    return (STATUS_SP_ERROR);
  }

  if (size > 6) {
    /* read remaining data */
    bytes_to_read = (size - 6); /* the 8 byte header contains 6 bytes of data */
    DebugMessage (fprintf (stderr, "Debug       : reading packet body ([%d] bytes)\n", bytes_to_read) );
    bytes_read = ReadLink ((int) con_id, (char *) &buffer[8], (unsigned int) bytes_to_read, IOSHORT_TIMEOUT);
    
    /* test error status */
    if (errormode == ERRORdetect) {
      result = TestError ((int) con_id);
      if (result < 0) {             
        (void) sprintf (lnkops_errorstring, " : module [lops.c], function [LOPS_GetRequest]\n -> 4th testerror failed\n");
        return (STATUS_LINK_FATAL);
      }
      if (result == 1) {
        return (STATUS_TARGET_ERROR);
      }
    }

    if (bytes_read != bytes_to_read) {
      (void) sprintf (lnkops_errorstring, " : module [lops.c], function [LOPS_GetRequest]\n -> 3rd readlink failed = [%d]\n", bytes_read);
      return (STATUS_LINK_FATAL);
    }
  }                                

  if (do_heartbeat == true) {
    DebugMessage (fprintf (stderr, "Debug       : calling heartbeat function\n") );
    switch ((*heartbeat_fn) ()) {
      case DO_HEARTBEAT:          
        /* no key read */
        DebugMessage (fprintf (stderr, "Debug       : keep heartbeat going\n") );
        break;
        
      case KILL_HEARTBEAT:
        do_heartbeat = false;
        wait_time = 0L;
        DebugMessage (fprintf (stderr, "Debug       : disable heartbeat\n") );
        break;
        
      case FATAL_ERROR:
        (void) sprintf (lnkops_errorstring, " : module [lops.c], function [LOPS_GetRequest]\n -> heartbeat function returned error\n");
        return (STATUS_BAD_OPERATION);
        break;
        
      default:
        (void) sprintf (lnkops_errorstring, " : module [lops.c], function [LOPS_GetRequest]\n -> heartbeat function returned unexpected value\n");
        return (STATUS_BAD_OPERATION);
        break;
    }
  }
  DebugMessage (fprintf (stderr, "Debug       : sptag = [%d]\n", buffer[2]) );
  DebugMessage (fprintf (stderr, "]LOPS_GetRequest\n") );
  
  return (STATUS_NOERROR);
}

unsigned char LOPS_SendReply (con_id, buffer, errormode)
long int con_id;
unsigned char *buffer;
enum ErrorModes errormode;
{
  int result, size;
  int writeresult;
  
  DebugMessage (fprintf (stderr, "[LOPS_SendReply\n") );
  
  size = buffer[0] + (buffer[1]*256);
  if ((size <= 0) || (size > SPMAXPACKET)) {
    (void) sprintf (lnkops_errorstring, " : module [lops.c], function [LOPS_SendReply]\n -> sp header bad = [%d]\n", size);
    return (STATUS_SP_ERROR);
  }
  
  size = size + 2; /* include header */
  
  DebugMessage (fprintf (stderr, "Debug       : writing reply of [%ld] bytes\n", (long int) size) );
  /* send reply */
  writeresult = WriteLink ((int) con_id, (char *)buffer, (unsigned int) size, IOSHORT_TIMEOUT);

  /* test error status */
  if (errormode == ERRORdetect) {
    result = TestError ((int) con_id);
    if (result < 0) {             
      (void) sprintf (lnkops_errorstring, " : module [lops.c], function [LOPS_SendReply]\n -> testerror failed\n");
      return (STATUS_LINK_FATAL);
    }
    if (result == 1) {
      return (STATUS_TARGET_ERROR);
    }
  }

  /* Check write was complete */
  if (writeresult != size) {
    /* It may be because the error flag has become set */
    (void) sprintf (lnkops_errorstring, " : module [lops.c], function [LOPS_SendReply]\n -> writelink failed = [%d]\n", result);
    return (STATUS_LINK_FATAL);
  }
  
  DebugMessage (fprintf (stderr, "]LOPS_SendReply\n") );
  return (STATUS_NOERROR);
}

unsigned char LOPS_Restart (con_id, commsmode, errormode)
long int con_id;
enum CommsModes *commsmode;
enum ErrorModes *errormode;
{
  DebugMessage (fprintf (stderr, "[LOPS_Restart\n") );
  
  *commsmode = COMMSsynchronous;
  *errormode = ERRORignore;
  
  DebugMessage (fprintf (stderr, "]LOPS_Restart\n") );
  return (STATUS_NOERROR);
}
