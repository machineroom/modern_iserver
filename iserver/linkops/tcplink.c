/*------------------------------------------------------------------------------
--
-- File    : tcplink.c
--    linkio module via linkops protocol over TCP/IP
--     name specification given to OpenLink = <resource>@<server>
--     where <server> is either an internet address or host name
--
-- Copyright 1990, 1991 INMOS Limited
--
-- Date    : 6/26/91
-- Version : 1.4
--
-- 30/06/90 - dave edwards
--  originated
-- 04/12/90 - dave edwards
--  updated debug/info bits
-- 07/12/90 - dave edwards
--  'pc-nfs'ified
-- 19/03/91 - dave edwards 
--  added sccs id stuff
-- 26/06/91 - dave edwards
--  added DEC/TCP support
--
------------------------------------------------------------------------------*/

static char SccsId[] = "@(#) Module: tcplink.c, revision 1.4 of 6/26/91";

#ifndef NOCOMMS

#include <stdio.h>
#include <errno.h>

#ifdef DECTCP
#include <types.h>
#include <socket.h>
#include <time.h>
#include <in.h>
#include <netdb.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#ifndef WINTCP
#include <memory.h>
#endif /* WINTCP */
#endif /* DECTCP */

#ifdef WINTCP
#include <sys/errno.h>
#endif

#include "types.h"
#include "debug.h"
#include "opsprot.h"
#include "opserror.h"

#include "linkio.h"

#ifdef VMS
#include <string.h>
#else
#ifdef MSDOS
#include <string.h>
#else
#ifdef SOLARIS
#include <string.h>
#else
#include <strings.h>
#endif
#endif
#endif

#ifdef PCNFS
#include "pcntypes.h"
extern int errno;
#endif

#define ONESECOND 1000000L
#define TSERIES_PROCESSORID 0L

/* timeout when trying to close or restart */
#define WAIT_TIMEOUT (ONESECOND * 4L)
/* timeout when trying to read service string */
#define READOPEN1_TIMEOUT (ONESECOND * 20L)
#define READOPEN2_TIMEOUT (ONESECOND * 20L)

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

static BOOL LinkIsOpen = FALSE;

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


/*** 
 ** internal functions
***/

