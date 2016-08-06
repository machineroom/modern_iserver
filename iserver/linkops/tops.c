/*------------------------------------------------------------------------------
--
-- File    : tops.c
--   tcp implementation of linkops protocol layered over TCP/IP via BSD sockets
--
-- Copyright 1990, 1991 INMOS Limited
--
-- Date    : 3/4/91
-- Version : 1.8
--
-- 20/06/90 - dave edwards
--  originated, added heartbeat function (GetRequest)
-- 04/10/90 - dave edwards
--   updated to return error messages into lnkops_errorstring rather
--     than print out
-- 04/12/90 - dave edwards
--   altered debug/info bits
-- 07/12/90 - dave edwards
--  'pc-nfs'ified
-- 21/02/91 - dave edwards
--  added sprequest buffering to a finite user specified depth
-- 26/02/91 - dave edwards
--  altered so when sp queue full, network not checked for responses
-- 05/02/91 - dave edwards
--  added localmethod parameter to TOPS_Open
-- 07/02/91 - dave edwards
--  made WINTCP version use QIO mechanism rather than select for DoTimedSocketRead (speed reasons)
-- 12/03/91 - dave edwards
--  fixed malloc bug in QueueSPBuffer                                                            
-- 14/03/91 & 18/03/91 - dave edwards
--  fixed bug in OPS_GetRequest - checks if request buffered after
--  calling heartbeat function
-- 19/03/91 - dave edwards
--  added sccs id stuff
-- 25/03/91 - dave edwards
--  fixed VMS DoTimedRead to be in ms rather than uS
-- 03/06/91 - dave edwards
--  increased WAIT_TIMEOUT (see TL/0077 for explanation)
-- 26/06/91 - dave edwards
--  added DEC/TCP support
-- 15/07/91 - dave edwards
--  altered so DEC/TCP version doesn't set socket window sizes (need syspriv)
-- 03/03/92 - nick stephen
--  added PC/TCP functionality
-- 20/03/92 - nick stephen
--  upgraded to run under PC-NFS Toolkit v2.0 (was under v1.0)
--
------------------------------------------------------------------------------*/

/* CMSIDENTIFIER */
static char *CMS_Id = "PRODUCT:ITEM.VARIANT-TYPE;0(DATE)";

#ifndef NOCOMMS

#include <stdio.h>

#ifdef WINTCP
extern int uerrno;
#define ERRNO uerrno
#endif

#ifdef DECTCP
#define ERRNO errno
#endif

#ifdef LINUX
#include <errno.h>
#define ERRNO errno
#include <stdlib.h>
#include <unistd.h>
#endif

#ifdef PCNFS
/* #include "pcntypes.h" */  /* PC-NFS Toolkit v2.0 has these defined */
#include <sys/tk_types.h>    /* PC-NFS Toolkit v2.0 defines */
extern int errno;
#define ERRNO errno
#include <tk_errno.h>
#include <dos.h>             /* To allow call to error-handler */
union REGS dos_regs;
#else
#include <errno.h>
#endif /* PCNFS */

#ifdef PCTCP
#include "errno.h"
#define ERRNO errno
#endif

#ifdef SUNTCP
#define ERRNO errno
#endif

#ifdef DECTCP
#include <types.h>
#include <socket.h>
#include <time.h>
#include <in.h>
#include <netdb.h>
#include <psldef.h>
#include <descrip.h>
#include <ssdef.h>
#include <iodef.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#ifdef PCNFS
#include <sys/nfs_time.h>
#else
#include <sys/time.h>
#endif /* PCNFS */
#include <netinet/in.h>
#include <netdb.h>
#ifndef WINTCP
#include <memory.h>
#endif /* WINTCP */
#endif /* DECTCP */

#ifdef WINTCP
#include <psldef.h>
#include <time.h>
#include <descrip.h>
#include <ssdef.h>
#include <vms/iodef.h>
#include <sys/errno.h>
#endif

#ifdef VMS
#include <string.h>
#else
#ifdef MSDOS
#include <string.h>
#include <malloc.h>
#else
#ifdef SOLARIS
#include <string.h>
#else
#include <strings.h>
#endif
#endif /* MSDOS */
#endif /* VMS */

#include "types.h"
#include "linkops.h"  
#include "conlib.h"
#include "debug.h"
#include "hbeat.h"
#include "lmethod.h"
#include "opsprot.h"
#include "opserror.h"

#ifdef NODELAY
#include <netinet/tcp.h>
#endif

/* timeout when trying to close or restart */
#define WAIT_TIMEOUT (ONESECOND * 30L)

/* timeout when trying to read service string */
#define READOPEN1_TIMEOUT (ONESECOND * 20L)
#define READOPEN2_TIMEOUT (ONESECOND * 20L)

/**
 ** imports
**/

/**
 ** enum declarations 
**/
static enum ProtocolStates
 {Closed, OpenCheck,
  Synchronous, SynchronousReply, SyncClosing, SyncReplyClosing, SyncReplyClosed,
  Asynchronous, CancelAsynchronous, AsyncClosing, CancelAsyncClosing, Restarting};
  
static enum TimedStatus {TIMEDSTATUSdata, TIMEDSTATUSnodata, TIMEDSTATUSfailure};

/**
 ** internal variables
 **/
 
/* used to read replies & events */
static unsigned char response_buffer[OPSMAXPACKET + OPSMAXOVERHEADS];
/* reflects protocol state */
static enum ProtocolStates TOPSprotocolstate = Closed;
/* reflects ErrorMode */
static bool TOPSerrordetect = false;

struct SPBuffer {
  int spsize;
  unsigned char sppacket[SPMAXPACKET];
  struct SPBuffer *next;
};

/* pointer to buffered requests */
static struct SPBuffer *TOPSspqueueptr;
/* pointer to last allocated request (only valid just after allocated) */
static struct SPBuffer *TOPSnewbufptr;
/* number of buffered requests */
static int TOPSnum_spbuffered;

/* maximum number of buffered requests */
static int TOPSbuffer_depth;

/* used to calculate 32 bit int parameters */
static union {
  long int as_long;
  char as_bytes[4];
} SignedInt32;

static union {
  unsigned long as_long;
  unsigned char as_bytes[4];
} UnsignedInt32;

/* used to calculate 16 bit int parameters */
static union {
  short int as_short;
  char as_bytes[2];
} SignedInt16;

static union {
  unsigned short as_short;
  unsigned char as_bytes[2];
} UnsignedInt16;

/**
 ** internal functions
 **/    

/* DoSocketWrite -
  effects : attempts to write N bytes from the buffer to the socket.
            if successfull, returns STATUS_NOERROR
            if an error occurs, returns STATUS_COMMS_FATAL
*/
static unsigned char DoSocketWrite (sock, to_write, buffer)
int sock;
int to_write;
char *buffer;
{
  int data_written;
  int left_to_write = to_write;
  
  while (left_to_write > 0) {  
    data_written = send (sock, buffer, to_write, 0);
    if (data_written == 0) {
      (void) sprintf (lnkops_errorstring, " : connection to server lost\n");
      return (STATUS_COMMS_FATAL);
    }
    if (data_written < 0) {
#ifdef PCNFS
      if (ERRNO == EINTR)
        int86(0x1b,&dos_regs,&dos_regs); /* Generate a 'break' interrupt */
#endif
      switch (ERRNO) {
        case ENOBUFS: /* not enough buffers, try again */
          DebugMessage (fprintf (stderr, "Warning     : couldn't send, trying again\n") );
          break;
        
        default:
          (void) sprintf (lnkops_errorstring, " : module [tops.c], function [DoSocketWrite]\n -> send failed, errno=[%d]\n", ERRNO);
          return (STATUS_COMMS_FATAL);         
          break;
      }
    }
    left_to_write = left_to_write - data_written;
    buffer = buffer + data_written;
  }
  
  return (STATUS_NOERROR);
}

/* DoSocketRead -
  effects   : attempts to read N bytes from the socket into buffer
              if succesfull returns STATUS_NOERROR
              if an error occurs returns STATUS_COMMS_FATAL
*/
static unsigned char DoSocketRead (sock, to_read, buffer)
int sock;
int to_read;
unsigned char *buffer;
{
  int left_to_read = to_read;
  int data_read;
  
  while (left_to_read > 0) {
    data_read = recv (sock, (char *)buffer, left_to_read, 0);
    if (data_read < 0) {
#ifdef PCNFS
      if (ERRNO == EINTR)
          int86(0x1b,&dos_regs,&dos_regs); /* Generate a 'break' interrupt */
#endif
      (void) sprintf (lnkops_errorstring, " : module [tops.c], function [DoSocketRead]\n -> recv failed, errno=[%d]\n", ERRNO);
      return (STATUS_COMMS_FATAL);
    }
    if (data_read == 0) {
      (void) sprintf (lnkops_errorstring, " : connection to server lost\n");
      return (STATUS_COMMS_FATAL);
    }
    left_to_read = left_to_read - data_read;
    buffer = buffer + data_read;
  }
  return (STATUS_NOERROR);
}

/* DoTimedSocketRead -
  parameters : timeout is in mS
  effects   : 
    examines socket,
    if data is available to read, attempts to read 'to_read' bytes &
     returns TIMEDSTATUSdata
    
    if no data is available, returns TIMEDSTATUSnodata
    
    if an error occurs, returns TIMEDSTATUSfailure
*/
static enum TimedStatus DoTimedSocketRead (sock, to_read, timeout, buffer)
int sock;
int to_read;
long int timeout;
unsigned char *buffer;
{
#ifdef WINTCP
#define time_event (unsigned long int) 1  /* Event flags and masks */
#define read_event (unsigned long int) 2
#define read_mask  (unsigned long int) 4
#define event_mask (unsigned long int) 6

#define IO$_RECEIVE IO$_READVBLK  /* See WIN/TCP reference manual */
  int bytes_read;
  unsigned long int flags;
  int status;               /* status is consistantly ignored */
  int vms_delta_time[2];    /* To store our backdoor version of VMS time */
  int read_iosb[2];         /* i/o status block */

  vms_delta_time[1] = -1;
  vms_delta_time[0] = timeout * -10; /* convert ms to VMS 100 nS intervals */

  /* Queue read */
  status = SYS$QIO(read_event, sock, IO$_RECEIVE, read_iosb, 0, 0, buffer, to_read, 0, 0, 0, 0);
  if (status != SS$_NORMAL) {
    (void) sprintf (lnkops_errorstring, " : module [tops.c], function [DoTimedSocketRead]\n -> Abnormal QIO status [%d]\n", status);
    return (TIMEDSTATUSfailure);
  };
	
  /* Set up timer */
  status = SYS$SETIMR(time_event, &vms_delta_time, 0, 0);
  if (status != SS$_NORMAL) {
    (void) sprintf (lnkops_errorstring, " : module [tops.c], function [DoTimedSocketRead]\n -> Abnormal timer status [%d]\n", status);
    return (TIMEDSTATUSfailure);
  };
	
  /* Wait for read or timer event flag top be set */
  status = SYS$WFLOR(read_event, event_mask);
  if (status != SS$_NORMAL) {
    (void) sprintf (lnkops_errorstring, " : module [tops.c], function [DoTimedSocketRead]\n -> Abnormal WFLOR status [%d]\n", status);
    return (TIMEDSTATUSfailure);
  };
	
  /* Findout which event flag was set */
  status = SYS$READEF(read_event, &flags);
  if ((status != SS$_WASSET) && (status != SS$_WASCLR)) {
    (void) sprintf (lnkops_errorstring, " : module [tops.c], function [DoTimedSocketRead]\n -> Abnormal READEF status [%d]\n", status);
    return (TIMEDSTATUSfailure);
  };

  if ((flags & read_mask) == read_mask)  { /* It was a read, not a timeout */
    int fnresult = read_iosb[0] & 0XFFFF;

    status = SYS$CANTIM(0, PSL$C_USER);  /* cancel timer */

    /* Extract length of data from i/o status block */
    bytes_read = (read_iosb[0] & 0XFFFF0000) >> 16;
    if (bytes_read != to_read) {
      (void) sprintf (lnkops_errorstring, "#####only read (%d) bytes, asked for [%d]\n", bytes_read, to_read);
      return (TIMEDSTATUSfailure);
    } else {
      return (TIMEDSTATUSdata);
    }
    
  } else {
    status = SYS$CANCEL(sock);
    return (TIMEDSTATUSnodata);
  }
}

#else /* not WINTCP */
  int fdwidth, data_read, result;
  struct timeval delay;
  int timeout_looping = 0;
  
#ifdef DECTCP
  int read_fds;
#else /* DECTCP */
fd_set read_fds;
#endif /* DECTCP */
  
#ifdef PCTCP
  if (timeout == 0L) {
    timeout_looping = 1;
    timeout = (ONESECOND * 8);
  }
#endif
  if (timeout >= ONESECOND) {    
    delay.tv_sec = (int)(timeout / ONESECOND);
    delay.tv_usec = (int)(timeout % ONESECOND);
  } else {
    delay.tv_sec = 0;        /* seconds */
    delay.tv_usec = (int)timeout; /* uS */
  }
  
