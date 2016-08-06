/* Copyright INMOS Limited 1988,1990 */

/* CMSIDENTIFIER */
static char *CMS_Id = "PRODUCT:ITEM.VARIANT-TYPE;0(DATE)";

#include <stdio.h>
#include <setjmp.h>
#ifdef MSDOS
#include <time.h>
#endif
#include <stdarg.h>
#include <stdlib.h>

#include "server.h"
#include "iserver.h"
#include "sh.h"
#include "misc.h"
#include "files.h"
#include "linkops.h"
#include "opserror.h"
#include "pack.h"
#include "ttyio.h"

void close_server(exit_code, msg)
int exit_code;
char *msg;
{
   unsigned char ops_res;
   FILE          *errout=FileInfo[FD_STDERR].fd;
   
   if (errout == NULL)
      errout = stderr;
   
   fprintf(errout,"\nError - %s - ", PROGRAM_NAME);
   fprintf(errout, "%s", msg);
   fputs(".\n", errout);
   
   /* If we're in the Session shell, we just want to
    * return to it
    */
    
   if (InShell)
      longjmp(SafePlace, exit_code);
      
   ops_res = OPS_Close(ConnId);
   
   CloseOpenFiles();
   
   HostEnd();
   
   exit(exit_code);
}

void infomsg(char *fmt, ...)
{
   if (VerboseSwitch) {
      
      va_list argptr;
      va_start(argptr, fmt);
      vfprintf(stdout, fmt, argptr);
      va_end(argptr);
 
      fflush(stdout);
   }
}

void dbgmsg(char *fmt, ...)
{
   char  eom;
   
   if (DebugSwitch) {
      eom = (VerboseSwitch)?'\n':' ';
   
      fputs("(", stdout);
      va_list argptr;
      va_start(argptr, fmt);
      vfprintf(stdout, fmt, argptr);
      va_end(argptr);
      printf(")%c", eom);

      fflush(stdout);
   }
}

void SpFail(failcode)
int failcode;
{
   Tbuf[2] = failcode;
   put_count(1);
   
   return;
}

#ifdef VMS
char *strdup(str)
char *str;
{
   char *dst = malloc(strlen(str)+1);
   
   if (dst != NULL)
      strcpy(dst, str);
      
   return dst;
}
#endif /* VMS */

#ifdef PCNFS
int sleep(seconds)
unsigned seconds;
{
   time_t   now;
   time_t   endtime;
   
   time(&endtime);
   endtime += (time_t) seconds;
   
   time(&now);
   while (now <= endtime)
      time(&now);
}
#endif /* PCNFS */