/* DoSocketWrite -
  effects : attempts to write N bytes from the buffer to the socket.
            if successfull, returns SUCCEEDED
            if an error occurs, returns ER_LINK_SOFT
*/
static int DoSocketWrite (socket, to_write, buffer)
int socket;
int to_write;
char *buffer;
{
  int data_written;
  int left_to_write = to_write;
  
  while (left_to_write > 0) {  
    data_written = send (socket, buffer, to_write, 0);
    if (data_written == 0) {
      ErrorMessage (fprintf (stderr, " : connection to server lost\n") );
      return (ER_LINK_CANT);
    }                          
    if (data_written < 0) {
      switch (errno) {
        case ENOBUFS: /* not enough buffers, try again */
          DebugMessage (fprintf (stderr, "Warning     : couldn't send, trying again\n") );
          break;
        
        default:
          ErrorMessage (fprintf (stderr, " : module [tcplink.c], function [DoSocketWrite]\n -> send failed, errno=[%d]\n", errno) );
          return (ER_LINK_SOFT);
          break;
      }
    }
    left_to_write = left_to_write - data_written;
    buffer = buffer + data_written;
  }
  
  return (SUCCEEDED);
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
static enum TimedStatus DoTimedSocketRead (socket, to_read, timeout, buffer)
int socket;
int to_read;
long int timeout;
unsigned char *buffer;
{
  int fdwidth, data_read, result;
  struct timeval delay;

#ifdef DECTCP
  int read_fds;
#else /* DECTCP */
#ifdef PCNFS
  long int read_fds;
#else
  fd_set read_fds;
#endif /* PCNFS */
#endif /* DECTCP */
  
  if (timeout >= ONESECOND) {    
    delay.tv_sec = (int)(timeout / ONESECOND);
    delay.tv_usec = (int)(timeout % ONESECOND);
  } else {
    delay.tv_sec = 0;        /* seconds */
    delay.tv_usec = (int)timeout; /* uS */
  }
  
#ifdef DECTCP
  read_fds = (1 << socket);
#else
  FD_ZERO (&read_fds);
  FD_SET (socket, &read_fds);
#endif

  fdwidth = 32;
#ifdef PCNFS
  if (timeout == 0) {
    /* infinite timeout */
    result = select (fdwidth, (long int *) &read_fds, (long int *) NULL, (long int *) NULL, (struct timeval *)NULL);
  } else {
    result = select (fdwidth, (long int *) &read_fds, (long int *) NULL, (long int *) NULL, &delay);
  }
#else
#ifdef DECTCP
  if (timeout == 0) {
    /* infinite timeout */
    result = select (fdwidth, (int *)&read_fds, (int *) NULL, (int *) NULL, (struct timeval *)NULL);
  } else {
    result = select (fdwidth, &read_fds, (int *) NULL, (int *) NULL, &delay);
  }
#else
  if (timeout == 0) {
    /* infinite timeout */
    result = select (fdwidth, &read_fds, (fd_set *) NULL, (fd_set *) NULL, (struct timeval *)NULL);
  } else {
    result = select (fdwidth, &read_fds, (fd_set *) NULL, (fd_set *) NULL, &delay);
  }
#endif /* DECTCP */
#endif /* PCNFS */
  
  
  if (result < 0) {
    ErrorMessage (fprintf (stderr, " : module [tcplink.c], function [DoTimedSocketRead]\n -> select failed, errno=[%d]\n", errno) );
    return (TIMEDSTATUSfailure);
  }
  
  if (result == 0) {
    return (TIMEDSTATUSnodata);
  } else {
    data_read = recv (socket, (char *)buffer, to_read, 0);
    if (data_read < 0) {                                                                                    
      ErrorMessage (fprintf (stderr, " : module [tcplink.c], function [DoTimedSocketRead]\n -> recv failed, errno=[%d]\n", errno) );
      return (TIMEDSTATUSfailure);                                                                                  
    }
    if (data_read == 0) {
      ErrorMessage (fprintf (stderr, " : connection to server lost\n") );
      return (TIMEDSTATUSfailure);
    } else {
      if (data_read != to_read) {
        ErrorMessage (fprintf (stderr, "#####only read (%d) bytes, asked for [%d]\n", data_read, to_read) );
        return (TIMEDSTATUSfailure);
      }
      return (TIMEDSTATUSdata);
    }
  }
}

/* DoSocketRead -
  effects   : attempts to read N bytes from the socket into buffer
              if succesfull returns SUCCEEDED
              if an error occurs returns ER_LINK_SOFT or ER_LINK_CANT
*/
static unsigned char DoSocketRead (socket, to_read, buffer)
int socket;
int to_read;
unsigned char *buffer;
{
  int left_to_read = to_read;
  int data_read;
  
  while (left_to_read > 0) {
    data_read = recv (socket, (char *)buffer, left_to_read, 0);
    if (data_read < 0) {
      ErrorMessage (fprintf (stderr, " : module [tcplink.c], function [DoSocketRead]\n -> recv failed, errno=[%d]\n", errno) );
      return (ER_LINK_SOFT);
    }
    if (data_read == 0) {
      ErrorMessage (fprintf (stderr, " : connection to server lost\n") );
      return (ER_LINK_CANT);
    }
    left_to_read = left_to_read - data_read;
    buffer = buffer + data_read;
  }
  return (SUCCEEDED);
}

/* HandleMessage ()
     requires :  OPSMIN_RESPONSEMESSAGESIZE bytes of message read into
                response_buffer
     modifies : response_buffer, TOPSprotocolstate
     effects  : handles message event :
                 outputs text string,
                 if message was fatal or a fatal error occurs,
                  return ER_LINK_SOFT or ER_LINK_CANT
                 else
                  return SUCCEDED
*/
static unsigned char HandleMessage (socket, packet_size)
int socket;
int packet_size;
{
  int extra_to_read;
  int result;
  
   /* read rest of message response */
   extra_to_read = (packet_size - OPSMIN_RESPONSEMESSAGESIZE);
   result = DoSocketRead (socket, extra_to_read, (unsigned char *)&response_buffer[OPSMIN_RESPONSEMESSAGESIZE]);
   if (result != STATUS_NOERROR) {
     ErrorMessage (fprintf (stderr, "called by [HandleMessage]\n") );
     return (result);
   }      
   if (response_buffer[OEVENTMESSAGE_fatal] == ((unsigned char) 0)) {
     ErrorMessage (fprintf (stderr, "\n>>>>Message from linkops server<<<<\n[%s]\n", (char *)&response_buffer[OEVENTMESSAGE_string]) );
     ErrorMessage (fflush (stderr) );      
     return (SUCCEEDED);
   } else {
     ErrorMessage (fprintf (stderr, "\n>>>>Fatal message from linkops server<<<<\n[%s]\n", (char *)&response_buffer[OEVENTMESSAGE_string]) );
     ErrorMessage ( fflush (stderr) );
     /* alter protocol state */
     TOPSprotocolstate = Closed;
     return (ER_LINK_SOFT);
   }    
}

/* GetSyncResponse
     requires : sizeof(buffer) >= OPSMAXPACKET
     modifies : buffer
     effects : tries to read the synchronous-mode response message
               whoose tag field = 'wanted_tag'.
               the message will be read into 'response_buffer'.
               the data fields of OREPLY_TCPReadLink messages are
               messages is read into 'buffer'.
               handles message events. if successful, returns SUCCEEDED
               else returns ER_LINK_SOFT or ER_LINK_CANT
*/
static unsigned char GetSyncResponse (socket, wanted_tag, buffer)
int socket;
unsigned char wanted_tag;
unsigned char *buffer;
{
  unsigned char response_tag;
  int result, extra_to_read, bytes_read, packet_size;
  BOOL got_wanted_tag = FALSE;
  
  while (got_wanted_tag == FALSE) {
  
    /* read header of linkops response message */
    result = DoSocketRead (socket, OPSMIN_RESPONSEMESSAGESIZE, (unsigned char *)&response_buffer[0]);
    if (result != STATUS_NOERROR) {
      ErrorMessage (fprintf (stderr, "called by [GetSyncResponse]\n") );
      return (result);
    }
    bytes_read = OPSMIN_RESPONSEMESSAGESIZE;
    
    /* deduce packet size, tag & how much more data needs to be read */
    UnsignedInt16.as_bytes[0] = response_buffer[OPSPACKETSIZEFIELD];
    UnsignedInt16.as_bytes[1] = response_buffer[OPSPACKETSIZEFIELD + 1];
    packet_size = ntohs (UnsignedInt16.as_short);
    if (packet_size > OPSMAXPACKET) {
      ErrorMessage (fprintf (stderr, " : module [tcplink.c], function [GetSyncResponse]\n -> packet size too large = [%d]\n", packet_size) );
      return (ER_LINK_SOFT);
    }
    
    response_tag = response_buffer[OPSTAGFIELD];
    got_wanted_tag = (wanted_tag == response_tag);
    
    DebugMessage (fprintf (stderr, "Debug       : got tag        [%d]\n", response_tag) );
    DebugMessage (fprintf (stderr, "Debug       : got packetsize [%d]\n", packet_size) );  
    
    if (got_wanted_tag == FALSE) {
      /* only synchronous mode response capable of handling (apart from
         the one we are waiting & OEVENT_MESSAGE is an error */
      if (response_tag == OEVENT_Message) {
        result = HandleMessage (socket, packet_size);
        if (result != STATUS_NOERROR) {
          ErrorMessage ( fprintf (stderr, "called by [GetSyncResponse]\n") );
          return (result);
        }
      } else {
        ErrorMessage (fprintf (stderr, " : module [tcplink.c], function [GetSyncResponse]\n -> didnt get wanted tag - panic!\n") );
        return (ER_LINK_SOFT);
      }
      
    } else {
      /* got desired message, check tags & read rest of message */
      switch (response_tag) {
        /* responses with more data to read */
        case OREPLY_TestError:
        case OREPLY_WriteLink:
          extra_to_read = (packet_size - bytes_read);
          result = DoSocketRead (socket, extra_to_read, (unsigned char *)&response_buffer[bytes_read]);
          if (result != STATUS_NOERROR) {
           ErrorMessage (fprintf (stderr, "called by [GetSyncResponse]\n") );
          }
          return (result);
          break;
        
        /* responses with special data, read normal part first */
        case OREPLY_ReadLink:
          extra_to_read = (OPSReadLinkReplyBasicSize - bytes_read); /* enough to complete status field */
          break;
          
        default:
          ErrorMessage (fprintf (stderr, " : module [tcplink.c], function [GetSyncResponse]\n -> got unexpected tag value\n") );
          return (STATUS_COMMS_FATAL);
          break;
      }
      
      /* read extra normal data */
      result = DoSocketRead (socket, extra_to_read, (unsigned char *)&response_buffer[bytes_read]);
      if (result != STATUS_NOERROR) {
        ErrorMessage (fprintf (stderr, "called by [GetSyncResponse](A)\n") );
        return (result);
      }
      bytes_read = bytes_read + extra_to_read;
      
      /* calculate if any more to read, if so this is a special read into buffer */
      extra_to_read = (packet_size - bytes_read);
      result = DoSocketRead (socket, extra_to_read, (unsigned char *)&buffer[0]);
      if (result != STATUS_NOERROR) {
        ErrorMessage (fprintf (stderr, "called by [GetSyncResponse](B)\n") );
      }
      return (result);
    }
  }
  return (SUCCEEDED);
}

/* WaitForReply ()
    effects : waits for a particular reply, if nothing read within WAIT_TIMEOUT returns
              ER_LINK_SOFT. throws away everything except tag waiting for - however
              if a message event arrives it will be printed.
*/
static int WaitForReply (socket, wanted_tag)
int socket;
unsigned char wanted_tag;
{
  enum TimedStatus timed_result;
  unsigned char response_tag;
  int result, extra_to_read, bytes_read, packet_size;
  BOOL got_reply = FALSE;
  
  while (got_reply == FALSE) {
    /* read header of linkops response message */
    timed_result = DoTimedSocketRead (socket, OPSMIN_RESPONSEMESSAGESIZE, WAIT_TIMEOUT, (unsigned char *)&response_buffer[0]);
    
    if (timed_result != TIMEDSTATUSdata) {
      if (timed_result == TIMEDSTATUSnodata) {
        ErrorMessage (fprintf (stderr, " : no response from server (timed out)\n") );
      } else {
        ErrorMessage (fprintf (stderr, "called by [WaitForReply]\n") );
      }
      return (ER_LINK_CANT);
    }
    
    bytes_read = OPSMIN_RESPONSEMESSAGESIZE;
    /* deduce packet size, tag & how much more data needs to be read */
    UnsignedInt16.as_bytes[0] = response_buffer[OPSPACKETSIZEFIELD];
    UnsignedInt16.as_bytes[1] = response_buffer[OPSPACKETSIZEFIELD + 1];
    
    packet_size = ntohs (UnsignedInt16.as_short);
    if (packet_size > OPSMAXPACKET) {
      ErrorMessage (fprintf (stderr, " : module [tops.c], function [WaitForReply]\n -> packet size too large = [%d]\n", packet_size) );
      return (ER_LINK_SOFT);
    }
    response_tag = response_buffer[OPSTAGFIELD];
    DebugMessage (fprintf (stderr, "Debug       : tag        [%d]\n", response_tag) );
    DebugMessage (fprintf (stderr, "Debug       : packetsize [%d]\n", packet_size) );
    
    switch (response_tag) {
      case OEVENT_Message:
        result = HandleMessage (socket, packet_size);
        if (result != SUCCEEDED) {
          ErrorMessage (fprintf (stderr, "called by [WaitForReply]\n") );
          return (result);
        }
        break;
        
      default:
        /* read rest of message */
        extra_to_read = (packet_size - bytes_read);
        result = DoSocketRead (socket, extra_to_read, (unsigned char *)&response_buffer[bytes_read]);
        if (result != SUCCEEDED) {
          ErrorMessage (fprintf (stderr, "called by [WaitForReply]\n") );
          return (result);
        }
        break;
    }
    got_reply = (response_tag == wanted_tag);
  }
  return (SUCCEEDED);
}

/**
 ** exported functions
 **/

int TCPOpenLink (Name)
char *Name;
{
#define HOSTNAMELEN 65
  char internal_resourcename[65], internal_machinename[129];
  char hostname[HOSTNAMELEN];
  int sock, param_count, result, num_a, num_b, num_c, num_d, i;
  struct sockaddr_in ops_server;
  struct hostent *hostptr, *gethostbyname();
  struct servent *servptr;
  char service_string[40];
  enum TimedStatus timed_result;
  
  struct OPSOpenCommand open_cmd;
  struct OPSOpenReply open_reply;
  
  DebugMessage (fprintf (stderr, "(tcplink.c)TCPOpenLink[\n") );
  
  if (LinkIsOpen == TRUE) {
    ErrorMessage ( fprintf (stderr, " : module [tcplink.c], function [TCPOpenLink]\n -> link already open\n") );
    return (ER_LINK_CANT);
  }
  
  if (TOPSprotocolstate != Closed) {
    DebugMessage (fprintf (stderr, " : module [tcplink.c], function [TCPOpenLink]\n -> linkops protocol error, bad protocol mode\n") );
    return (ER_LINK_SOFT);
  }
  
  /* initialise database search, setup resource name */
  if ((Name == NULL) || (*Name == NUL)) {
    DebugMessage (fprintf (stderr, "Debug       : null link name given\n") );
    ErrorMessage ( fprintf (stderr, " : invalid linkname\n") );
    return (ER_LINK_SYNTAX);
  }
  
  /**
   ** parse resource name into separate internal_resource
   ** and internal_machine parts
  **/
  {
    BOOL strip_resource, strip_machine;
    int i,j;
    
    internal_resourcename[0] = NUL;
    internal_machinename[0] = NUL;
    
    strip_resource = TRUE;
    strip_machine = FALSE;
    i = 0;
    while (strip_resource == TRUE) {
      if (i > 64) {                                 
        ErrorMessage ( fprintf (stderr, " : null resource name given\n") );
        return (ER_LINK_SYNTAX);
      }
      if (Name[i] == NUL) {
        internal_resourcename[i] = NUL;
        if (i < 1) {
          ErrorMessage ( fprintf (stderr, " : null resource name given\n") );
          return (ER_LINK_SYNTAX);
        } else {
          strip_resource = FALSE; 
        }
        
      } else {
        if (Name[i] == '@') {
          if (i < 1) {
            DebugMessage (fprintf (stderr, "Debug       : resource name before @\n") );
            ErrorMessage ( fprintf (stderr, " : resource name before @\n") );
            return (ER_LINK_SYNTAX);
          } else {
            internal_resourcename[i] = NUL;
            strip_resource = FALSE;
            strip_machine = TRUE;
            i++;
          }
        
        } else {
          internal_resourcename[i] = Name[i];
          i++;
        } 
      }
    }
   
    if (strip_machine == FALSE) {
      ErrorMessage ( fprintf (stderr, " : null machine name after @\n") );
      return (ER_LINK_SYNTAX);
    } else {
      j = 0;
      while (strip_machine == TRUE) {
        if (j > 64) {
          DebugMessage (fprintf (stderr, "Debug       : machine name too long\n") );
          ErrorMessage ( fprintf (stderr, " : machine name too long\n") );
          return (ER_LINK_SYNTAX);
        }
        if (Name[i] == NUL) {
          internal_machinename[j] = NUL;
          if (j < 1) {                                  
            DebugMessage (fprintf (stderr, "Debug       : null resource name given\n") );
            ErrorMessage ( fprintf (stderr, " : null resource name given\n") );
            return (ER_LINK_SYNTAX);
          } else {
            internal_machinename[j] = NUL;
            strip_machine = FALSE;
          }
          
        } else {
          internal_machinename[j] = Name[i];
          i++;
          j++;
        }
      }
    }
  }
  
  /**
   ** deduce name of this machine
  **/
  if (gethostname(hostname, HOSTNAMELEN) < 0) {
    ErrorMessage ( fprintf (stderr, " : module[tcplink.c], function [TCPOpenLink]\n -> gethostname failed\n") );
    return (ER_LINK_SOFT);
  }
  
  if ((servptr = getservbyname (SERVICENAME, SERVICEPROTOCOL)) == NULL) {
    ErrorMessage ( fprintf (stderr, " : linkops service is not defined in the network service database\n") );
    return (ER_LINK_SOFT);
  }
  
  /* create an Internet, stream, IP socket */
  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    ErrorMessage ( fprintf (stderr, " : module [tcplink.c], function [TCPOpenLink]\n -> socket failed\n") );
    return (ER_LINK_SOFT);
  }
  
#ifndef PCNFS
  /* set size of send & receive buffers */
  {
    int ReceiveSize = (FRAG_SIZE + 1024);
    int SendSize = (FRAG_SIZE + 1024);      
    
    if (setsockopt (sock, SOL_SOCKET, SO_RCVBUF, (char *)&ReceiveSize, sizeof(ReceiveSize)) != 0) {
      ErrorMessage ( fprintf (stderr, " : module [tcplink.c], function [TCPOpenLink]\n -> setsockopt(REC) failed, errno=[%d]\n", errno) );
      return (ER_LINK_SOFT);
    }
    if (setsockopt (sock, SOL_SOCKET, SO_SNDBUF, (char *)&SendSize, sizeof(SendSize)) != 0) {
      ErrorMessage ( fprintf (stderr, ": module [tcplink.c], function [TCPOpenLink]\n -> setsockopt(SEND) failed, errno=[%d]\n", errno) );
      return (ER_LINK_SOFT);
    }
  }
#endif /* not PCNFS */
  
  /**
  *** connect socket to remote machine 
  **/
  
  /* determine whether server = internet address or internet name */
  param_count = sscanf(internal_machinename, "%d.%d.%d.%d", &num_a, &num_b, &num_c, &num_d);
  if (param_count == 4) {
    DebugMessage (fprintf (stderr, " machinename = internet address\n") );
    SignedInt32.as_bytes[0] = num_a;
    SignedInt32.as_bytes[1] = num_b;
    SignedInt32.as_bytes[2] = num_c;
    SignedInt32.as_bytes[3] = num_d;
    ops_server.sin_addr.s_addr = SignedInt32.as_long;
    
  } else {
    /* get internet address from network table */
    hostptr = gethostbyname (internal_machinename);
    if (hostptr == 0) {
      ErrorMessage ( fprintf (stderr, " : machine [%s] not defined in host's network database\n", internal_machinename) );
      (void) close(sock);
      return (ER_LINK_SOFT);
    } else {
      /* copy address from network table into sockaddr structure */
      memcpy ((char *) &ops_server.sin_addr,
             (char *) hostptr -> h_addr,
             hostptr -> h_length);
    }
  }
  
  ops_server.sin_family = AF_INET;
  ops_server.sin_port = servptr -> s_port;
  
  InfoMessage ( fprintf (stderr, "Info        : attempting TCP open to resource [%s@%s]\n", internal_resourcename, internal_machinename) );
  
  /* can't use select to check for write activity so just do plain                                
     boring connect - this will not fail quickly - it will take the
     full 3 minutes */
  result = connect (sock, (struct sockaddr *) &ops_server, sizeof (ops_server) );
  if (result < 0) {
    DebugMessage (fprintf (stderr, " : module [tcplink.c], function [TCPOpenLink]\n -> connect1 errno=[%d]\n", errno) );
    switch (errno) {
      case ETIMEDOUT:
      case ENETUNREACH:
      case EHOSTUNREACH:
      case ECONNREFUSED:
        (void) shutdown (sock, 2);
        (void) close (sock);
        return (ER_LINK_SOFT);
        break;
      
      default:
        InfoMessage ( fprintf (stderr, "Warning     : server [%s] will not connect\n", internal_machinename) );
        (void) shutdown (sock, 2);
        (void) close (sock);
        return (ER_LINK_SOFT);
        break;
    }
  }

  DebugMessage (fprintf (stderr, "Debug       : connection to [%s] established\n", internal_machinename) );
  
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
  (void) strncpy (open_cmd.resource_name, internal_resourcename, 64);
  
  if (DoSocketWrite (sock, OPSOpenCmdSize, (char *) &open_cmd) != SUCCEEDED) {
    ErrorMessage ( fprintf(stderr, "called by (tcplink)[TCPOpenLink]\n") );
    (void) shutdown (sock, 2);
    (void) close (sock);
    return (ER_LINK_SOFT);
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
      ErrorMessage ( fprintf (stderr, " : no response from server (timed out)\n") );
    } else {
      ErrorMessage ( fprintf(stderr, "called by (tcplink)[TCPOpenLink]\n") );
    }
    (void) shutdown (sock, 2);
    (void) close (sock);
    TOPSprotocolstate = Closed;
    return (ER_LINK_SOFT);
  }
  
  if ((open_reply.service_message[0] == '-') &&
     (open_reply.service_message[1] == '\n')) {
    InfoMessage ( fprintf (stderr, "Warning     : machine [%s] does not understand open call\n", internal_machinename) );
    (void) shutdown (sock, 2);
    (void) close (sock);
    TOPSprotocolstate = Closed;
    return (ER_LINK_SOFT);
  }
  
  /* read rest of service string + reply.tag */
  timed_result = DoTimedSocketRead (sock, (41 - 2), READOPEN2_TIMEOUT, (unsigned char *) &open_reply.service_message[2]);
  if (timed_result != TIMEDSTATUSdata) {
    if (timed_result == TIMEDSTATUSnodata) {
      ErrorMessage ( fprintf (stderr, " : no response from server  (timed out)\n") );
    } else {
      ErrorMessage ( fprintf(stderr, "called by (tcplink)[TCPOpenLink]\n") );
    }
    (void) shutdown (sock, 2);
    (void) close (sock);
    TOPSprotocolstate = Closed;
    return (ER_LINK_SOFT);
  }
  for (i = 0; i < 40; i++) {
    if (open_reply.service_message[i] != service_string[i]) {
      InfoMessage ( fprintf (stderr, "Warning     : machine [%s] does not understand open call\n", internal_machinename) );
      (void) shutdown (sock, 2);
      (void) close (sock);
      TOPSprotocolstate = Closed;
      return (ER_LINK_SOFT);
    }   
  }
  
  switch (open_reply.reply_tag) {
    case OREPLY_Open:
      break;
      
    case INMOSREPLY_BadService:
      InfoMessage ( fprintf (stderr, "Warning     : machine [%s] does not support linkops service\n", internal_machinename) );
      (void) shutdown (sock, 2);
      (void) close (sock);
      TOPSprotocolstate = Closed;
      return (ER_LINK_SOFT);
      break;
      
    case INMOSREPLY_BadVersion:
      InfoMessage ( fprintf (stderr, "Warning     : machine [%s] can't cope with this version of linkops\n", internal_machinename) );
      (void) shutdown (sock, 2);
      (void) close (sock);
      TOPSprotocolstate = Closed;
      return (ER_LINK_SOFT);
      break;
      
    default:
      ErrorMessage ( fprintf (stderr, " : module [tcplink.c], function [TOPS_Open]\n -> linkops protocol error, reply_tag = [%d]\n", open_reply.reply_tag) );
      InfoMessage ( fprintf (stderr, "Warning     : linkops protocol error, reply_tag = [%d]\n", (int) open_reply.reply_tag) );
      (void) shutdown (sock, 2);
      (void) close (sock);
      TOPSprotocolstate = Closed;
      return (ER_LINK_SOFT);
      break;
  }
  
  /* read status & device name fields */
  timed_result = DoTimedSocketRead (sock, (OPSOpenReplySize - 41), READOPEN2_TIMEOUT, (unsigned char *) &open_reply.status);
  if (timed_result != TIMEDSTATUSdata) {
    if (timed_result == TIMEDSTATUSnodata) {
      ErrorMessage ( fprintf (stderr, " : no response from server (timed out)\n") );
    } else {
      ErrorMessage ( fprintf(stderr, "called by (tcplink)[TCPOpenLink]\n") );
    }
    (void) shutdown (sock, 2);
    (void) close (sock);
    TOPSprotocolstate = Closed;
    return (ER_LINK_SOFT);
  }
  switch (open_reply.status) {
    case STATUS_NOERROR:
      break;
      
    case STATUS_BAD_TARGET_NAME:
      InfoMessage ( fprintf (stderr, "Warning     : server [%s]'s database does not contain resource [%s]\n", internal_machinename, internal_resourcename) );
      (void) shutdown (sock, 2);
      (void) close (sock);
      TOPSprotocolstate = Closed;
      return (ER_NO_LINK);
      break;
      
    case STATUS_TARGET_UNAVAILABLE:
      InfoMessage ( fprintf (stderr, "Warning     : resource [%s@%s] is unavailable\n", internal_resourcename, internal_machinename) );
      (void) shutdown (sock, 2);
      (void) close (sock);
      TOPSprotocolstate = Closed;
      return (ER_LINK_BUSY);
      break;
      
    case STATUS_BAD_METHOD:
      InfoMessage ( fprintf (stderr, "Warning     : remote database method field is illegal\n") );
      (void) shutdown (sock, 2);
      (void) close (sock);
      TOPSprotocolstate = Closed;
      return (ER_NO_LINK);
      break;
      
    default:
      ErrorMessage ( fprintf (stderr, " : module [tcplink.c], function [TOPS_Open]\n -> linkops protocol error, undefined status field = [%d]\n", open_reply.status) );
      (void) shutdown (sock, 2);
      (void) close (sock);
      TOPSprotocolstate = Closed;
      return (ER_LINK_SOFT);
      break;
  }
  
  /* modify protocol state */
  TOPSprotocolstate = Synchronous;
  LinkIsOpen = TRUE;
  
  DebugMessage (fprintf (stderr, "](tcplink.c)TCPOpenLink\n") );
  return (sock);
  
}