#ifdef PCTCP
 do {
#endif

#ifdef DECTCP
  read_fds = (1 << sock);
#else
  FD_ZERO (&read_fds);
  FD_SET (sock, &read_fds);
#endif

  fdwidth = 32;
#ifdef DECTCP
  if (timeout == 0L) {
    /* infinite timeout */
    result = select (fdwidth, (int *) &read_fds, (int *) NULL, (int *) NULL, (struct timeval *)NULL);
  } else {
    result = select (fdwidth, (int *) &read_fds, (int *) NULL, (int *) NULL, &delay);
  }
#else
  if (timeout == 0L) {
    /* infinite timeout */
    result = select (fdwidth, &read_fds, (fd_set *) NULL, (fd_set *) NULL, (struct timeval *)NULL);
  } else {
    result = select (fdwidth, &read_fds, (fd_set *) NULL, (fd_set *) NULL, &delay);
  }
#endif /* DECTCP */

  if (result < 0) {
#ifdef PCNFS
    if (ERRNO == EINTR)
        int86(0x1b,&dos_regs,&dos_regs); /* Generate a 'break' interrupt */
#endif
    (void) sprintf (lnkops_errorstring, " : module [tops.c], function [DoTimedSocketRead]\n -> select failed errno=[%d]\n", ERRNO);
    return (TIMEDSTATUSfailure);
  }

#ifdef PCTCP  /* Loop around instead of infinite timeout. Poll CTRL-C key */
  (void) kbhit();
 } while (timeout_looping && (result == 0));
#endif

  if (result == 0) {
    return (TIMEDSTATUSnodata);
  } else {
    data_read = recv (sock, (char *)buffer, to_read, 0);
    if (data_read < 0) {
#ifdef PCNFS
      if (ERRNO == EINTR)
          int86(0x1b,&dos_regs,&dos_regs); /* Generate a 'break' interrupt */
#endif
      (void) sprintf (lnkops_errorstring, " : module [tops.c], function [DoTimedSocketRead]\n -> recv failed, errno=[%d]\n", ERRNO);
      return (TIMEDSTATUSfailure);
    }
    if (data_read == 0) {
      (void) sprintf (lnkops_errorstring, " : connection to server lost\n");
      return (TIMEDSTATUSfailure);
    } else {
      if (data_read != to_read) {
        (void) sprintf(lnkops_errorstring, "#####only read (%d) bytes, asked for [%d]\n", data_read, to_read);
        return (TIMEDSTATUSfailure);
      }
      return (TIMEDSTATUSdata);
    }
  }
}
#endif /* WINTCP */

