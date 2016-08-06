/* Copyright INMOS Limited 1988,1990 */

/* CMSIDENTIFIER */
static char *CMS_Id = "PRODUCT:ITEM.VARIANT-TYPE;0(DATE)";

#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef SUN
#include <sys/termios.h>
#endif

#ifdef SOLARIS
#include <sys/termios.h>
#endif

#ifdef SGI
#include <termios.h>
#endif

#ifdef LINUX
#include <stdlib.h>
#endif

#ifdef VMS
#include <ssdef.h>
#include <iodef.h>
#include <descrip.h>
#include <ssdef.h>
#include <psldef.h>
#endif

#ifdef HELIOS
#include <nonansi.h>
#include <stdlib.h>
#include <attrib.h>
#endif

#include "server.h"
#include "iserver.h"
#include "pack.h"
#include "misc.h"
#include "ttyio.h"
#include "serve.h"
#include "linkops.h"
#include "opserror.h"
#include "hbeat.h"
#include "hostc.h"

#define NOKEY   -1

static char     DataBuffer[MAX_SLICE_LENGTH + 1];
static int      Size;
static char     KeyBuf[4]={2,0,ER_AKEYREPLY,0};

/*
 * SpGetKey
 */

void            SpGetkey(void)
{
   char           c;
   unsigned char  ops_res;

   dbgmsg("SP.GETKEY");
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;

   c = GetAKey();
      
   /* Make sure heartbeat is off */
   if (HeartBeat != NULL) {
      ops_res = OPS_SendReply(ConnId, KeyBuf);
      
      if (ops_res != STATUS_NOERROR) {
         sprintf(ErrMsg,"linkops error: %s\n", lnkops_errorstring);
         close_server(MISC_EXIT, ErrMsg);
      }
         
      HeartBeat = NULL;
   }

   dbgmsg("key was %c", c);
   put_8(ER_SUCCESS);
   put_8(c);
   put_count(OutCount);
}

/*
 * DoHeartBeat
 */
 
int DoHeartBeat()
{
   unsigned char ops_res;
   
   if (PollKey()) {
      ops_res = OPS_SendReply(ConnId, KeyBuf);

      if (ops_res != STATUS_NOERROR)
         return FATAL_ERROR;
      
      HeartBeat = NULL;
      return KILL_HEARTBEAT;
   }
   
   return DO_HEARTBEAT;
}

/*
 * SpPollkey
 */

void            SpPollkey(void)
{
   dbgmsg("SP.POLLKEY");
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;
   
   if (PollKey()) {
      put_8(ER_SUCCESS);
      put_8(GetAKey());
      
      /* Disable heartbeat */
      HeartBeat = NULL;
   }
   else {
      put_8(ER_ERROR);
      
      /* Enable HeartBeat */
      if (LinkMethod == TCP_LINKOPS)
         HeartBeat = DoHeartBeat;
      else
         HeartBeat = NULL;
   }

   put_count(OutCount);
}

/*
 * SpGetenv
 */

void            SpGetenv(void)
{
   char  *Name;
   char  *getenv();

   dbgmsg("SP.GETENV");
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;

   Name = &DataBuffer[0];
   Size = get_slice(Name);
   *(Name + Size) = 0;
   dbgmsg("\"%s\"", Name);

   if (*Name == 0) {
      put_8(ER_ERROR);
      put_slice(0, Name);
   }
   else {
      if ((Name = getenv(Name)) == NULL) {
         put_8(ER_ERROR);
      }
      else {
         dbgmsg("\"%s\"", Name);
         Size = strlen(Name);
         
         if (Size > 509) {
            Size = 509;
            put_8(ER_ERROR);
         }
         else
            put_8(ER_SUCCESS);
            
         put_slice(Size, Name);
      }
   }
   put_count(OutCount);
}

/*
 * SpTime
 */