int TCPCloseLink (LinkId)
int LinkId;
{
  struct OPSCloseCommand close_cmd;
  enum ProtocolStates nextstate;
  unsigned char result;
  
  DebugMessage (fprintf (stderr, "[(tcplink.c)TCPCloseLink\n") );
  
  if (LinkIsOpen == FALSE) {
    ErrorMessage ( fprintf (stderr, " : module [tcplink.c], function [TCPCloseLink]\n -> link not open\n") );
    return (ER_LINK_CANT);
  }
  
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
      (void) shutdown (LinkId, 2);
      (void) close (LinkId);
      TOPSprotocolstate = Closed;
      LinkIsOpen = FALSE;
      DebugMessage (fprintf (stderr, "](tcplink.c)TCPCloseLink\n") );
      return (SUCCEEDED);
      break;
  }
      
  /* valid states to issue a close command */
  close_cmd.packet_size = htons(OPSCloseCommandSize);
  close_cmd.command_tag = OCMD_Close;
  DebugMessage (fprintf (stderr, " -> sending close command, packetsize=[%d]\n", ntohs(close_cmd.packet_size)) );

  if (DoSocketWrite (LinkId, OPSCloseCommandSize, (char *) &close_cmd) != SUCCEEDED) {
    ErrorMessage ( fprintf (stderr, "called by (tcplink.c)TCPCloseLink\n") );
    (void) shutdown (LinkId, 2);
    (void) close (LinkId);
    TOPSprotocolstate = Closed;
    LinkIsOpen = FALSE;
    return (SUCCEEDED);
  }    
  
  /* alter protocol state */
  TOPSprotocolstate = nextstate;

  /* get reply */
  DebugMessage (fprintf (stderr, "Debug       : getting close reply\n") );
  result = WaitForReply (LinkId, OREPLY_Close);
  /* if reply received ok, return status field of reply */
  if (result == SUCCEEDED) {
    result = response_buffer[OREPLYCLOSE_status];
    switch (result) {
      case STATUS_NOERROR:
        break;
        
      case STATUS_BAD_OPERATION:
        ErrorMessage ( fprintf (stderr, " : module [tcplink.c], function [TCPCloseLink]\n -> remote operation failed\n") );
        break;
      
      case STATUS_LINK_FATAL:
        ErrorMessage ( fprintf (stderr, " : module [tcplink.c], function [TCPCloseLink]\n -> remote link operation failed\n") );
        break;
      
      default:
        ErrorMessage ( fprintf (stderr, " : module [tcplink.c], function [TCPCloseLink]\n -> linkops protocol error, got unexpected status = [%d]\n", result) );
        break;
    }
  } else {
    ErrorMessage ( fprintf (stderr, "called by (tcplink.c)[TCPCloseLink]\n") );
  }

  /* alter protocol state & close socket */
  TOPSprotocolstate = Closed;
  DebugMessage (fprintf (stderr, "Debug       : closing socket\n") );
  (void) shutdown (LinkId, 2);
  (void) close (LinkId);
  TOPSprotocolstate = Closed;
  LinkIsOpen = FALSE;
  DebugMessage (fprintf (stderr, "](tcplink.c)TCPCloseLink\n") );
  return (SUCCEEDED);
}