/* HandleMessage ()
     requires :  OPSMIN_RESPONSEMESSAGESIZE bytes of message read into
                response_buffer
     modifies : response_buffer, TOPSprotocolstate
     effects  : handles message event :
                 outputs text string,
                 if message was fatal or a fatal error occurs,
                  return STATUS_COMMS_FATAL.
                 else
                  return STATUS_NOERROR
*/
static unsigned char HandleMessage (sock, packet_size)
int sock;
int packet_size;
{
  int extra_to_read;
  unsigned char result;
  
   /* read rest of message response */
   extra_to_read = (packet_size - OPSMIN_RESPONSEMESSAGESIZE);
   result = DoSocketRead (sock, extra_to_read, (unsigned char *)&response_buffer[OPSMIN_RESPONSEMESSAGESIZE]);
   if (result != STATUS_NOERROR) {
     (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [HandleMessage]\n");
     return (result);
   }      
   if (response_buffer[OEVENTMESSAGE_fatal] == ((unsigned char) 0)) {
#ifdef GIVENONFATALMESSAGES /* linkops server uses them for keepalives ! */
     (void) fprintf(stderr, "\n>>>>Message from linkops server<<<<\n[%s]\n", (char *)&response_buffer[OEVENTMESSAGE_string]);
     fflush (stderr);
#endif
     return (STATUS_NOERROR);
   } else {
     (void) fprintf(stderr, "\n>>>>Fatal message from linkops server<<<<\n[%s]\n", (char *)&response_buffer[OEVENTMESSAGE_string]);
     fflush (stderr);
     /* alter protocol state */
     TOPSprotocolstate = Closed;
     return (STATUS_COMMS_FATAL);
   }    
}

/* HandleErrorTransition ()
     requires :  OPSMIN_RESPONSEMESSAGESIZE bytes of error transition message read into
                response_buffer
     modifies : response_buffer, TOPSprotocolstate
     effects  : handles error transition event :
                 if error flag set,
                  returns STATUS_TARGET_ERROR
                 else if a fatal error occurs,
                  returns STATUS_COMMS_FATAL.
                 else
                  return STATUS_NOERROR
*/
static unsigned char HandleErrorTransition (sock, packet_size)
int sock;
int packet_size;
{
  int extra_to_read;
  unsigned char result;
  
  /* read rest of message response */
  extra_to_read = (packet_size - OPSMIN_RESPONSEMESSAGESIZE);
  result = DoSocketRead (sock, extra_to_read, (unsigned char *)&response_buffer[OPSMIN_RESPONSEMESSAGESIZE]);
  if (result != STATUS_NOERROR) {
    (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [HandleErrorTransition]\n");
    return (result);
  }      
  if (TOPSerrordetect == false) {
    (void) sprintf (lnkops_errorstring, " : module [tops.c], function [HandleErrorTransition]\n -> not in error detect mode\n");
    return (STATUS_COMMS_FATAL);
  }
  /* check processor id */
  UnsignedInt32.as_bytes[0] = response_buffer[OEVENTERRORTRANS_processorid];
  UnsignedInt32.as_bytes[1] = response_buffer[OEVENTERRORTRANS_processorid+1];
  UnsignedInt32.as_bytes[2] = response_buffer[OEVENTERRORTRANS_processorid+2];
  UnsignedInt32.as_bytes[3] = response_buffer[OEVENTERRORTRANS_processorid+3];
  if (ntohl(UnsignedInt32.as_long) != TSERIES_PROCESSORID) {
    (void) sprintf (lnkops_errorstring, " : module [tops.c], function [HandleErrorTransition]\n -> bad processor_id (T-series implementation)\n");
    return (STATUS_COMMS_FATAL);
  }
  /* check if error flag was set */
  if ((bool) response_buffer[OEVENTERRORTRANS_state] ==  ((unsigned char) 1)) {
    DebugMessage (fprintf (stderr, "Debug       : transputer error flag set\n") );
    return (STATUS_TARGET_ERROR);
  } else {
    DebugMessage (fprintf (stderr, "Debug       : transputer error flag clear\n") );
    return (STATUS_NOERROR);
  }
}

/* GetSyncResponse
     requires : sizeof(*buffer) >= OPSMAXPACKET
     modifies : buffer
     effects : tries to read the synchronous-mode response message
               whoose tag field = 'wanted_tag'.
               the message will be read into 'response_buffer'.
               the data fields of OREPLY_ReadLink, OREPLY_Peek16 and OREPLY_Peek32
               messages is read into 'buffer'.
               handles message events. if successful, returns STATUS_NOERROR
               else returns another linkops error code.
*/
static unsigned char GetSyncResponse (sock, wanted_tag, buffer)
int sock;
unsigned char wanted_tag;
unsigned char *buffer;
{
  unsigned char result, response_tag;
  int extra_to_read, bytes_read, packet_size;
  bool got_wanted_tag = false;
  
  while (got_wanted_tag == false) {
    /* read header of linkops response message */
    result = DoSocketRead (sock, OPSMIN_RESPONSEMESSAGESIZE, (unsigned char *)&response_buffer[0]);
    if (result != STATUS_NOERROR) {
      (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [GetSyncResponse]\n");
      return (result);
    }
    bytes_read = OPSMIN_RESPONSEMESSAGESIZE;
    
    /* deduce packet size, tag & how much more data needs to be read */
    UnsignedInt16.as_bytes[0] = response_buffer[OPSPACKETSIZEFIELD];
    UnsignedInt16.as_bytes[1] = response_buffer[OPSPACKETSIZEFIELD + 1];
    
    packet_size = ntohs (UnsignedInt16.as_short);
    if (packet_size > (OPSMAXPACKET + OPSMAXOVERHEADS)) {
      (void) sprintf (lnkops_errorstring, " : module [tops.c], function [GetSyncResponse]\n -> packet size too large = [%d]\n", packet_size);
      return (STATUS_COMMS_FATAL);
    }
    
    response_tag = response_buffer[OPSTAGFIELD];
    got_wanted_tag = (wanted_tag == response_tag);
    
    DebugMessage (fprintf (stderr, "Debug       : got tag        [%d]\n", response_tag) );
    DebugMessage (fprintf (stderr, "Debug       : got packetsize [%d]\n", packet_size) );  
    
    if (got_wanted_tag == false) {
      /* only synchronous mode response capable of handling (apart from
         the one we are waiting & OEVENT_MESSAGE is an error */
      if (response_tag == OEVENT_Message) {
        result = HandleMessage (sock, packet_size);
        if (result != STATUS_NOERROR) {
          (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [GetSyncResponse]\n");
          return (result);
        }
      } else {
        (void) sprintf (lnkops_errorstring, " : module [tops.c], function [GetSyncResponse]\n -> didnt get wanted tag - panic!\n");
        return (STATUS_COMMS_FATAL);
      }
      
    } else {
      /* got desired message, check tags & read rest of message */
      switch (response_tag) {
        /* responses with no more data to read */
        case OREPLY_Close:
        case OREPLY_Poke16:
        case OREPLY_Poke32:
        case OREPLY_CommsSynchronous:
          DebugMessage (fprintf (stderr, "Debug       : no more data to read\n") );
          return (STATUS_NOERROR);
          break;
          
        /* responses with more data to read */
        case OREPLY_TestError:
        case OREPLY_WriteLink:
          extra_to_read = (packet_size - bytes_read);
          result = DoSocketRead (sock, extra_to_read, (unsigned char *)&response_buffer[bytes_read]);
          if (result != STATUS_NOERROR) {
           (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [GetSyncResponse]\n");
          }
          return (result);
          break;
        
        /* responses with special data, read normal part first */
        case OREPLY_ReadLink:
          extra_to_read = (OPSReadLinkReplyBasicSize - bytes_read);
          break;
          
        case OREPLY_Peek16:
          extra_to_read = (OPSPeek16ReplyBasicSize - bytes_read);
          break;
          
        case OREPLY_Peek32:
          extra_to_read = (OPSPeek32ReplyBasicSize - bytes_read);
          break;
        
        default:
          (void) sprintf (lnkops_errorstring, " : module [tops.c], function [GetSyncResponse]\n -> got unexpected tag value =[%d]\n", response_tag);
          return (STATUS_COMMS_FATAL);
          break;
      }
      
      result = DoSocketRead (sock, extra_to_read, (unsigned char *)&response_buffer[bytes_read]);
      if (result != STATUS_NOERROR) {
        (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [GetSyncResponse]\n");
        return (result);
      }
      bytes_read = bytes_read + extra_to_read;
      extra_to_read = (packet_size - bytes_read);
      
      if (buffer == NULL) { 
        (void) sprintf(lnkops_errorstring," : module [tops.c], function [GetSyncResponse]\n -> buffer is NUL, panic!\n");
        return (STATUS_COMMS_FATAL);
      }
      /* read "data" field into users buffer */
      result = DoSocketRead (sock, extra_to_read, (unsigned char *)&buffer[0]);
      if (result != STATUS_NOERROR) {
        (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [GetSyncResponse]\n");
      }
      return (result);
    }
  }
  return (STATUS_NOERROR);
}

/* QueueSPBuffer :
     modifies : TOPSnum_spbuffered, TOPSspqueueptr, TOPSnewbufptr (lnkops_errorstring)
     effects  : allocates an sp buffer & links it into the queue
                of buffers
*/
static unsigned char QueueSPBuffer ()
{
  struct SPBuffer *new_sp_buffer, *last_spbuffer;
  int find;
  
  /* allocate new buffer */
  new_sp_buffer = (struct SPBuffer *) malloc(sizeof(*new_sp_buffer));
  if (new_sp_buffer == NULL) {
    (void) sprintf (lnkops_errorstring, " : module [tops.c], function [QueueSPBuffer]\n -> alloc failed\n");
    return (STATUS_BAD_OPERATION);
  }
          
  /* make new buffer hang on end of queue */ 
  if (TOPSnum_spbuffered == 0) {
    TOPSspqueueptr = new_sp_buffer;
    new_sp_buffer->next = NULL;
  } else {
    last_spbuffer = TOPSspqueueptr;
    for (find = 0; find < (TOPSnum_spbuffered - 1); find++) {
      last_spbuffer = last_spbuffer->next;
    }
    last_spbuffer->next = new_sp_buffer;
  }
  TOPSnum_spbuffered++;
  TOPSnewbufptr = new_sp_buffer;
  return (STATUS_NOERROR);
}

/* UnQueueSPBuffer
     modifies : dest_sppacket, TOPSspqueueptr, TOPSnum_spbuffered (lnkops_errorstring)
     effects  : writes the sprequest in the queue into dest_sppacket then removes
                the request from the queue
*/
unsigned char UnQueueSPBuffer (dest_sppacket)
unsigned char *dest_sppacket;
{
  struct SPBuffer *tmp_sp_buffer;
  
  DebugMessage (fprintf (stderr, "Debug       : unqueuing sppacket\n") );
  if (TOPSnum_spbuffered == 0) {
    (void) sprintf (lnkops_errorstring, " : module [tops.c], function [UnQueueSPPacket]\n -> no buffered requests\n");
    return (STATUS_BAD_OPERATION);
  }
  (void) memcpy (dest_sppacket, (char *)&TOPSspqueueptr->sppacket[0], TOPSspqueueptr->spsize);
  tmp_sp_buffer = TOPSspqueueptr;                 
  TOPSspqueueptr = TOPSspqueueptr->next;
  free (tmp_sp_buffer);
  TOPSnum_spbuffered--;
  return (STATUS_NOERROR);
}

/* CleanUpBufferQueue ()
    modifies : TOPSspqueueptr, TOPSnum_spbuffered (lnkops_errorstring)
    effects  : destroys buffer queue entirely
*/
unsigned char CleanUpBufferQueue ()
{
  char dummy_sppacket[SPMAXPACKET];
  int kill_queue;
  unsigned char result;
  
  for (kill_queue = TOPSnum_spbuffered; kill_queue > 0; kill_queue--) {
    result = UnQueueSPBuffer ((unsigned char *)&dummy_sppacket[0]);
    if (result != STATUS_NOERROR) {
      (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [CleanUpBufferQueue]\n");
      return (STATUS_BAD_OPERATION);
    }
  }
  TOPSnum_spbuffered = 0;
  TOPSspqueueptr = NULL;
  return (STATUS_NOERROR);
}


/* GetAsyncRequest ()
  modifies : buffer
  effects : tries to read an OEVENT_Request message, if in
            errordetect mode can receive OEVENT_ErrorTransition
            messages. can also read OEVENT_Message messages.
            
            if reads nothing within 'timeout' period, returns
            STATUS_TIMEDOUT
            
            'max_goes' defines the maximum number of non-
            OEVENT_Request messages that can be received. if this
            value is met, STATUS_TIMED_OUT is returned. 'max_goes'
            of 0 refers to infinite tries until a request is read
            or an error occurs.
*/
static unsigned char GetAsyncRequest (sock, buffer, timeout, max_goes)
int sock;
unsigned char *buffer;
long int timeout;
int max_goes;
{
  enum TimedStatus time_result;
  unsigned char result, response_tag;
  int extra_to_read, bytes_read, packet_size, sppacketsize;
  int goes = 0;
  bool got_data;
  
  while ((max_goes == 0) || (goes < max_goes)) {
    /* read header of linkops response message */
    got_data = false;
    while (got_data == false) {
      time_result = DoTimedSocketRead (sock, OPSMIN_RESPONSEMESSAGESIZE, timeout, (unsigned char *)&response_buffer[0]);
      switch (time_result) {
        case TIMEDSTATUSdata:
          /* data read ok so continue */
          got_data = true;
          break;
      
        case TIMEDSTATUSnodata:
          /* nodata read */
          if (timeout != 0L) {
            return (STATUS_TIMEDOUT);
          }
          break;
      
        case TIMEDSTATUSfailure:
          (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [GetAsyncRequest]\n");
          return (STATUS_COMMS_FATAL);
          break;
      }
    }
    bytes_read = OPSMIN_RESPONSEMESSAGESIZE;
    
    /* deduce packet size, tag & how much more data needs to be read */
    UnsignedInt16.as_bytes[0] = response_buffer[OPSPACKETSIZEFIELD];
    UnsignedInt16.as_bytes[1] = response_buffer[OPSPACKETSIZEFIELD + 1];
    
    packet_size = ntohs (UnsignedInt16.as_short);
    if (packet_size > OPSMAXPACKET) {
      (void) sprintf (lnkops_errorstring, " : module [tops.c], function [GetAsyncRequest]\n -> packet size too large = [%d]\n", packet_size);
      return (STATUS_COMMS_FATAL);
    }
    response_tag = response_buffer[OPSTAGFIELD];
    
    DebugMessage (fprintf (stderr, "Debug       : got tag        [%d]\n", response_tag) );
    DebugMessage (fprintf (stderr, "Debug       : got packetsize [%d]\n", packet_size) );  
    
    switch (response_tag) {
      case OEVENT_Request:
      
        switch (response_buffer[OEVENTREQUEST_status]) {
          case STATUS_NOERROR:
            break;
            
          case STATUS_SP_ERROR:
            (void) sprintf(lnkops_errorstring, " : read nonsense SP request\n");
            return (response_buffer[OEVENTREQUEST_status]);
            break;
            
          default:
            (void) sprintf(lnkops_errorstring, " : module [tops.c], function [TOPS_GetAsyncRequest]\n -> status = [%d]\n",
                  response_buffer[OEVENTREQUEST_status]);
            return (response_buffer[OEVENTREQUEST_status]);
            break;
        }
        
        /* read body of request into user buffer */
        extra_to_read = (packet_size - bytes_read);
        /* calculate sppacketsize */
        UnsignedInt16.as_bytes[0] = response_buffer[OEVENTREQUEST_packetsize];
        UnsignedInt16.as_bytes[1] = response_buffer[OEVENTREQUEST_packetsize + 1];
        sppacketsize = (ntohs(UnsignedInt16.as_short) - OPSRequestEventBasicSize);
        DebugMessage (fprintf (stderr, "Debug       : sppacketsize [%d]\n", sppacketsize) );
            
        if (buffer == NULL) {
          /* no user buffer so read into buffer queue */
          /* check if buffers are full */
          if (TOPSnum_spbuffered > TOPSbuffer_depth) {
            (void) sprintf (lnkops_errorstring, " : can't buffer an sp request - queue full =[%d]\n", TOPSnum_spbuffered);
            return (STATUS_COMMS_FATAL);
          }
          /* setup new buffer & link into queue */
          result = QueueSPBuffer ();
          if (result != STATUS_NOERROR) {
            (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [GetAsyncRequest]\n");
            return (result);
          }
          /* read straight into allocated buffer */
          DebugMessage (fprintf (stderr, "Debug       : read sp request into queue at pos [%d]\n", (TOPSnum_spbuffered - 1)) );
          DebugMessage (fprintf (stderr, "Debug       : sppacketsize [%d]\n", sppacketsize) );
          result = DoSocketRead (sock, sppacketsize, (unsigned char *)&TOPSnewbufptr->sppacket[0]);
          if (result != STATUS_NOERROR) {
            (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [GetAsyncRequest]\n");
            return (result);
          }
          TOPSnewbufptr->spsize = sppacketsize; /* setup spsize field */
          DebugMessage (fprintf (stderr, "Debug       : sptag [%d]\n",  TOPSnewbufptr->sppacket[2]) );
        
        } else {
          /* read into fn supplied buffer */
          result = DoSocketRead (sock, extra_to_read, (unsigned char *)&buffer[0]);
          if (result != STATUS_NOERROR) {
            (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [GetAsyncRequest]\n");
            return (result);
          }
          DebugMessage (fprintf (stderr, "Debug       : sptag [%d]\n", buffer[2]) );
        }
        
        if (result != STATUS_NOERROR) {
          (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [GetAsyncRequest]\n");
          return (result);
        }      
        got_data = true;
        return (result);
        break;
        
        
      case OEVENT_Message:
        result = HandleMessage (sock, packet_size);
        if (result != STATUS_NOERROR) {
          (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called [GetAsyncRequest]\n");
          return (result);
        }
        break;
        
      case OEVENT_ErrorTransition:
        result = HandleErrorTransition (sock, packet_size);
        if (result != STATUS_NOERROR) {
          if (result != STATUS_TARGET_ERROR) {
            (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [GetAsyncRequest]\n");
          }
          return (result);
        }
        break;
        
    }
    goes++;
  }
  return (STATUS_TIMEDOUT);
}

/* GetCommsSynchronousReply ()
    effects : tries to read a OREPLY_CommsSynchronous message when in an asynchronous protocol sub-mode
              handles (by ignoring) OEVENT_ErrorTransitions,
              handles OEVENT_Message messages.
              if an OEVENT_Request event is read, it is thrown away.
              
*/
static unsigned char GetCommsSynchronousReply (sock)
int sock;
{
  unsigned char result, response_tag;
  int extra_to_read, bytes_read, packet_size;
  bool got_commssync_reply = false;
  
  while (got_commssync_reply == false) {
    /* read header of linkops response message */
    result = DoSocketRead (sock, OPSMIN_RESPONSEMESSAGESIZE, (unsigned char *)&response_buffer[0]);
    if (result != STATUS_NOERROR) {
      (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [GetCommsSynchronousReply]\n");
      return (result);
    }
    bytes_read = OPSMIN_RESPONSEMESSAGESIZE;
    
    /* deduce packet size, tag & how much more data needs to be read */
    UnsignedInt16.as_bytes[0] = response_buffer[OPSPACKETSIZEFIELD];
    UnsignedInt16.as_bytes[1] = response_buffer[OPSPACKETSIZEFIELD + 1];
    
    packet_size = ntohs (UnsignedInt16.as_short);
    if (packet_size > OPSMAXPACKET) {
      (void) sprintf (lnkops_errorstring, " : module [tops.c], function [GetCommsSynchronousReply]\n -> packet size too large = [%d]\n", packet_size);
      return (STATUS_COMMS_FATAL);
    }
    response_tag = response_buffer[OPSTAGFIELD];
    
    DebugMessage (fprintf (stderr, "Debug       : got tag        [%d]\n", response_tag) );
    DebugMessage (fprintf (stderr, "Debug       : got packetsize [%d]\n", packet_size) );  
    
    switch (response_tag) {
      case OREPLY_CommsSynchronous:
        got_commssync_reply = true;
        break;
        
      case OEVENT_Message:
        result = HandleMessage (sock, packet_size);
        if (result != STATUS_NOERROR) {
          (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [GetCommsSynchronousReply]\n");
          return (result);
        }
        break;
        
      case OEVENT_ErrorTransition:
        /* read rest of message response */
        extra_to_read = (packet_size - bytes_read);
        result = DoSocketRead (sock, extra_to_read, (unsigned char *)&response_buffer[bytes_read]);
        if (result != STATUS_NOERROR) {
         (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [GetCommsSynchronousReply]\n");
          return (result);
        }      
        if (TOPSerrordetect == false) {
          (void) sprintf (lnkops_errorstring, " : module [tops.c], function [GetCommsSynchronousReply]\n -> not in errordetect mode");
          return (STATUS_COMMS_FATAL);
        }
        /* check processor id */
        UnsignedInt32.as_bytes[0] = response_buffer[OEVENTERRORTRANS_processorid];
        UnsignedInt32.as_bytes[1] = response_buffer[OEVENTERRORTRANS_processorid+1];
        UnsignedInt32.as_bytes[2] = response_buffer[OEVENTERRORTRANS_processorid+2];
        UnsignedInt32.as_bytes[3] = response_buffer[OEVENTERRORTRANS_processorid+3];
        if (ntohl(UnsignedInt32.as_long) != TSERIES_PROCESSORID) {
          (void) sprintf (lnkops_errorstring, " : module [tops.c], function [GetCommsSynchronousReply]\n -> bad processor_id (T-series implementation)\n");
          return (STATUS_COMMS_FATAL);
        }
        /* check if error flag was set */
        if ((bool) response_buffer[OEVENTERRORTRANS_state] == true) {
          DebugMessage (fprintf (stderr, "Debug       : transputer error flag set\n") );
          return (STATUS_TARGET_ERROR);
        } else {
          DebugMessage (fprintf (stderr, "Debug       : transputer error flag clear\n") );
        }
        break;
        
      case OEVENT_Request:
        /* read body of request into response buffer to throw it away */
        DebugMessage (fprintf (stderr, "Debug       : got OEVENT_REQUEST, throw it away\n") );
        extra_to_read = (packet_size - bytes_read);
        result = DoSocketRead (sock, extra_to_read, (unsigned char *)&response_buffer[bytes_read]);
        if (result != STATUS_NOERROR) {
          (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [GetCommsSynchronousReply]\n");
          return (result);
        }      
        break;
    }
  }
  return (STATUS_NOERROR);
}

/* WaitForReply ()
    effects : waits for a particular reply, if nothing read within WAIT_TIMEOUT returns
              STATUS_TIMEDOUT. throws away everything except tag waiting for - however
              if a message event arrives it will be printed.
*/
static unsigned char WaitForReply (sock, wanted_tag)
int sock;
unsigned char wanted_tag;
{
  enum TimedStatus timed_result;
  unsigned char result, response_tag;
  int extra_to_read, bytes_read, packet_size;
  bool got_reply = false;
  
  while (got_reply == false) {
    /* read header of linkops response message */
    timed_result = DoTimedSocketRead (sock, OPSMIN_RESPONSEMESSAGESIZE, WAIT_TIMEOUT, (unsigned char *)&response_buffer[0]);
    
    if (timed_result != TIMEDSTATUSdata) {
      if (timed_result == TIMEDSTATUSnodata) {
        (void) sprintf (lnkops_errorstring, " : no response from server (timed out)\n");
      } else {
        (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [WaitForReply]\n");
      }
      return (STATUS_BAD_OPERATION);
    }
    
    bytes_read = OPSMIN_RESPONSEMESSAGESIZE;
    /* deduce packet size, tag & how much more data needs to be read */
    UnsignedInt16.as_bytes[0] = response_buffer[OPSPACKETSIZEFIELD];
    UnsignedInt16.as_bytes[1] = response_buffer[OPSPACKETSIZEFIELD + 1];
    
    packet_size = ntohs (UnsignedInt16.as_short);
    if (packet_size > OPSMAXPACKET) {
      (void) sprintf (lnkops_errorstring, " : module [tops.c], function [WaitForReply]\n -> packet size too large = [%d]\n", packet_size);
      return (STATUS_COMMS_FATAL);
    }
    response_tag = response_buffer[OPSTAGFIELD];
    DebugMessage (fprintf (stderr, "Debug       : tag        [%d]\n", response_tag) );
    DebugMessage (fprintf (stderr, "Debug       : packetsize [%d]\n", packet_size) );

    
    switch (response_tag) {
      case OEVENT_Message:
        result = HandleMessage (sock, packet_size);
        if (result != STATUS_NOERROR) {
          (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [WaitForReply]\n");
          return (result);
        }
        break;
        
      default:
        /* read rest of message */
        extra_to_read = (packet_size - bytes_read);
        result = DoSocketRead (sock, extra_to_read, (unsigned char *)&response_buffer[bytes_read]);
        if ((result != STATUS_NOERROR) && (result != STATUS_TARGET_ERROR)) {
          (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [WaitForReply]\n");
          return (result);
        }
        break;
    }
    got_reply = (response_tag == wanted_tag);
  }
  return (STATUS_NOERROR);
}

/* 
  requires : in Synchronous protocol state 
  effects  : returns STATUS_NOERROR if error flag clear
                     STATUS_TARGET_ERROR if error flag set
                     STATUS_COMMS_FATAL if error occurs
*/
static unsigned char CheckError (sock, proc_id)
int sock;
long int proc_id;
{
  struct OPSTestErrorCommand testerror_cmd;
  int result;
  
  DebugMessage (fprintf (stderr, "Debug       : sending testerror command\n") );
  
  testerror_cmd.packet_size = htons(OPSTestErrorCommandSize);
  testerror_cmd.command_tag = OCMD_TestError;
  SignedInt32.as_long = htonl((u_long) proc_id);
  testerror_cmd.processor_id[0] = SignedInt32.as_bytes[0];
  testerror_cmd.processor_id[1] = SignedInt32.as_bytes[1];
  testerror_cmd.processor_id[2] = SignedInt32.as_bytes[2];
  testerror_cmd.processor_id[3] = SignedInt32.as_bytes[3];
  
  if (DoSocketWrite (sock, OPSTestErrorCommandSize, (char *) &testerror_cmd) != STATUS_NOERROR) {
    (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [CheckError]\n");
    return (STATUS_COMMS_FATAL);
  }
  
  /* alter protocol state */
  TOPSprotocolstate = SynchronousReply;
  
  /* get reply message */
  DebugMessage (fprintf (stderr, "Debug       : getting testerror reply\n") );
  result = GetSyncResponse (sock, OREPLY_TestError, (unsigned char *) NULL);
  if (result != STATUS_NOERROR) {
    (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [CheckError]\n");
    return (result);
  }
  DebugMessage (fprintf (stderr, "Debug       : received correct testerror reply\n") );
  
  /* alter protocol state */
  TOPSprotocolstate = Synchronous;
  
  /* check status field of reply */
  switch (response_buffer[OREPLYTESTERROR_status]) {
    case STATUS_NOERROR:
      return (STATUS_NOERROR);
      break;
    
    case STATUS_TARGET_ERROR:
      /* check processor id field  */
      UnsignedInt32.as_bytes[0] = response_buffer[OREPLYPEEK32_processorid];
      UnsignedInt32.as_bytes[1] = response_buffer[OREPLYPEEK32_processorid + 1];
      UnsignedInt32.as_bytes[2] = response_buffer[OREPLYPEEK32_processorid + 2];
      UnsignedInt32.as_bytes[3] = response_buffer[OREPLYPEEK32_processorid + 3];
      if (ntohl(UnsignedInt32.as_long) != proc_id) {
        (void) sprintf (lnkops_errorstring, " : module [tops.c], function [CheckError]\n -> bad processor_id (T-series implementation)\n");
        return (STATUS_BAD_OPERATION);
      } else {
        return (STATUS_TARGET_ERROR);
      }
      
    default:
      (void) sprintf (lnkops_errorstring, " : module [tops.c], function [CheckError]\n -> panic!\n");
      return (STATUS_COMMS_FATAL);
      break;
  }
  return (-99); /* to keep lint happy */
}

/*
   modifies : nothing
   effects  : returns STATUS_NOERROR if data can be read from sock
              returns STATUS_TIMED_OUT if data cant be read
              return STATUS_COMMS_FATAL if error occurs
*/
static unsigned char TestResponseAvailable (sock)
int sock;
{
#ifdef DECTCP
  int read_fds;
#else  /* DECTCP */
  fd_set read_fds;
#endif /* DECTCP */

  int fdwidth, result;
  struct timeval wait;
  
  wait.tv_sec = 0;
  wait.tv_usec = 10; /* check for 10microseconds */
  
#ifdef DECTCP
  read_fds = (1 << sock);
#else
  FD_ZERO (&read_fds);
  FD_SET (sock, &read_fds);
#endif
  
  fdwidth = 32;
#ifdef DECTCP
  result = select (fdwidth, (int *)&read_fds, (int *) NULL, (int *) NULL, &wait);
#else
  result = select (fdwidth, &read_fds, (fd_set *) NULL, (fd_set *) NULL, &wait);
#endif /* DECTCP */
  if (result < 0) {
#ifdef PCNFS
    if (ERRNO == EINTR)
        int86(0x1b,&dos_regs,&dos_regs); /* Generate a 'break' interrupt */
#endif
    (void) sprintf(lnkops_errorstring, " : module [tops.c], function [TestResponseAvailable]\n -> select failed");
    return (STATUS_COMMS_FATAL);
  }
  if (result > 0) {
    DebugMessage (fprintf (stderr, "Debug       : response available\n") );
    return (STATUS_NOERROR);
  } else {
    return (STATUS_TIMEDOUT);
  }
}


/**
 ** exported functions
 **/

/* TOPS_Open
   requires : TOPSprotocolstate = Closed
   modifies : con_id, link_name, TOPSprotocolstate
   effects  : if a fatal error occurs, returns
                STATUS_BAD_COMMS_MODE or STATUS_COMMS_FATAL
                (lnkops_errorstring suitably modified)
                
              else returns
                STATUS_NOERROR (opened ok)
                
              else returns
                STATUS_BAD_OPERATION or STATUS_BAD_SERVER or STATUS_TARGET_UNAVAILABLE
                  (informational messages give clue)
*/
unsigned char TOPS_Open (resource_name, server_name, buffer_depth, con_id, localmethod, link_name)
char *resource_name;
char *server_name;
int buffer_depth;
long int *con_id;
int *localmethod;
char *link_name;
{

  int sock, param_count, result, num_a, num_b, num_c, num_d, i;
  struct sockaddr_in ops_server;
  struct hostent *hostptr, *gethostbyname();
  struct servent *servptr;
  char service_string[40];
  
  enum TimedStatus timed_result;
  struct OPSOpenCommand open_cmd;
  struct OPSOpenReply open_reply;
  
  DebugMessage (fprintf (stderr, "[TOPS_Open\n") );
  
  
  (void) strcpy (lnkops_errorstring, NULSTRING);
  
  if (TOPSprotocolstate != Closed) {
    (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_Open]\n -> linkops protocol error, bad protocol mode\n");
    return (STATUS_BAD_COMMS_MODE);
  }
  
#ifdef DECTCP
  ops_server.sin_port = htons(LINKOPS_PORT);
  InfoMessage ( fprintf (stderr, "Info        : using port [%d] as no services database under Ultrix\n", LINKOPS_PORT) );
#else
  if ((servptr = getservbyname (SERVICENAME, SERVICEPROTOCOL)) == NULL) {
    (void) sprintf(lnkops_errorstring, " : linkops service is not defined in the network service database\n");
    return (STATUS_COMMS_FATAL);
  }
  ops_server.sin_port = servptr -> s_port;
#endif /* DECTCP */
  
  /* initialise sp request queue */
  TOPSbuffer_depth = buffer_depth;
  TOPSnum_spbuffered = 0;
  TOPSspqueueptr = NULL;
  
#ifdef REDUNDANT
  This was the old method of setting up size of send/receive buffers
  under PC-NFS. Version 2.0 conforms to the standard method.
  {
    int parameters[5];
    parameters[0] = 1024; /* send mms */
    parameters[1] = 1024; /* recv mms */
    parameters[2] = 9216; /* send window */
    parameters[3] = 9216; /* recv window */
    parameters[4] = 0;    /* ack delay */
    setpparm (IPPROTO_TCP, (char *)&parameters[0]);
  }
#endif /* REDUNDANT */
  /* create an Internet, stream, IP socket */
  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
#ifdef PCNFS
    if (ERRNO == EINTR)
        int86(0x1b,&dos_regs,&dos_regs); /* Generate a 'break' interrupt */
#endif
    (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_Open]\n -> socket failed\n");
    return (STATUS_COMMS_FATAL);
  }
  
  /* set size of send & receive buffers */
#ifndef DECTCP
  {
    int ReceiveSize = FRAG_SIZE + 1024;
    int SendSize = FRAG_SIZE + 1024;        
    
    if (setsockopt (sock, SOL_SOCKET, SO_RCVBUF, (char *)&ReceiveSize, sizeof(ReceiveSize)) != 0) {
      (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_Open]\n -> setsockopt(REC) failed\n");
      return (STATUS_COMMS_FATAL);
    }
    if (setsockopt (sock, SOL_SOCKET, SO_SNDBUF, (char *)&SendSize, sizeof(SendSize)) != 0) {
      (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_Open]\n -> setsockopt(SEND) failed\n");
      return (STATUS_COMMS_FATAL);
    }
    
  }
  
#ifdef NODELAY
  {
    struct protoent *pp;    
    int NoDelay = 1;
                            
    pp = getprotobyname ("tcp");
    if (setsockopt (sock, pp->p_proto, TCP_NODELAY, (char *)&NoDelay, sizeof(NoDelay)) != 0) {
      (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_Open]\n -> setsockopt(NODELAY) failed\n");
      return (STATUS_COMMS_FATAL);
    }
    
  }
#endif /* NODELAY */

#endif /* not DECTCP */
  
  /**
  *** connect socket to remote machine 
  **/
  
  /* determine whether server = internet address or internet name */
  param_count = sscanf(server_name, "%d.%d.%d.%d", &num_a, &num_b, &num_c, &num_d);
  if (param_count == 4) {
    DebugMessage (fprintf (stderr, "-> server = internet address\n") );
    SignedInt32.as_bytes[0] = num_a;
    SignedInt32.as_bytes[1] = num_b;
    SignedInt32.as_bytes[2] = num_c;
    SignedInt32.as_bytes[3] = num_d;
    ops_server.sin_addr.s_addr = SignedInt32.as_long;
    
  } else {
    /* get internet address from network table */
    hostptr = gethostbyname (server_name);   
    if (hostptr == NULL) {
      InfoMessage ( fprintf (stderr, "Warning     : machine [%s] not defined in host's network database\n", server_name) );
      (void) close(sock);
      return (STATUS_BAD_SERVER);
    } else {
      /* copy address from network table into sockaddr structure */
      
      memcpy ((char *) &ops_server.sin_addr,
              (char *) hostptr -> h_addr,
             hostptr -> h_length);
    }
  }
  
  ops_server.sin_family = AF_INET;
  
  InfoMessage ( fprintf (stderr, "Info        : attempting TCP open to resource [%s@%s]\n", resource_name, server_name) );
  
  /* can't use select to check for write activity so just do plain
     boring connect - this will not fail quickly - it will take the
     full 3 minutes */
  result = connect (sock, (struct sockaddr *) &ops_server, sizeof (ops_server) );
  if (result < 0) {
#ifdef PCNFS
    if (ERRNO == EINTR)
        int86(0x1b,&dos_regs,&dos_regs); /* Generate a 'break' interrupt */
#endif
    switch (ERRNO) {
      case ETIMEDOUT:
      case ENETUNREACH:
      case EHOSTUNREACH:
      case ECONNREFUSED:
        InfoMessage ( fprintf (stderr, "Warning     : server [%s] can't be contacted\n", server_name) );
        (void) shutdown (sock, 2);
        (void) close (sock);
        return (STATUS_BAD_SERVER);
        break;
      
      default:
        InfoMessage ( fprintf (stderr, "Warning     : server [%s] will not connect\n", server_name) );
        (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_Open]\n -> connect1 errno=[%d]\n", ERRNO);
        (void) shutdown (sock, 2);
        (void) close (sock);
        return (STATUS_BAD_SERVER);
        break;
    }
  }

  DebugMessage (fprintf (stderr, "Debug       : connection to [%s] established\n", server_name) );
  
  /* send linkops open message to see if ok */
  
  /* setup service string as 40 byte space padded */
  (void) strcpy (service_string, SERVICE_OPSOPEN);
  for (i = strlen(service_string); i < 40; i++) {
    service_string[i] = ' ';
  }
  
  for (i = 0; i < 40; i++) {
    open_cmd.service_message[i] = service_string[i];
  }
  open_cmd.command_tag = OCMD_Open;
  (void) strncpy (open_cmd.resource_name, resource_name, 64);
  
  if (DoSocketWrite (sock, OPSOpenCmdSize, (char *) &open_cmd) != STATUS_NOERROR) {
    (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_Open]\n");
    (void) shutdown (sock, 2);
    (void) close (sock);
    return (STATUS_COMMS_FATAL);
  }
  
  /* modify protocol mode */
  TOPSprotocolstate = OpenCheck;
  
  /**
   ** read linkops open reply message
  **/
  /* read 1st 2 bytes of service string */
  timed_result = DoTimedSocketRead (sock, 2, READOPEN1_TIMEOUT, (unsigned char *) &open_reply.service_message[0]);
  if (timed_result != TIMEDSTATUSdata) {
    if (timed_result == TIMEDSTATUSnodata) {
      (void) sprintf (lnkops_errorstring, " : no response from server (timed out)\n");
    } else {                                                            
      (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_Open]\n");
    }
    (void) shutdown (sock, 2);
    (void) close (sock);
    TOPSprotocolstate = Closed;
    return (STATUS_BAD_OPERATION);
  }
  
  if ((open_reply.service_message[0] == '-') &&
     (open_reply.service_message[1] == '\n')) {
    InfoMessage ( fprintf (stderr, "Warning     : machine [%s] does not understand open call\n", server_name) );
    (void) shutdown (sock, 2);
    (void) close (sock);
    TOPSprotocolstate = Closed;
    return (STATUS_BAD_OPERATION);
  }
  
  /* read rest of service string + reply.tag */
  timed_result = DoTimedSocketRead (sock, (41 - 2), READOPEN2_TIMEOUT, (unsigned char *) &open_reply.service_message[2]);
  if (timed_result != TIMEDSTATUSdata) {
    if (timed_result == TIMEDSTATUSnodata) {
      (void) sprintf (lnkops_errorstring, " : no response from server  (timed out)\n");
    } else {
      (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_Open]\n");
    }
    (void) shutdown (sock, 2);
    (void) close (sock);
    TOPSprotocolstate = Closed;
    return (STATUS_BAD_OPERATION);
  }
  for (i = 0; i < 40; i++) {
    if (open_reply.service_message[i] != service_string[i]) {
      InfoMessage ( fprintf (stderr, "Warning     : machine [%s] does not understand open call\n", server_name) );
      (void) shutdown (sock, 2);
      (void) close (sock);
      TOPSprotocolstate = Closed;
      return (STATUS_BAD_OPERATION);
    }   
  }
  
  switch (open_reply.reply_tag) {
    case OREPLY_Open:
      break;
      
    case INMOSREPLY_BadService:
      InfoMessage ( fprintf (stderr, "Warning     : machine [%s] does not support linkops service\n", server_name) );
      (void) shutdown (sock, 2);
      (void) close (sock);
      TOPSprotocolstate = Closed;
      return (STATUS_BAD_OPERATION);
      break;
      
    case INMOSREPLY_BadVersion:
      InfoMessage ( fprintf (stderr, "Warning     : machine [%s] can't cope with this version of linkops\n", server_name) );
      (void) shutdown (sock, 2);
      (void) close (sock);
      TOPSprotocolstate = Closed;
      return (STATUS_BAD_OPERATION);
      break;
      
    default:
      (void) sprintf(lnkops_errorstring, " : module [tops.c], function [TOPS_Open]\n -> linkops protocol error, reply_tag = [%d]\n", open_reply.reply_tag);
      InfoMessage ( fprintf (stderr, "Warning     : linkops protocol error, reply_tag = [%d]\n", (int) open_reply.reply_tag) );
      (void) shutdown (sock, 2);
      (void) close (sock);
      TOPSprotocolstate = Closed;
      return (STATUS_BAD_OPERATION);
      break;
  }
  
  /* read status & device name fields */
  timed_result = DoTimedSocketRead (sock, (OPSOpenReplySize - 41), READOPEN2_TIMEOUT, (unsigned char *) &open_reply.status);
  if (timed_result != TIMEDSTATUSdata) {
    if (timed_result == TIMEDSTATUSnodata) {
      (void) sprintf (lnkops_errorstring, " : no response from server (timed out)\n");
    } else {
      (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_Open]\n");
    }
    (void) shutdown (sock, 2);
    (void) close (sock);
    TOPSprotocolstate = Closed;
    return (STATUS_BAD_OPERATION);
  }
  switch (open_reply.status) {
    case STATUS_NOERROR:
      break;
      
    case STATUS_BAD_TARGET_NAME:
      InfoMessage ( fprintf (stderr, "Warning     : server [%s]'s database does not contain resource [%s]\n", server_name, resource_name) );
      (void) shutdown (sock, 2);
      (void) close (sock);
      TOPSprotocolstate = Closed;
      return (STATUS_BAD_TARGET_NAME);
      break;
      
    case STATUS_TARGET_UNAVAILABLE:
      InfoMessage ( fprintf (stderr, "Warning     : resource [%s@%s] is unavailable\n", resource_name, server_name) );
      (void) shutdown (sock, 2);
      (void) close (sock);
      TOPSprotocolstate = Closed;
      return (STATUS_TARGET_UNAVAILABLE);
      break;
      
    case STATUS_BAD_METHOD:
      InfoMessage ( fprintf (stderr, "Warning     : remote database method field is illegal\n") );
      (void) shutdown (sock, 2);
      (void) close (sock);
      TOPSprotocolstate = Closed;
      return (STATUS_TARGET_UNAVAILABLE);
      break;
      
    default:
      (void) sprintf(lnkops_errorstring, " : module [tops.c], function [TOPS_Open]\n -> linkops protocol error, undefined status field = [%d]\n", open_reply.status);
      (void) shutdown (sock, 2);
      (void) close (sock);
      TOPSprotocolstate = Closed;
      return (STATUS_BAD_OPERATION);
      break;
  }
  
  /* modify protocol state */
  TOPSprotocolstate = Synchronous;
  /* fill in return values */
  *con_id = (long int) sock;
  (void) strcpy (link_name, open_reply.device_name);
  
  *localmethod = TCP_LINKOPS;
  DebugMessage (fprintf (stderr, "]TOPS_Open\n") );
  return (STATUS_NOERROR);
  
}

/*
   modifies :
      TOPSprotocolstate = Closed;
      TOPSerrordetect = false;
      TOPSnum_spbuffered = 0;
*/
unsigned char TOPS_Close (con_id)
long int con_id;
{
  struct OPSCloseCommand close_cmd;
  enum ProtocolStates nextstate;
  unsigned char result;
  
  DebugMessage (fprintf (stderr, "[TOPS_Close\n") );
  
  switch (TOPSprotocolstate) {
    case Synchronous:
      nextstate = SyncClosing;
      break;
      
    case SynchronousReply:
      nextstate = SyncReplyClosing;
      break;
      
    case Asynchronous:
      nextstate = AsyncClosing;
      break;
      
    case CancelAsynchronous:
      nextstate = CancelAsyncClosing;
      break;
      
    case Restarting:
      nextstate = Restarting;
      break;
      
    default:
      DebugMessage (fprintf (stderr, "Debug       : not sending close command (not in correct mode)\n") );
      (void) shutdown ((int) con_id, 2);
      (void) close ((int) con_id);            
      TOPSprotocolstate = Closed;
      TOPSerrordetect = false;
      
      result = CleanUpBufferQueue ();
      if (result != STATUS_NOERROR) {
        (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_Close]");
        return (result);
      } else {
        DebugMessage (fprintf (stderr, "]TOPS_Close\n") );
        return (STATUS_NOERROR);
      }
      break;
  }
      
  /* valid states to issue a close command */
  close_cmd.packet_size = htons(OPSCloseCommandSize);
  close_cmd.command_tag = OCMD_Close;
  DebugMessage (fprintf (stderr, "-> sending close command, packetsize=[%d]\n", ntohs(close_cmd.packet_size)) );

  if (DoSocketWrite ((int) con_id, OPSCloseCommandSize, (char *) &close_cmd) != STATUS_NOERROR) {
    (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_Close]\n");
    (void) shutdown ((int) con_id, 2);
    (void) close ((int) con_id);
    TOPSprotocolstate = Closed;
    TOPSerrordetect = false;
    result = CleanUpBufferQueue ();
    if (result != STATUS_NOERROR) {
      (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_Close]");
      return (result);
    } else {
      return (STATUS_COMMS_FATAL);
    }
  }    
  
  /* alter protocol state */
  TOPSprotocolstate = nextstate;

  /* get reply */
  DebugMessage (fprintf (stderr, "Debug       : getting close reply\n") );
  result = WaitForReply ((int) con_id, OREPLY_Close);
  /* if reply received ok, return status field of reply */
  if (result == STATUS_NOERROR) {
    result = response_buffer[OREPLYCLOSE_status];
    switch (result) {
      case STATUS_NOERROR:
        break;
        
      case STATUS_BAD_OPERATION:
        (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_Close]\n -> remote operation failed\n");
        break;
      
      case STATUS_LINK_FATAL:
        (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_Close]\n -> remote link operation failed\n");
        break;
      
      default:
        (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_Close]\n -> linkops protocol error, got unexpected status = [%d]\n", result);
        break;
    }
  } else {
    (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_Close]\n");
  }

  /* alter protocol state & close socket */
  TOPSprotocolstate = Closed;
  DebugMessage (fprintf (stderr, "Debug       : closing socket\n") );
  (void) shutdown ((int) con_id, 2);
  (void) close ((int) con_id); 
  TOPSprotocolstate = Closed;
  TOPSerrordetect = false;
  result = CleanUpBufferQueue ();
  if (result != STATUS_NOERROR) {
    (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_Close]");
    return (result);
  } else {
    DebugMessage (fprintf (stderr, "]TOPS_Close\n") );
    return (result);
  }
}

/*
  modifies : commsmode, TOPSprotocolstate
*/
unsigned char TOPS_CommsAsynchronous (con_id, commsmode, errormode)
long int con_id;          
enum CommsModes *commsmode;
enum ErrorModes errormode;
{
  struct OPSCommsAsynchronousCommand commsasync_cmd;
  unsigned char result;
  
  DebugMessage (fprintf (stderr, "[TOPS_CommsAsynchronous\n") );
  if ((TOPSprotocolstate == Synchronous) || (TOPSprotocolstate == Asynchronous)) {
    /* valid state to issue command */
    commsasync_cmd.command_tag = OCMD_CommsAsynchronous; 
    commsasync_cmd.packet_size = htons(OPSCommsAsynchronousCommandSize);
    DebugMessage (fprintf (stderr, "Debug       : sending commsasynchronous command, packetsize=[%d]\n", ntohs(commsasync_cmd.packet_size)) );
  
    if (DoSocketWrite ((int) con_id, OPSCommsAsynchronousCommandSize, (char *) &commsasync_cmd) != STATUS_NOERROR) {
      (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_CommsAsynchronous]\n");
      return (STATUS_COMMS_FATAL);
    }
    *commsmode = COMMSasynchronous;
    TOPSprotocolstate = Asynchronous;
    
    if (TOPSnum_spbuffered < TOPSbuffer_depth) {
      /* while response available, handle them until an error occurs 
       or no more responses can be received */
      while ( (result = TestResponseAvailable ((int) con_id )) == STATUS_NOERROR) {
        result = GetAsyncRequest ((int) con_id, (unsigned char *)NULL, 0L, 1);
        switch (result) {
          case STATUS_NOERROR:
            break;
            
          case STATUS_TIMEDOUT:
            DebugMessage (fprintf (stderr, "]TOPS_CommsAsynchronous\n") );
            (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_CommsAsynchronous]\n");
            return (STATUS_NOERROR);
            break;
            
          case STATUS_TARGET_ERROR:
            DebugMessage (fprintf (stderr, "]TOPS_CommsAsynchronous\n") );
            return (result);
            break;
            
          default:
            DebugMessage (fprintf (stderr, "]TOPS_CommsAsynchronous\n") );
            (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_CommsAsynchronous]\n");
            return (result);
            break;
            
        }
      }
      DebugMessage (fprintf (stderr, "]TOPS_CommsAsynchronous\n") );
      if (result != STATUS_TIMEDOUT) {
        (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_CommsAsynchronous]\n");
        return (result);
      }
      return (STATUS_NOERROR);
      
    } else {
      DebugMessage (fprintf (stderr, "Debug       : spqueue full so not checking network\n") );
      DebugMessage (fprintf (stderr, "]TOPS_CommsAsynchronous\n") );
      return (STATUS_NOERROR);
    }
    
  } else {
    (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_CommsAsynchronous]\n -> linkops protocol error, bad protocol mode\n");
    return (STATUS_BAD_COMMS_MODE);
  }
}

/*
  modifies : commsmode, TOPSprotocolstate
*/
unsigned char TOPS_CommsSynchronous (con_id, commsmode, errormode)
long int con_id;
enum CommsModes *commsmode;
enum ErrorModes errormode;
{
  struct OPSCommsSynchronousCommand commssync_cmd;
  enum ProtocolStates nextstate;
  unsigned char result;
  
  DebugMessage (fprintf (stderr, "[TOPS_CommsSynchronous\n") );
  switch (TOPSprotocolstate) {
    case Synchronous:
      nextstate = SynchronousReply;
      break;
      
    case Asynchronous:
      nextstate = CancelAsynchronous;
      break;
      
    default:
      (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_CommsSynchronous]\n -> linkops protocol error, bad protocol mode\n");
      return (STATUS_BAD_COMMS_MODE);
      break;
  }
    
  /* valid state to issue command */
  commssync_cmd.packet_size = htons(OPSCommsSynchronousCommandSize);
  commssync_cmd.command_tag = OCMD_CommsSynchronous; 

  if (DoSocketWrite ((int) con_id, OPSCommsSynchronousCommandSize, (char *) &commssync_cmd) != STATUS_NOERROR) {
    (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_CommsSynchronous]\n");
    return (STATUS_COMMS_FATAL);
  }
  /* alter protocols state */
  TOPSprotocolstate = nextstate;
  
  /* get reply */
  DebugMessage (fprintf (stderr, "Debug       : getting commssynchronous reply\n") );
  result = GetCommsSynchronousReply ((int) con_id);
  /* if reply received ok, return status field of reply */
  if (result == STATUS_NOERROR) {
    result = response_buffer[OREPLYCOMMSSYNCHRONOUS_status];
    switch (result) {
      case STATUS_NOERROR:
        TOPSprotocolstate = Synchronous;
        *commsmode = COMMSsynchronous;
        DebugMessage (fprintf (stderr, "Debug       : errormode=[%d]\n", (int) errormode) );
        if (errormode == ERRORdetect) {
          result = CheckError ((int) con_id, TSERIES_PROCESSORID);
          if (result != STATUS_NOERROR) {
            (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_CommsSynchronous]\n");
          }
        } 
        break;
        
      case STATUS_BAD_OPERATION:
        (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_GetCommsSynchronous]\n -> remote operation failed\n");
        break;
      
      case STATUS_LINK_FATAL:
        (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_GetCommsSynchronous]\n -> remote link operation failed\n");
        break;
      
      default:
        (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_GetCommsSynchronous]\n -> linkops protocol error, got unexpected status = [%d]\n", result);
        break;
    }
  } else {
    if (result == STATUS_TARGET_ERROR) {
      TOPSprotocolstate = Synchronous;
      *commsmode = COMMSsynchronous;
    } else {
      (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_CommsSynchronous]\n");
    }
  }
  DebugMessage (fprintf (stderr, "]TOPS_CommsSynchronous\n") );
  return (result);
}

/*
   modifies : errormode, TOPSerrordetect
*/
unsigned char TOPS_ErrorDetect (con_id, errormode)
long int con_id;
enum ErrorModes *errormode;
{
  struct OPSErrorDetectCommand errordetect_cmd;
  
  DebugMessage (fprintf (stderr, "[TOPS_ErrorDetect\n") );
  if (TOPSprotocolstate == Synchronous) {
    DebugMessage (fprintf (stderr, "Debug       : errormode=[%d] (before)\n", *errormode) );
                                                  
    /* valid state to issue command */
    errordetect_cmd.packet_size = htons(OPSErrorDetectCommandSize);
    errordetect_cmd.command_tag = OCMD_ErrorDetect;
    
    if (DoSocketWrite ((int) con_id, OPSErrorDetectCommandSize, (char *) &errordetect_cmd) != STATUS_NOERROR) {
      (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_ErrorDetect]\n");
      return (STATUS_COMMS_FATAL);
    } else {
      *errormode = ERRORdetect;
      TOPSerrordetect = true;
      DebugMessage (fprintf (stderr, "Debug       : errormode=[%d] (after)\n", *errormode) );
      DebugMessage (fprintf (stderr, "]TOPS_ErrorDetect\n") );
      return (STATUS_NOERROR);
    }
    
  } else {
    (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_ErrorDetect]\n -> linkops protocol error, bad protocol mode\n");
    return (STATUS_BAD_COMMS_MODE);
  }
}

unsigned char TOPS_ErrorIgnore (con_id, errormode)
long int con_id;
enum ErrorModes *errormode;
{
  struct OPSErrorIgnoreCommand errorignore_cmd;
  
  DebugMessage (fprintf (stderr, "[TOPS_ErrorIgnore\n") );
  if (TOPSprotocolstate == Synchronous) {
    /* valid state to issue command */
    errorignore_cmd.packet_size = htons(OPSErrorIgnoreCommandSize);
    errorignore_cmd.command_tag = OCMD_ErrorIgnore;
    
    DebugMessage (fprintf (stderr, "Debug       : errormode=[%d] (before)\n", *errormode) );
    
    if (DoSocketWrite ((int) con_id, OPSErrorIgnoreCommandSize, (char *) &errorignore_cmd) != STATUS_NOERROR) {
      (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_ErrorIgnore]\n");
      return (STATUS_COMMS_FATAL);
    } else {
      *errormode = ERRORignore;
      TOPSerrordetect = false;
      DebugMessage (fprintf (stderr, "Debug       : errormode=[%d] (after)\n", *errormode) );
      DebugMessage (fprintf (stderr, "]TOPS_ErrorIgnore\n") );
      return (STATUS_NOERROR);
    }
    
  } else {
    (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_ErrorIgnore]\n -> linkops protocol error, bad protocol mode\n");
    return (STATUS_BAD_COMMS_MODE);
  }
}

/* modifies : TOPSprotocolstate */
unsigned char TOPS_BootWrite (con_id, buffer, num_bytes, errormode)
long int con_id;
unsigned char *buffer;
unsigned long num_bytes;
enum ErrorModes errormode;
{
  
#define BOOTWRITETIMEOUT 0

  struct OPSWriteLinkCommand writelink_cmd;
  int bytes_to_send, bytes_sent, fragmented_send, bytes_written;
  unsigned char result;
  
  DebugMessage (fprintf (stderr, "[TOPS_BootWrite\n") );
  
  /* check in valid protocol state */
  if (TOPSprotocolstate != Synchronous) {
    (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_BootWrite]\n -> linkops protocol error, bad protocol mode\n");
    return (STATUS_BAD_COMMS_MODE);
  }
    
  bytes_sent = 0;
  bytes_to_send = num_bytes;
  
  while (bytes_to_send > 0) {
    if (bytes_to_send >= FRAG_SIZE) {
      fragmented_send = FRAG_SIZE;
    } else {
      fragmented_send = bytes_to_send;
    }
    writelink_cmd.packet_size = htons((u_short) (OPSWriteLinkCommandBasicSize + fragmented_send));
    writelink_cmd.command_tag = OCMD_WriteLink;
    SignedInt16.as_short = htons(BOOTWRITETIMEOUT);
    writelink_cmd.timeout[0] = SignedInt16.as_bytes[0];
    writelink_cmd.timeout[1] = SignedInt16.as_bytes[1];
    
    DebugMessage (fprintf (stderr, "Debug       : sending writelink (header only), packetsize=[%d]\n",
                            ntohs(writelink_cmd.packet_size)) );
    /* send header fields of command */
    if (DoSocketWrite ((int) con_id, OPSWriteLinkCommandBasicSize, (char *) &writelink_cmd) != STATUS_NOERROR) {
      (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_BootWrite]\n");
      return (STATUS_COMMS_FATAL);
    }    
    DebugMessage (fprintf (stderr, "Debug       : sending writelink (data only), packetsize=[%d]\n", fragmented_send) );
    /* send actual data field of command */
    if (DoSocketWrite ((int) con_id, fragmented_send, (char *) &buffer[bytes_sent]) != STATUS_NOERROR) {
      (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_BootWrite]\n");
      return (STATUS_COMMS_FATAL);
    }    
    
    bytes_to_send = bytes_to_send - fragmented_send;
    bytes_sent = bytes_sent + fragmented_send;
    
    /* alter protocol state */
    TOPSprotocolstate = SynchronousReply;
    
    /* get reply message */
    DebugMessage (fprintf (stderr, "Debug       : getting writelink reply\n") );
    result = GetSyncResponse ((int) con_id, OREPLY_WriteLink, (unsigned char *) NULL);
    /* check reply received ok */
    if (result != STATUS_NOERROR) {
      (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_BootWrite]\n");
      return (result);
    }
    
    /* alter protocol state */
    TOPSprotocolstate = Synchronous;
    
    /* check status field of reply */
    if (response_buffer[OREPLYWRITELINK_status] != STATUS_NOERROR) {
      (void) sprintf(lnkops_errorstring, " : module [tops.c], function [TOPS_BootWrite]\n -> remote link operation failed");
      return (response_buffer[OREPLYCLOSE_status]);
    }
    
    /* check all data was written */
    UnsignedInt16.as_bytes[0] = response_buffer[OREPLYWRITELINK_byteswritten];
    UnsignedInt16.as_bytes[1] = response_buffer[OREPLYWRITELINK_byteswritten + 1];    
    bytes_written = ntohs (UnsignedInt16.as_short);
    if (bytes_written != fragmented_send) {
      (void) sprintf(lnkops_errorstring," : module [tops.c], function [TOPS_BootWrite]\n -> remote link operation only wrote [%d] bytes\n", bytes_written);
      return (STATUS_LINK_FATAL);
    }
    DebugMessage (fprintf (stderr, "Debug       : received correct writelink reply\n") );
  }
  
  if (errormode == ERRORdetect) {
    result = CheckError ((int) con_id, TSERIES_PROCESSORID);
    if (result != STATUS_NOERROR) {
      (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_BootWrite]\n");
    }
  } else {              
    result = STATUS_NOERROR;
  }
  DebugMessage (fprintf (stderr, "]TOPS_BootWrite\n") );
  return (result);
}


/*
  modifies : SignedInt32
*/
unsigned char TOPS_Reset (con_id, proc_id)
long int con_id;
long int proc_id;
{
  struct OPSResetCommand reset_cmd;
  
  DebugMessage (fprintf (stderr, "[TOPS_Reset\n") );
  
  if (TOPSprotocolstate == Synchronous) {
    /* valid state to issue command */
    reset_cmd.packet_size = htons(OPSResetCommandSize);
    reset_cmd.command_tag = OCMD_Reset; 
    SignedInt32.as_long = htonl((u_long) proc_id);
    reset_cmd.processor_id[0] = SignedInt32.as_bytes[0];
    reset_cmd.processor_id[1] = SignedInt32.as_bytes[1];
    reset_cmd.processor_id[2] = SignedInt32.as_bytes[2];
    reset_cmd.processor_id[3] = SignedInt32.as_bytes[3];
  
    if (DoSocketWrite ((int) con_id, OPSResetCommandSize, (char *) &reset_cmd) != STATUS_NOERROR) {
      (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_Reset]\n");
      return (STATUS_COMMS_FATAL);
    } else {
      DebugMessage (fprintf (stderr, "]TOPS_Reset\n") );
      return (STATUS_NOERROR);
    }
    
  } else {
    (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_Reset]\n -> linkops protocol error, bad protocol mode\n");
    return (STATUS_BAD_COMMS_MODE);
  }
}
  
/*
  modifies : SignedInt32
*/
unsigned char TOPS_Analyse (con_id, proc_id)
long int con_id;
long int proc_id;
{
  struct OPSAnalyseCommand analyse_cmd;
  
  DebugMessage (fprintf (stderr, "[TOPS_Analyse\n") );
  
  if (TOPSprotocolstate == Synchronous) {
    /* valid state to issue command */
    analyse_cmd.packet_size = htons(OPSAnalyseCommandSize);
    analyse_cmd.command_tag = OCMD_Analyse;
    SignedInt32.as_long = htonl((u_long) proc_id);
    analyse_cmd.processor_id[0] = SignedInt32.as_bytes[0];
    analyse_cmd.processor_id[1] = SignedInt32.as_bytes[1];
    analyse_cmd.processor_id[2] = SignedInt32.as_bytes[2];
    analyse_cmd.processor_id[3] = SignedInt32.as_bytes[3];
  
    if (DoSocketWrite ((int) con_id, OPSAnalyseCommandSize, (char *) &analyse_cmd) != STATUS_NOERROR) {
      (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_Analyse]\n");
      return (STATUS_COMMS_FATAL);
    } else {
      DebugMessage (fprintf (stderr, "]TOPS_Analyse\n") );
      return (STATUS_NOERROR);
    }
    
  } else {
    (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_Analyse]\n -> linkops protocol error, bad protocol mode\n");
    return (STATUS_BAD_COMMS_MODE);
  }
}
  
unsigned char TOPS_Poke16 (con_id, proc_id, pokesize, address, buffer)
long int con_id;
long int proc_id;
unsigned long pokesize;
short int address;       
unsigned char *buffer;
{

  struct OPSPoke16Command poke16_cmd;
  short int addr;
  int bytes_to_poke, bytes_poked, frag_poke_bytes;
  unsigned char result;
  
  DebugMessage (fprintf (stderr, "[TOPS_Poke16\n") );
  
  /* check in valid protocol state */
  if (TOPSprotocolstate != Synchronous) {
    (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_Poke16]\n -> linkops protocol error, bad protocol mode\n");
    return (STATUS_BAD_COMMS_MODE);
  }
    
  addr = address;
  bytes_poked = 0;
  bytes_to_poke = (int)(pokesize * 2L);
  
  while (bytes_to_poke > 0) {
    if (bytes_to_poke > FRAG_SIZE) {
      frag_poke_bytes = FRAG_SIZE;
    } else {
      frag_poke_bytes = bytes_to_poke;
    }
    poke16_cmd.packet_size = htons((u_short) (OPSPoke16CommandBasicSize + frag_poke_bytes));
    poke16_cmd.command_tag = OCMD_Poke16;
    SignedInt32.as_long = htonl((u_long) proc_id);
    poke16_cmd.processor_id[0] = SignedInt32.as_bytes[0];
    poke16_cmd.processor_id[1] = SignedInt32.as_bytes[1];
    poke16_cmd.processor_id[2] = SignedInt32.as_bytes[2];
    poke16_cmd.processor_id[3] = SignedInt32.as_bytes[3];
    SignedInt16.as_short = htons((u_short) addr);
    poke16_cmd.address[0] = SignedInt16.as_bytes[0];
    poke16_cmd.address[1] = SignedInt16.as_bytes[1];
    DebugMessage (fprintf (stderr, "Debug       : sending poke16 command (header only), packetsize=[%d]\n",ntohs(poke16_cmd.packet_size)) );
    /* send header fields of command */
    if (DoSocketWrite ((int) con_id, OPSPoke16CommandBasicSize, (char *) &poke16_cmd) != STATUS_NOERROR) {
      (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_Poke16]\n");
      return (STATUS_COMMS_FATAL);
    }    
    DebugMessage (fprintf (stderr, "Debug       : sending poke16 command (data only), packetsize=[%d]\n", frag_poke_bytes) );
    /* send actual data field of command */
    if (DoSocketWrite ((int) con_id, frag_poke_bytes, (char *) &buffer[bytes_poked]) != STATUS_NOERROR) {
      (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_Poke16]\n");
      return (STATUS_COMMS_FATAL);
    }    
    
    /* get reply message */
    DebugMessage (fprintf (stderr, "Debug       : getting poke16 reply\n") );
    result = GetSyncResponse ((int) con_id, OREPLY_Poke16, (unsigned char *) NULL);
    /* check reply received ok */
    if (result != STATUS_NOERROR) {
      (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_Poke16]\n");
      return (result);
    }
    /* alter protocol state */
    TOPSprotocolstate = Synchronous;
    
    /* check status field of reply */
    if (response_buffer[OREPLYPOKE16_status] != STATUS_NOERROR) {
      (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_Poke16]\n -> remote operation failed\n");
      return (response_buffer[OREPLYCLOSE_status]);
    }
    
    DebugMessage (fprintf (stderr, "Debug       : received correct poke16 reply\n") );
    bytes_to_poke = bytes_to_poke - frag_poke_bytes;
    bytes_poked = bytes_poked + frag_poke_bytes;   
    addr = addr + frag_poke_bytes;
    
  }
  DebugMessage (fprintf (stderr, "]TOPS_Poke16\n") );
  return (STATUS_NOERROR);
}

unsigned char TOPS_Poke32 (con_id, proc_id, pokesize, address, buffer)
long int con_id;
long int proc_id;
unsigned long pokesize;
long int address;       
unsigned char *buffer;
{

  struct OPSPoke32Command poke32_cmd;
  long int addr;
  int bytes_to_poke, bytes_poked, frag_poke_bytes;
  unsigned char result;
  
  DebugMessage (fprintf (stderr, "[TOPS_Poke32\n") );
  
  /* check in valid protocol state */
  if (TOPSprotocolstate != Synchronous) {
    (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_Poke32]\n -> linkops protocol error, bad protocol mode\n");
    return (STATUS_BAD_COMMS_MODE);
  }
    
  addr = address;
  bytes_poked = 0;
  bytes_to_poke = (int)(pokesize * 4L);
  
  while (bytes_to_poke > 0) {
    if (bytes_to_poke > FRAG_SIZE) {
      frag_poke_bytes = FRAG_SIZE;
    } else {
      frag_poke_bytes = bytes_to_poke;
    }
    poke32_cmd.packet_size = htons((u_short) (OPSPoke32CommandBasicSize + frag_poke_bytes));
    poke32_cmd.command_tag = OCMD_Poke32;
    SignedInt32.as_long = htonl((u_long) proc_id);
    poke32_cmd.processor_id[0] = SignedInt32.as_bytes[0];
    poke32_cmd.processor_id[1] = SignedInt32.as_bytes[1];
    poke32_cmd.processor_id[2] = SignedInt32.as_bytes[2];
    poke32_cmd.processor_id[3] = SignedInt32.as_bytes[3];
    SignedInt32.as_long = htonl((u_long) addr);
    poke32_cmd.address[0] = SignedInt32.as_bytes[0];
    poke32_cmd.address[1] = SignedInt32.as_bytes[1];
    poke32_cmd.address[2] = SignedInt32.as_bytes[2];
    poke32_cmd.address[3] = SignedInt32.as_bytes[3];
    DebugMessage (fprintf (stderr, "Debug       : sending poke32 command (header only), packetsize=[%d]\n", ntohs(poke32_cmd.packet_size)) );
    /* send header fields of command */
    if (DoSocketWrite ((int) con_id, OPSPoke32CommandBasicSize, (char *) &poke32_cmd) != STATUS_NOERROR) {
      (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_Poke32]\n");
      return (STATUS_COMMS_FATAL);
    }    
    DebugMessage (fprintf (stderr, "Debug       : sending poke32 command (data only), packetsize=[%d]\n", frag_poke_bytes) );
    /* send actual data field of command */
    if (DoSocketWrite ((int) con_id, frag_poke_bytes, (char *) &buffer[bytes_poked]) != STATUS_NOERROR) {
      (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_Poke32]\n");
      return (STATUS_COMMS_FATAL);
    }    
    
    /* get reply message */
    DebugMessage (fprintf (stderr, "Debug       : getting poke32 reply\n") );
    result = GetSyncResponse ((int) con_id, OREPLY_Poke32, (unsigned char *) NULL);
    /* check reply received ok */
    if (result != STATUS_NOERROR) {
      (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_Poke32]\n");
      return (result);
    }
    /* alter protocol state */
    TOPSprotocolstate = Synchronous;
    
    /* check status field of reply */
    if (response_buffer[OREPLYPOKE32_status] != STATUS_NOERROR) {
      (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_Poke32]\n -> remote operation failed\n");
      return (response_buffer[OREPLYCLOSE_status]);
    }
    
    DebugMessage (fprintf (stderr, "Debug       : received correct poke32 reply\n") );
    bytes_to_poke = bytes_to_poke - frag_poke_bytes;
    bytes_poked = bytes_poked + frag_poke_bytes;   
    addr = addr + frag_poke_bytes;
    
  }
  DebugMessage (fprintf (stderr, "]TOPS_Poke32\n") );
  return (STATUS_NOERROR);
}


unsigned char TOPS_Peek16 (con_id, proc_id, peeksize, address, buffer)
long int con_id;
long int proc_id;
unsigned long peeksize;
short int address;
unsigned char *buffer;
{
  struct OPSPeek16Command peek16_cmd;
  short int addr;
  int bytes_to_peek, bytes_peeked, frag_peek_bytes, frag_peek_words;
  unsigned char result;
  
  DebugMessage (fprintf (stderr, "[TOPS_Peek16\n") );
  
  /* check in valid protocol state */
  if (TOPSprotocolstate != Synchronous) {
    (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_Peek16]\n -> linkops protocol error, bad protocol mode\n");
    return (STATUS_BAD_COMMS_MODE);
  }
    
  bytes_peeked = 0;
  bytes_to_peek = (int)(peeksize * 2L);
  addr = address;
  
  while (bytes_to_peek > 0) {
    if (bytes_to_peek > FRAG_SIZE) {
      frag_peek_bytes = FRAG_SIZE;
    } else {
      frag_peek_bytes = bytes_to_peek;
    }
    frag_peek_words = (frag_peek_bytes / 2);
                                                                          
    peek16_cmd.packet_size = htons(OPSPeek16CommandSize);
    peek16_cmd.command_tag = OCMD_Peek16;
    SignedInt32.as_long = htonl((u_long) proc_id);
    peek16_cmd.processor_id[0] = SignedInt32.as_bytes[0];
    peek16_cmd.processor_id[1] = SignedInt32.as_bytes[1];
    peek16_cmd.processor_id[2] = SignedInt32.as_bytes[2];
    peek16_cmd.processor_id[3] = SignedInt32.as_bytes[3];
    UnsignedInt16.as_short = htons((u_short) frag_peek_words);
    peek16_cmd.peek_length[0] = UnsignedInt16.as_bytes[0];
    peek16_cmd.peek_length[1] = UnsignedInt16.as_bytes[1];
    SignedInt16.as_short = htons((u_short) addr);
    peek16_cmd.address[0] = SignedInt16.as_bytes[0];
    peek16_cmd.address[1] = SignedInt16.as_bytes[1];
    
    /* send header fields of command */
    if (DoSocketWrite ((int) con_id, OPSPeek16CommandSize, (char *) &peek16_cmd) != STATUS_NOERROR) {
      (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_Peek16]\n");
      return (STATUS_COMMS_FATAL);
    }    
    
    /* alter protocol state */
    TOPSprotocolstate = SynchronousReply;
    
    /* get reply message */
    DebugMessage (fprintf (stderr, "Debug       : getting peek16 reply\n") );
    
    result = GetSyncResponse ((int) con_id, OREPLY_Peek16, (unsigned char *)&buffer[bytes_peeked]);
    
    /* check reply received ok */
    if (result != STATUS_NOERROR) {
      (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_Peek16]\n");
      return (result);
    }
    /* alter protocol state */
    TOPSprotocolstate = Synchronous;
    
    bytes_to_peek = bytes_to_peek - frag_peek_bytes;
    bytes_peeked = bytes_peeked + frag_peek_bytes;
    addr = addr + frag_peek_bytes; 
    
    /* check status field of reply */
    if (response_buffer[OREPLYPEEK32_status] != STATUS_NOERROR) {
      (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_Peek16]\n -> remote operation failed\n");
      return (response_buffer[OREPLYCLOSE_status]);
    }
    
    /* check processor id field  */
    UnsignedInt32.as_bytes[0] = response_buffer[OREPLYPEEK32_processorid];
    UnsignedInt32.as_bytes[1] = response_buffer[OREPLYPEEK32_processorid + 1];
    UnsignedInt32.as_bytes[2] = response_buffer[OREPLYPEEK32_processorid + 2];
    UnsignedInt32.as_bytes[3] = response_buffer[OREPLYPEEK32_processorid + 3];
    if (UnsignedInt32.as_long != TSERIES_PROCESSORID) {
      (void) sprintf (lnkops_errorstring, " : module [tops.c], function [Peek16]\n -> bad processor_id (T-series implementation)\n");
      return (STATUS_BAD_OPERATION);
    }
    
    DebugMessage (fprintf (stderr, "Debug       : received correct peek16 reply\n") );
  }
  DebugMessage (fprintf (stderr, "]TOPS_Peek16\n") );
  return (STATUS_NOERROR);
}

unsigned char TOPS_Peek32 (con_id, proc_id, peeksize, address, buffer)
long int con_id;
long int proc_id;
unsigned long peeksize;
long int address;
unsigned char *buffer;
{
  struct OPSPeek32Command peek32_cmd;
  long int addr;
  int bytes_to_peek, bytes_peeked, frag_peek_bytes, frag_peek_words;
  unsigned char result;
  
  DebugMessage (fprintf (stderr, "[TOPS_Peek32\n") );
  
  /* check in valid protocol state */
  if (TOPSprotocolstate != Synchronous) {
    (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_Peek32]\n -> linkops protocol error, bad protocol mode\n");
    return (STATUS_BAD_COMMS_MODE);
  }
    
  bytes_peeked = 0;
  bytes_to_peek = (int)(peeksize * 4L);
  addr = address;
  
  while (bytes_to_peek > 0) {
    if (bytes_to_peek > FRAG_SIZE) {
      frag_peek_bytes = FRAG_SIZE;
    } else {
      frag_peek_bytes = bytes_to_peek;
    }
    frag_peek_words = (frag_peek_bytes / 4);
                                                                          
    peek32_cmd.packet_size = htons(OPSPeek32CommandSize);
    peek32_cmd.command_tag = OCMD_Peek32;
    SignedInt32.as_long = htonl((u_long) proc_id);
    peek32_cmd.processor_id[0] = SignedInt32.as_bytes[0];
    peek32_cmd.processor_id[1] = SignedInt32.as_bytes[1];
    peek32_cmd.processor_id[2] = SignedInt32.as_bytes[2];
    peek32_cmd.processor_id[3] = SignedInt32.as_bytes[3];
    UnsignedInt16.as_short = htons((u_short) frag_peek_words);
    peek32_cmd.peek_length[0] = UnsignedInt16.as_bytes[0];
    peek32_cmd.peek_length[1] = UnsignedInt16.as_bytes[1];
    SignedInt32.as_long = htonl((u_long) addr);
    peek32_cmd.address[0] = SignedInt32.as_bytes[0];
    peek32_cmd.address[1] = SignedInt32.as_bytes[1];
    peek32_cmd.address[2] = SignedInt32.as_bytes[2];
    peek32_cmd.address[3] = SignedInt32.as_bytes[3];
    
    /* send header fields of command */
    if (DoSocketWrite ((int) con_id, OPSPeek32CommandSize, (char *) &peek32_cmd) != STATUS_NOERROR) {
      (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_Peek32]\n");
      return (STATUS_COMMS_FATAL);
    }    
    
    /* alter protocol state */
    TOPSprotocolstate = SynchronousReply;
    
    /* get reply message */
    DebugMessage (fprintf (stderr, "Debug       : getting peek32 reply\n") );
    
    result = GetSyncResponse ((int) con_id, OREPLY_Peek32, (unsigned char *)&buffer[bytes_peeked]);
    
    /* check reply received ok */
    if (result != STATUS_NOERROR) {
      (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_Peek32]\n");
      return (result);
    }
    /* alter protocol state */
    TOPSprotocolstate = Synchronous;
    
    bytes_to_peek = bytes_to_peek - frag_peek_bytes;
    bytes_peeked = bytes_peeked + frag_peek_bytes;
    addr = addr + frag_peek_bytes; 
    
    /* check status field of reply */
    if (response_buffer[OREPLYPEEK32_status] != STATUS_NOERROR) {
      (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_Peek32]\n -> remote operation failed\n");
      return (response_buffer[OREPLYCLOSE_status]);
    }
    
    /* check processor id field  */
    UnsignedInt32.as_bytes[0] = response_buffer[OREPLYPEEK32_processorid];
    UnsignedInt32.as_bytes[1] = response_buffer[OREPLYPEEK32_processorid + 1];
    UnsignedInt32.as_bytes[2] = response_buffer[OREPLYPEEK32_processorid + 2];
    UnsignedInt32.as_bytes[3] = response_buffer[OREPLYPEEK32_processorid + 3];
    if (UnsignedInt32.as_long != TSERIES_PROCESSORID) {
      (void) sprintf (lnkops_errorstring, " : module [tops.c], function [Peek32]\n -> bad processor_id (T-series implementation)\n");
      return (STATUS_BAD_OPERATION);
    }
    
    DebugMessage (fprintf (stderr, "Debug       : received correct peek32 reply\n") );
  }
  DebugMessage (fprintf (stderr, "]TOPS_Peek32\n") );
  return (STATUS_NOERROR);
}

unsigned char TOPS_GetRequest (con_id, buffer, timeout, heartbeat_fn, errormode)
long int con_id;
unsigned char *buffer;
long int timeout;
unsigned char (*heartbeat_fn) ();
enum ErrorModes errormode;
{
  long int wait_time;
  int result;
  bool got_request, do_heartbeat;
  
  DebugMessage (fprintf (stderr, "[TOPS_GetRequest\n") );
  
  /* check in valid protocol state */
  if (TOPSprotocolstate != Asynchronous) {
    (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_GetRequest]\n -> linkops protocol error, bad protocol mode\n");
    return (STATUS_BAD_COMMS_MODE);
  }
  
  if (heartbeat_fn == NULL) {
    DebugMessage (fprintf (stderr, "Debug       : no heartbeat function\n") );
    do_heartbeat = false;
    wait_time = 0L;
  } else {
    do_heartbeat = true;
    wait_time = timeout;
  }
  
  if (TOPSnum_spbuffered > 0) {
    DebugMessage (fprintf (stderr, "Debug       : have [%d] requests buffered, copy one\n", TOPSnum_spbuffered) ); 
    result = UnQueueSPBuffer ((unsigned char *)&buffer[0]);
    if (result != STATUS_NOERROR) {
      (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_GetRequest](A)\n");
      return (result);
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
          (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_GetRequest]\n -> heartbeat function returned error\n");
          return (STATUS_BAD_OPERATION);
          break;
          
        default:
          (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_GetRequest]\n -> heartbeat function returned unexpected value\n");
          return (STATUS_BAD_OPERATION);
          break;
      }
    }
    return (STATUS_NOERROR);
    
  } else {
    DebugMessage (fprintf (stderr, "Debug       : no buffered request, wait for one\n") );
    got_request = false;
    while (got_request == false) {
      /* read getrequest event */
      DebugMessage (fprintf (stderr, "Debug       : getting request event, wait_time=[%d]\n", wait_time) );
      result = GetAsyncRequest ((int) con_id, (unsigned char *)&buffer[0], wait_time, 0);
      switch (result) {
        case STATUS_TIMEDOUT:
          DebugMessage (fprintf (stderr, "Debug       : didn't get request event (timedout)\n") );
          /* no data ready on socket so call heartbeat fn*/
          if (do_heartbeat == true) {
            DebugMessage (fprintf (stderr, "Debug       : calling heartbeat function\n") );
            switch ((*heartbeat_fn) ()) {
              case DO_HEARTBEAT:          
                /* no key read */
                DebugMessage (fprintf (stderr, "Debug       : keep heartbeat going\n") );
                if (TOPSnum_spbuffered > 0) {
                  DebugMessage (fprintf (stderr, "Debug       : after buffered requests = [%d], copy one\n", TOPSnum_spbuffered) ); 
                  result = UnQueueSPBuffer ((unsigned char *)&buffer[0]);
                  if (result != STATUS_NOERROR) {
                    (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_GetRequest](B)\n");
                    return (result);
                  }               
                  got_request = true;
                }
                break;
                
              case KILL_HEARTBEAT:
                do_heartbeat = false;
                if (TOPSnum_spbuffered > 0) {
                  DebugMessage (fprintf (stderr, "Debug       : after buffered requests = [%d], copy one\n", TOPSnum_spbuffered) ); 
                  result = UnQueueSPBuffer ((unsigned char *)&buffer[0]);
                  if (result != STATUS_NOERROR) {
                    (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_GetRequest](C)\n");
                    return (result);
                  }               
                  got_request = true;
                }
                wait_time = 0L;
                DebugMessage (fprintf (stderr, "Debug       : disable heartbeat\n") );
                break;
                
              case FATAL_ERROR:
                (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_GetRequest]\n -> heartbeat function returned error\n");
                return (STATUS_BAD_OPERATION);
                break;
                
              default:
                (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_GetRequest]\n -> heartbeat function returned unexpected value\n");
                return (STATUS_BAD_OPERATION);
                break;
            }
          }
          break;
          
        case STATUS_NOERROR:
          /* got sp request ok */
          got_request = true;
          DebugMessage (fprintf (stderr, "Debug       : got request event\n") );
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
                (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_GetRequest]\n -> heartbeat function returned error\n");
                return (STATUS_BAD_OPERATION);
                break;
                
              default:
                (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_GetRequest]\n -> heartbeat function returned unexpected value\n");
                return (STATUS_BAD_OPERATION);
                break;
            }
          }
          break;
          
        case STATUS_TARGET_ERROR:
          return (result);
          
        default:
          /* error conditions */
          (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_GetRequest]\n");
          return (result);
          break;
      }
    }
    
    DebugMessage (fprintf (stderr, "]TOPS_GetRequest\n") );
    return (result);
  }
  
}

unsigned char TOPS_SendReply (con_id, buffer, errormode)
long int con_id;
unsigned char *buffer;
enum ErrorModes errormode;
{

  struct OPSAsyncWriteCommand asyncwrite_cmd;
  int size;
  unsigned char result;
  
  DebugMessage (fprintf (stderr, "[TOPS_SendReply\n") );
  
  /* check in valid protocol state */
  if (TOPSprotocolstate != Asynchronous) {
    (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_SendReply]\n -> linkops protocol error, bad protocol mode\n");
    return (STATUS_BAD_COMMS_MODE);
  }
    
  size = buffer[0] + (buffer[1] * 256);
  if ((size <= 0) || (size > SPMAXPACKET)) {
    (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_SendReply]\n -> sp header illegal, size = %d bytes\n", size);
    return (STATUS_SP_ERROR);
  }
  
  size = size + 2; /* include header */
  
  asyncwrite_cmd.packet_size = htons((u_short) (OPSAsyncWriteCommandBasicSize + size));
  asyncwrite_cmd.command_tag = OCMD_AsyncWrite;                 
  DebugMessage (fprintf (stderr, "Debug       : sending asyncwrite command (header only), packetsize=[%d]\n", ntohs(asyncwrite_cmd.packet_size)) );
  /* send header fields of command */
  if (DoSocketWrite ((int) con_id, OPSAsyncWriteCommandBasicSize, (char *) &asyncwrite_cmd) != STATUS_NOERROR) {
    (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_SendReply]\n");
    return (STATUS_COMMS_FATAL);
  }    
  DebugMessage (fprintf (stderr, "Debug       : sending asyncwrite command (data only), packetsize=[%d]\n", size) );
  /* send actual data field of command */
  if (DoSocketWrite ((int) con_id, size, (char *) &buffer[0]) != STATUS_NOERROR) {
    (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_SendReply]\n");
    return (STATUS_COMMS_FATAL);
  }    
    
  /* while response available, handle them until an error occurs
     or no more responses can be received */
  while ( ((result = TestResponseAvailable ((int) con_id) ) == STATUS_NOERROR) && (TOPSnum_spbuffered < TOPSbuffer_depth) ) {
  
    result = GetAsyncRequest ((int) con_id, (unsigned char *)NULL, 0L, 1);
    switch (result) {
      case STATUS_NOERROR:
      case STATUS_TIMEDOUT:
        break;
        
      case STATUS_TARGET_ERROR:
        DebugMessage (fprintf (stderr, "]TOPS_SendReply\n") );
        return (result);
        break;
        
      default:
        (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_SendReply]\n");
        DebugMessage (fprintf (stderr, "]TOPS_SendReply\n") );
        return (result);
        break;
        
    }
  }
  DebugMessage (fprintf (stderr, "]TOPS_SendReply\n") );
  return (STATUS_NOERROR);
}

unsigned char TOPS_Restart (con_id, commsmode, errormode)
long int con_id;
enum CommsModes *commsmode;
enum ErrorModes *errormode;
{

  struct OPSRestartCommand restart_cmd;
  unsigned char result;
  
  DebugMessage (fprintf (stderr, "[TOPS_Restart\n") );
  
  switch (TOPSprotocolstate) {
    case Synchronous:
    case SynchronousReply:
    case Asynchronous:
    case CancelAsynchronous:
      break;
    
    default:
      (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_Restart]\n -> linkops protocol error, bad protocol mode\n");
      return (STATUS_BAD_COMMS_MODE);
      break;
  };
  
  TOPSprotocolstate = Restarting;
  restart_cmd.packet_size = htons((u_short) OPSRestartCommandSize);
  restart_cmd.command_tag = OCMD_Restart;
  DebugMessage (fprintf (stderr, "Debug       : sending restart command, packetsize=[%d]\n", ntohs(restart_cmd.packet_size)) );
                                                                                              
  if (DoSocketWrite ((int) con_id, OPSRestartCommandSize, (char *) &restart_cmd) != STATUS_NOERROR) {
    (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_Restart]\n");
    return (STATUS_COMMS_FATAL);
  }    
  
  /* get reply */
  DebugMessage (fprintf (stderr,"Debug       : getting restart reply\n") );
  result = WaitForReply ((int) con_id, OREPLY_Restart);
  
  /* if reply received ok, return status field of reply */
  if (result == STATUS_NOERROR) {
    result = response_buffer[OREPLYRESTART_status];
    switch (result) {
      case STATUS_NOERROR:
        /* alter protocol state */
        TOPSprotocolstate = Synchronous;
        TOPSerrordetect = false;
        result = CleanUpBufferQueue ();
        *commsmode = COMMSsynchronous;
        *errormode = ERRORignore;
        if (result != STATUS_NOERROR) {
          (void) sprintf(&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_Close]");
          return (result);
        } else {
          DebugMessage (fprintf (stderr, "]TOPS_Restart\n") );
          return (STATUS_NOERROR);
        }
        break;
        
      case STATUS_BAD_OPERATION:
        (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_Restart]\n -> remote operation failed\n");
        break;
      
      case STATUS_LINK_FATAL:
        (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_Restart]\n -> remote link operation failed\n");
        break;
      
      default:
        (void) sprintf (lnkops_errorstring, " : module [tops.c], function [TOPS_Restart]\n -> linkops protocol error, got unexpected status = [%d]\n", result);
        break;
    }
  } else {
    (void) sprintf (&lnkops_errorstring[strlen(lnkops_errorstring)], "called by [TOPS_Restart]\n");
  }

  DebugMessage (fprintf (stderr, "]TOPS_Restart\n") );
  return (result);
  
}



#endif /* NOCOMMS */


