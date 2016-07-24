/* Copyright INMOS Limited 1988,1990 */

/* CMSIDENTIFIER */
static char *CMS_Id = "PRODUCT:ITEM.VARIANT-TYPE;0(DATE)";

#include <stdio.h>
#include <setjmp.h>
#ifdef MSDOS
#include <time.h>
#endif

#include "server.h"
#include "iserver.h"
#include "sh.h"
#include "misc.h"
#include "files.h"
#include "linkops.h"
#include "opserror.h"

void close_server(exit_code, msg)
int exit_code;
char *msg;
{
   unsigned char ops_res;
   FILE          *errout=FileInfo[FD_STDERR].fd;
   
   if (errout == NULL)
      errout = stderr;
   
   fprintf(errout,"\nError - %s - ", PROGRAM_NAME);
   fprintf(errout, msg);
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

void infomsg(fmt, a, b, c, d, e, f, g)
char *fmt;
long a, b, c, d, e, f, g;
{
   if (VerboseSwitch) {
      printf(fmt, a, b, c, d, e, f, g);
  
      fflush(stdout);
   }
}

void dbgmsg(fmt, a, b, c, d, e, f, g)
char *fmt;
long a, b, c, d, e, f, g;
{
   char  eom;
   
   if (DebugSwitch) {
      eom = (VerboseSwitch)?'\n':' ';
   
      fputs("(", stdout);
      printf(fmt, a, b, c, d, e, f, g);
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