int TCPReadLink (LinkId, Buffer, Count, Timeout)
int LinkId;
char *Buffer;
unsigned int Count;
int Timeout;
{
  struct OPSReadLinkCommand readlink_cmd;
  int result, bytes_read, bytes_to_read, fragmented_read, frag_timeout, remote_bytes_read, number_frags;
  
  DebugMessage (fprintf (stderr, "[(tcplink.c)TCPReadLink\n") );
  
  /* check in valid protocol state */
  if (TOPSprotocolstate != Synchronous) {
    ErrorMessage ( fprintf (stderr, " : module [tcplink.c], function [TCPReadLink]\n -> linkops protocol error, bad protocol mode\n") );
    return (ER_LINK_SOFT);
  }
  
  bytes_read = 0;
  bytes_to_read = Count;
  
  /* calculate number of fragmentations */
  if (Count > FRAG_SIZE) {
    number_frags = (Count / FRAG_SIZE);
  } else {
    number_frags = 1;
  }
  
  /* calculate timeout according to number of fragmented read operations */
  if (number_frags > 1) {
    frag_timeout = (Timeout / number_frags);
    if (frag_timeout <= 1) {
      frag_timeout = 1;
    }
  } else {
    frag_timeout = Timeout;
  }
  
  while ((bytes_to_read > 0) && (number_frags > 0)) {
    if (bytes_to_read >= FRAG_SIZE) {
      fragmented_read = FRAG_SIZE;
    } else {
      fragmented_read = bytes_to_read;
    }
    readlink_cmd.packet_size = htons ((u_short) OPSReadLinkCommandSize);
    readlink_cmd.command_tag = OCMD_ReadLink;   
    SignedInt16.as_short = htons ((u_short) frag_timeout);
    readlink_cmd.timeout[0] = SignedInt16.as_bytes[0];
    readlink_cmd.timeout[1] = SignedInt16.as_bytes[1];
    SignedInt16.as_short = htons ((u_short) fragmented_read);
    readlink_cmd.readsize[0] = SignedInt16.as_bytes[0];
    readlink_cmd.readsize[1] = SignedInt16.as_bytes[1];
    
    DebugMessage (fprintf (stderr, "Debug       : sending readlink, packetsize=[%d]\n",  ntohs(readlink_cmd.packet_size)) );
    
    result = DoSocketWrite (LinkId, OPSReadLinkCommandSize, (char *) &readlink_cmd);
    if (result != SUCCEEDED) {
      ErrorMessage ( fprintf (stderr, "called by (tcplink.c)[TCPReadLink]\n") );
      return (result);
    }
    
    /* alter protocol state */
    TOPSprotocolstate = SynchronousReply;
    
    /* get reply message */
    DebugMessage (fprintf (stderr, "Debug       : getting readlink reply\n") );
    result = GetSyncResponse (LinkId, OREPLY_ReadLink, (unsigned char *) &Buffer[bytes_read]);
    /* check reply received ok */
    if (result != SUCCEEDED) {
      ErrorMessage ( fprintf (stderr, "called by (tcplink.c)[TCPReadLink]\n") );
      return (result);
    }
    
    /* alter protocol state */
    TOPSprotocolstate = Synchronous;
    
    /* check status field of reply */
    if (response_buffer[OREPLYREADLINK_status] != STATUS_NOERROR) {
      ErrorMessage ( fprintf (stderr, " : module [tops.c], function [TOPS_BootWrite]\n -> remote link operation failed") );
      return (ER_LINK_SOFT);
    }
    
    UnsignedInt16.as_bytes[0] = response_buffer[0];
    UnsignedInt16.as_bytes[1] = response_buffer[1];
    remote_bytes_read = ( ntohs (UnsignedInt16.as_short) - OPSReadLinkReplyBasicSize);
    
    bytes_to_read = bytes_to_read - remote_bytes_read;
    bytes_read = bytes_read + remote_bytes_read;       
    
    number_frags--;
    
  }
  DebugMessage (fprintf (stderr, "](tcplink.c)TCPReadLink\n") );
  return (bytes_read);
}