void            SpTime(void)
{
   long            Time, UTCTime;

   dbgmsg("SP.TIME");
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;

#ifdef MSDOS
   tzset();
   time(&UTCTime);
   Time = UTCTime - timezone;
   put_8(ER_SUCCESS);
   put_32(Time);
   put_32(UTCTime);
   put_count(OutCount);
   return;
#endif

#ifdef SUN
   UTCTime = time(NULL);
   Time = UTCTime + (localtime(&UTCTime))->tm_gmtoff;
   put_8(ER_SUCCESS);
   put_32(Time);
   put_32(UTCTime);
   put_count(OutCount);
   return;
#endif

#ifdef SOLARIS
   tzset();
   UTCTime = time(NULL);
   if (localtime( &UTCTime )->tm_isdst)
      Time = UTCTime - altzone;
   else
      Time = UTCTime - timezone;
   put_8(ER_SUCCESS);
   put_32(Time);
   put_32(UTCTime);
   put_count(OutCount);
   return;
#endif

#ifdef SGI
   tzset();
   UTCTime = time(NULL);
   if (localtime( &UTCTime )->tm_isdst)
      Time = UTCTime - altzone;
   else
      Time = UTCTime - timezone;
   put_8(ER_SUCCESS);
   put_32(Time);
   put_32(UTCTime);
   put_count(OutCount);
   return;
#endif

#ifdef HELIOS
   time(&Time);
   UTCTime = 0L;
   put_8(ER_SUCCESS);
   put_32(Time);
   put_32(UTCTime);
   put_count(OutCount);
   return;
#endif

#ifdef VMS
   time(&Time);
   UTCTime = 0L;
   put_8(ER_SUCCESS);
   put_32(Time);
   put_32(UTCTime);
   put_count(OutCount);
   return;
#endif
}

/*
 * SpSystem
 */
void            SpSystem(void)
{
   char           *Command;
   long            Status = 0L;
   char            Result = ER_SUCCESS;
   dbgmsg("SP.SYSTEM");
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;

#ifndef UNKNOWN_HOST
   Command = &DataBuffer[0];
   Size = get_slice(Command);
   *(Command + Size) = 0;
   dbgmsg("\"%s\"", Command);

   if (*Command == '\0') {
#ifdef VMS
      Status = 0;
#endif                          /* VMS */

#ifdef MSDOS
      Status = system(NULL);
      if (Status == 0)
         Result = ER_ERROR;
#endif                          /* MSDOS */

#ifdef SUN
      Status = system("");
      if (Status)
         Result = ER_ERROR;
#endif                          /* SUN */

#ifdef SOLARIS
      Status = system("");
      if (Status)
         Result = ER_ERROR;
#endif                          /* SUN */

#ifdef SGI
      Status = system("");
      if (Status)
         Result = ER_ERROR;
#endif                          /* SUN */
   }
   else
      Status = system(Command);

   dbgmsg("status %ld", Status);
   put_8(Result);
   put_32(Status);
   put_count(OutCount);
#else
   put_8(SP_UNIMPLEMENTED);
   put_count(OutCount);
#endif                          /* UNKNOWN_HOST */
}

/*
 * SpExit
 */

int             SpExit(void)
{
   long            Status;
   dbgmsg("SP.EXIT");
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;

   Status = get_32();
   dbgmsg("%ld", Status);

   if (Status == 999999999)
      Status = TERMINATE_OK_EXIT;
   else if (Status == -999999999)
      Status = TERMINATE_FAIL_EXIT;

   dbgmsg("exit with %d", (int) Status);

   put_8(ER_SUCCESS);
   put_count(OutCount);
   return ((int) Status);
}

/*
 * SpTranslate
 */

void SpTranslate(void)
{
   char  *name;
   long  offset;
   int   length;
   int   reslen;
   char  *getenv();

   dbgmsg("SP.TRANSLATE");

   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;

   offset = get_32();
   length = get_16();
   name = &DataBuffer[0];
   Size = get_slice(name);
   *(name + Size) = '\0';

   dbgmsg("%ld:%d \"%s\"", offset, length, name);

   if (*name == '\0')
      put_8(ER_ERROR);
   else {
      if ((name = getenv(name)) == NULL)
         put_8(ER_ERROR);
      else {
         dbgmsg("\"%s\"", name);
         put_8(ER_SUCCESS);
         reslen = strlen(name);
         put_32((unsigned long) strlen);
         if (reslen < length)
            length = reslen;
         put_slice(length,&name[offset]);
      }
   }

   put_count(OutCount);
}