int TCPWriteLink (LinkId, Buffer, Count, Timeout)
int LinkId;
char *Buffer;
unsigned int Count;
int Timeout;
{
  struct OPSWriteLinkCommand writelink_cmd;
  int result, bytes_sent, bytes_to_send, fragmented_send, frag_timeout, bytes_written, number_frags;
  
  DebugMessage (fprintf (stderr, "[(tcplink.c)TCPWriteLink\n") );
  
  /* check in valid protocol state */
  if (TOPSprotocolstate != Synchronous) {
    ErrorMessage ( fprintf (stderr, " : module [tcplink.c], function [TCPWriteLink]\n -> linkops protocol error, bad protocol mode\n") );
    return (ER_LINK_SOFT);
  }
  
  /* calculate number of fragmentations */
  if (Count > FRAG_SIZE) {
    number_frags = (Count / FRAG_SIZE);
  } else {
    number_frags = 1;
  }
  
  /* calculate timeout according to number of fragmented read operations */
  if (number_frags > 1) {
    frag_timeout = (Timeout / number_frags);
    if (frag_timeout <= 1) {
      frag_timeout = 1;
    }
  } else {
    frag_timeout = Timeout;
  }
  
  bytes_sent = 0;
  bytes_to_send = Count;
  
  /* calculate timeout according to number of fragmented write operations */
  if ((Count >= FRAG_SIZE) && (Timeout != 0)) {
    frag_timeout = (Timeout / (Count / FRAG_SIZE) );
    if (frag_timeout <= 1) {
      frag_timeout = 1;
    }
  } else {
    frag_timeout = Timeout;
  }
  
  while ( (bytes_to_send > 0) && (number_frags > 0) ){
    if (bytes_to_send >= FRAG_SIZE) {
      fragmented_send = FRAG_SIZE;
    } else {
      fragmented_send = bytes_to_send;
    }
    writelink_cmd.packet_size = htons ((u_short) (OPSWriteLinkCommandBasicSize + fragmented_send));
    writelink_cmd.command_tag = OCMD_WriteLink;
    SignedInt16.as_short = htons ((u_short) frag_timeout);
    writelink_cmd.timeout[0] = SignedInt16.as_bytes[0];
    writelink_cmd.timeout[1] = SignedInt16.as_bytes[1];
    
    DebugMessage (fprintf (stderr, "Debug       : sending writelink (header only), packetsize=[%d]\n", ntohs(writelink_cmd.packet_size)) );
    
    /* send header fields of command */
    result = DoSocketWrite (LinkId, OPSWriteLinkCommandBasicSize, (char *) &writelink_cmd);
    if (result != SUCCEEDED) {
      ErrorMessage ( fprintf (stderr, "called by (tcplink.c)[TCPWriteLink]\n") );
      return (result);
    }
    
    DebugMessage (fprintf (stderr, "Debug       : sending writelink (data only), packetsize=[%d]\n", fragmented_send) );
    /* send actual data field of command */
    result = DoSocketWrite (LinkId, fragmented_send, (char *) &Buffer[bytes_sent]);
    if (result != SUCCEEDED) {
      ErrorMessage ( fprintf (stderr, "called by (tcplink.c)[TCPWriteLink]\n") );
      return (result);
    }
    
    /* alter protocol state */
    TOPSprotocolstate = SynchronousReply;
    
    /* get reply message */
    DebugMessage (fprintf (stderr, "Debug       : getting writelink reply\n") );
    result = GetSyncResponse (LinkId, OREPLY_WriteLink, (unsigned char *) NULL);
    /* check reply received ok */
    if (result != SUCCEEDED) {
      ErrorMessage ( fprintf (stderr, "called by (tcplink.c)[TCPWriteLink]\n") );
      return (result);
    }
    
    /* alter protocol state */
    TOPSprotocolstate = Synchronous;
    
    /* check status field of reply */
    if (response_buffer[OREPLYREADLINK_status] != STATUS_NOERROR) {
      ErrorMessage ( fprintf (stderr, " : module [tcplink.c], function [TCPWriteLink]\n -> remote link operation failed") );
      return (ER_LINK_SOFT);
    }
    
    /* check all data was written */
    UnsignedInt16.as_bytes[0] = response_buffer[OREPLYWRITELINK_byteswritten];
    UnsignedInt16.as_bytes[1] = response_buffer[OREPLYWRITELINK_byteswritten + 1];    
    bytes_written = ntohs (UnsignedInt16.as_short);
    
    bytes_to_send = bytes_to_send - bytes_written;
    bytes_sent = bytes_sent + bytes_written;
  }
  DebugMessage (fprintf (stderr, "](tcplink.c)TCPWriteLink\n") );
  return (bytes_sent);
  
}

int TCPResetLink (LinkId)
int LinkId;
{
  struct OPSResetCommand reset_cmd;
  int result;
  
  DebugMessage (fprintf (stderr, "[(tcplink.c)TCPResetLink\n") );
  
  if (TOPSprotocolstate == Synchronous) {
    /* valid state to issue command */
    reset_cmd.packet_size = htons(OPSResetCommandSize);
    reset_cmd.command_tag = OCMD_Reset; 
    SignedInt32.as_long = htonl((u_long) TSERIES_PROCESSORID);
    reset_cmd.processor_id[0] = SignedInt32.as_bytes[0];
    reset_cmd.processor_id[1] = SignedInt32.as_bytes[1];
    reset_cmd.processor_id[2] = SignedInt32.as_bytes[2];
    reset_cmd.processor_id[3] = SignedInt32.as_bytes[3];
  
    result = DoSocketWrite (LinkId, OPSResetCommandSize, (char *) &reset_cmd);
    if (result != SUCCEEDED) {
      ErrorMessage ( fprintf (stderr, "called by (tcplink.c)[TCPResetLink]\n") );
      return (result);
    } else {
      DebugMessage (fprintf (stderr, "](tcplink.c)TCPResetLink\n") );
      return (SUCCEEDED);
    }
    
  } else {
    ErrorMessage ( fprintf (stderr, " : module [tcplink.c], function [TCPResetLink]\n -> linkops protocol error, bad protocol mode\n") );
    return (ER_LINK_SOFT);
  }
}
  
int TCPAnalyseLink (LinkId)
int LinkId;
{
  struct OPSAnalyseCommand analyse_cmd;
  int result;
  
  DebugMessage (fprintf (stderr, "[(tcplink.c)TCPAnalyseLink\n") );
  
  if (TOPSprotocolstate == Synchronous) {
    /* valid state to issue command */
    analyse_cmd.packet_size = htons(OPSAnalyseCommandSize);
    analyse_cmd.command_tag = OCMD_Analyse;
    SignedInt32.as_long = htonl((u_long) TSERIES_PROCESSORID);
    analyse_cmd.processor_id[0] = SignedInt32.as_bytes[0];
    analyse_cmd.processor_id[1] = SignedInt32.as_bytes[1];
    analyse_cmd.processor_id[2] = SignedInt32.as_bytes[2];
    analyse_cmd.processor_id[3] = SignedInt32.as_bytes[3];
  
    result = DoSocketWrite (LinkId, OPSAnalyseCommandSize, (char *) &analyse_cmd);
    if (result != SUCCEEDED) {
      ErrorMessage ( fprintf (stderr, "called by (tcplink.c)[TCPAnalyseLink]\n") );
      return (result);
    } else {
      DebugMessage (fprintf (stderr, "](tcplink.c)TCPAnalyseLink\n") );
      return (SUCCEEDED);
    }
    
  } else {
    ErrorMessage ( fprintf (stderr, " : module [tcplink.c], function [TCPAnalyseLink]\n -> linkops protocol error, bad protocol mode\n") );
    return (ER_LINK_SOFT);
  }
}

int TCPTestError (LinkId)
int LinkId;
{
  struct OPSTestErrorCommand testerror_cmd;
  int result;
  
  DebugMessage (fprintf (stderr, "[(tcplink.c)TCPResetLink\n") );

  testerror_cmd.packet_size = htons(OPSTestErrorCommandSize);
  testerror_cmd.command_tag = OCMD_TestError;
  SignedInt32.as_long = htonl((u_long) TSERIES_PROCESSORID);
  testerror_cmd.processor_id[0] = SignedInt32.as_bytes[0];
  testerror_cmd.processor_id[1] = SignedInt32.as_bytes[1];
  testerror_cmd.processor_id[2] = SignedInt32.as_bytes[2];
  testerror_cmd.processor_id[3] = SignedInt32.as_bytes[3];
  
  result = DoSocketWrite (LinkId, OPSTestErrorCommandSize, (char *) &testerror_cmd);
  if (result != SUCCEEDED) {
    ErrorMessage ( fprintf (stderr, "called by (tcplink.c)[TCPTestError]\n") );
    return (result);
  }
  
  /* alter protocol state */
  TOPSprotocolstate = SynchronousReply;
  
  /* get reply message */
  DebugMessage (fprintf (stderr, "Debug       : getting testerror reply\n") );
  result = GetSyncResponse (LinkId, OREPLY_TestError, (unsigned char *) NULL);
  if (result != STATUS_NOERROR) {
    ErrorMessage ( fprintf (stderr, "called by (tcplink.c)[TCPTestError]\n") );
    return (result);
  }
  DebugMessage (fprintf (stderr, "Debug       : received correct testerror reply\n") );
  
  /* alter protocol state */
  TOPSprotocolstate = Synchronous;
  
  /* check status field of reply */
  switch (response_buffer[OREPLYTESTERROR_status]) {
    case STATUS_NOERROR:
      DebugMessage (fprintf (stderr, "](tcplink.c)TCPTestError\n") );
      return (0); /* error flag is clear */
      break;
    
    case STATUS_TARGET_ERROR:
      /* check processor id field  */
      UnsignedInt32.as_bytes[0] = response_buffer[OREPLYPEEK32_processorid];
      UnsignedInt32.as_bytes[1] = response_buffer[OREPLYPEEK32_processorid + 1];
      UnsignedInt32.as_bytes[2] = response_buffer[OREPLYPEEK32_processorid + 2];
      UnsignedInt32.as_bytes[3] = response_buffer[OREPLYPEEK32_processorid + 3];
      if (ntohl(UnsignedInt32.as_long) != TSERIES_PROCESSORID) {
        ErrorMessage ( fprintf (stderr, " : module [tcplink.c], function [TCPTestError]\n -> bad processor_id (T-series implementation)\n") );
        return (ER_LINK_SOFT);
      } else {
        return (1); /* error flag is set */
      }
      
    default:
      ErrorMessage ( fprintf (stderr, " : module [tcplink.c], function [TCPTestError]\n -> panic!\n") );
      return (ER_LINK_SOFT);
      break;
  }
  return (-99); /* to keep lint quiet ! */
}

int TCPTestRead (LinkId)
int LinkId;
{
  int result;
  
  DebugMessage (fprintf (stderr, "[(tcplink.c)TCPTestRead\n") );
  if (LinkIsOpen == TRUE) {
    result = 0; /* may or may not be able to read 1 byte */
  } else {
    ErrorMessage ( fprintf (stderr, " : module [tcplink.c], function [TCPTestRead]\n -> link not open\n") );
    result = ER_LINK_BAD; /* link is not open */
  }
  DebugMessage (fprintf (stderr, "](tcplink.c)TCPTestRead\n") );
  return (result);
}


int TCPTestWrite (LinkId)
int LinkId;
{
  int result;
  
  DebugMessage (fprintf (stderr, "[(tcplink.c)TCPTestWrite\n") );
  if (LinkIsOpen == TRUE) {
    result = 0; /* may or may not be able to write 1 byte */
  } else {
    result = ER_LINK_BAD; /* link is not open */
  }
  DebugMessage (fprintf (stderr, "](tcplink.c)TCPTestWrite\n") );
  return (result);
}

#endif /* NOCOMMS */

