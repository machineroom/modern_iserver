/* Copyright INMOS Limited 1988,1990 */

/* CMSIDENTIFIER */
static char *CMS_Id = "PRODUCT:ITEM.VARIANT-TYPE;0(DATE)";

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <setjmp.h>

#ifdef MSDOS
#include <stdlib.h>
#include <malloc.h>
#endif

#ifdef VMS
#include <unixio.h>
#include <file.h>
#endif

#ifdef HELIOS
#include <stdlib.h>
#include <posix.h>
#endif

#include <stdlib.h>
#include <unistd.h>

#include "server.h"
#include "iserver.h"
#include "sh.h"
#include "misc.h"
#include "serve.h"
#include "files.h"
#include "parsecl.h"
#include "boot.h"
#include "linkops.h"
#include "opserror.h"
#include "ttyio.h"
#include "types.h"
#include "debug.h"

/* Buffer for all server operations */
unsigned char   Tbuf[TRANSACTION_BUFFER_SIZE];

/* Command lines are built in the following two strings */
char            RealCommandLine[MAX_COMMAND_LINE_LENGTH + 1];
char            DoctoredCommandLine[MAX_COMMAND_LINE_LENGTH + 1];

/* Command line switches */
bool            AnalyseSwitch;
bool            TestErrorSwitch;
bool            VerboseSwitch;
bool            LinkSwitch;
bool            ResetSwitch;
bool            ServeSwitch;
bool            LoadSwitch;
bool            DebugSwitch;
bool            SessionSwitch;
bool            OpsInfoSwitch;
bool            OpsDebugSwitch;

char            BootFileName[MAX_BOOT_FILE_LENGTH + 1];

char            LinkName[MAX_STRING_LENGTH];
int             LinkMethod;
char            ServerName[MAX_STRING_LENGTH];

char            *CoreDump;
int             CoreSize;

int             RetryTime;

char            ErrMsg[MAX_STRING_LENGTH];

/* LinkOPS connection id */
long            ConnId;
long            ProcId=0L;

char *getenv();


/*
 * Core  -  peek the transputer memory into a buffer
 */

static void     Core()
{
   unsigned char  ops_res;
   
   infomsg("Peeking root transputer...");
   
   ops_res = OPS_Peek32(ConnId, ProcId, (unsigned long) CoreSize>>2, 0x80000000L, CoreDump);

   if (ops_res != STATUS_NOERROR) {
      sprintf(ErrMsg,"Core: failed to peek root transputer because:\n   %s\n",
                        lnkops_errorstring);                
      close_server(MISC_EXIT, ErrMsg);
   }
                           
   if (isatty(1))
      infomsg("\r                           \r");
   else
      infomsg("ok\n");

   dbgmsg("peeked %d chars", CoreSize);
}

/*
 * PrintHelp
 */

static void     PrintHelp()
{
   printf("\n%s : inmos host file server\n", PROGRAM_NAME);
   printf("%s Linkops version %s.\n", HOST, VERSION_NAME);
   printf("%s\n", Copyright);
   printf("Usage:  %s {%coption}\n\n", PROGRAM_NAME, SWITCH_CHAR);
   printf("Options: \n");
   printf("        SB filename   boot the named file (same as %cSR %cSS %cSI %cSC filename)\n", SWITCH_CHAR, SWITCH_CHAR, SWITCH_CHAR, SWITCH_CHAR);
   printf("        SI            verbose mode\n");
   printf("        SE            test the error flag\n");
   printf("        SL linkname   use the named resource\n");
   printf("        SR            reset the root transputer\n");
   printf("        SA            analyse and peek the root transputer\n");
   printf("        SC filename   copy the named file to the link\n");
   printf("        SP n          set peek size to n Kchars.\n");
   printf("        SS            serve the link\n");
   printf("        SM            enter the session shell\n");
   printf("        SK interval   retry connects every interval (seconds)\n");
   printf("        SZ[1|2]       very verbose debug mode (logs all transactions)\n\n");
}

void            BreakHandler()
{
#ifndef SUN
#ifndef SOLARIS
#ifndef SGI
   signal(SIGINT, BreakHandler);
#endif
#endif
#endif

   fputc('\n', stderr);

   dbgmsg("break exit");
   if (InShell) {
      printf("aborted by user\n");
      longjmp(SafePlace, 42);
   }
   else
      close_server(USER_EXIT, "aborted by user");
}


/*
 * main
 */

int             main(argc, argv)
int             argc;
char          **argv;
{
   int             exit_status = MISC_EXIT;
   unsigned char   ops_res;
   char            *linkstr;
   char            *sess;

   if (argc == 1) {
      PrintHelp();
      return (MISC_EXIT);
   }

   InitArgs();
   MakeCommandLine(argc, argv);
   ParseCommandLine();

   signal(SIGINT, BreakHandler);
#ifdef SUN
   signal(SIGTSTP, SIG_IGN);    /* disable ctrl-z  */
#endif
#ifdef SOLARIS
   signal(SIGTSTP, SIG_IGN);    /* disable ctrl-z  */
#endif
#ifdef SGI
   signal(SIGTSTP, SIG_IGN);    /* disable ctrl-z  */
#endif
   dbgmsg("Verbose=%d, Debug=%d\n", VerboseSwitch, DebugSwitch);

   /* Open the resource */
   if (LinkName[0] == '\0') {
      linkstr = getenv("TRANSPUTER");
      
      if (linkstr == NULL)
         close_server(MISC_EXIT, "A link resource name must be supplied");
      else
         strcpy(LinkName, linkstr);
   }
      
   dbgmsg("Link name is \"%s\"", LinkName);

   SetDebugMode(OpsDebugSwitch);
   SetInfoMode(OpsInfoSwitch);
      
   do {
      ops_res = OPS_Open(LinkName, 1, &ConnId, &LinkMethod, ServerName, LinkName);
      if (ops_res == STATUS_NOERROR)
         break;
      else {
         printf("Failed to open connection to transputer because:\n   %s\n",
                     lnkops_errorstring);
         if (RetryTime == -1)
            close_server(MISC_EXIT, "Exiting.");
         else
            printf("Will retry again in %d seconds\n", RetryTime);
      }
      
      if (RetryTime >= 0)
         sleep((unsigned) RetryTime);
   } while (RetryTime != -1);
   
   dbgmsg("Using %s@%s", LinkName, ServerName);

   if ((CoreDump = (char *) malloc(CoreSize)) == NULL)
      close_server(MISC_EXIT, "failed to allocate CoreDump buffer\n");
      
   if (SessionSwitch) {
      sess = getenv("ISESSION");
      if (sess == NULL)
         sess = "session.cfg";

      exit_status = (shell(sess) == OK) ? TERMINATE_OK_EXIT : MISC_EXIT;
   }
   else {
      DupStdStreams();
      
      exit_status = server_proper(argc, argv);
      
#ifdef POOHBEAR
      CloseOpenFiles();
#endif
   }

   signal(SIGINT, SIG_IGN);     /* dont want to be interrupted while closing  */

   HostEnd();
   
   /* Close the connection */
   ops_res = OPS_Close(ConnId);
   if (ops_res != STATUS_NOERROR) {
      sprintf(ErrMsg,"Failed to close connection to transputer because\n   %s\n",
                        lnkops_errorstring);                
      close_server(MISC_EXIT, ErrMsg);
   }

   return (exit_status);
}

int server_proper(argc, argv)
int argc;
char *argv[];
{
   int           exit_status;
   unsigned char ops_res;
   
   ops_res = OPS_CommsSynchronous(ConnId);
   
   if ((ops_res != STATUS_NOERROR) && (ops_res != STATUS_TARGET_ERROR)) {
      sprintf(ErrMsg, "%s", lnkops_errorstring);
      
      close_server(MISC_EXIT, ErrMsg);
   }

   MakeCommandLine(argc, argv);
   ParseCommandLine();

   dbgmsg("original line is \"%s\"", RealCommandLine);
   dbgmsg("doctored line is \"%s\"", DoctoredCommandLine);
   
   SetDebugMode(OpsDebugSwitch);
   SetInfoMode(OpsInfoSwitch);
      
   if (DebugSwitch)
      printf("(analyse=%d error=%d verbose=%d link=%d reset=%d serve=%d load=%d dbg=%d)\n", AnalyseSwitch, TestErrorSwitch, VerboseSwitch, LinkSwitch, ResetSwitch, ServeSwitch, LoadSwitch, DebugSwitch);

   dbgmsg("peek size = %d chars\n", CoreSize);

   dbgmsg("using \"%s\" as the boot file", BootFileName);

   if (ResetSwitch && AnalyseSwitch)
      close_server(MISC_EXIT, "reset and analyse are incompatible");

   if (ResetSwitch) {
      ops_res = OPS_Reset(ConnId, ProcId);
      
      if (ops_res != STATUS_NOERROR) {
         sprintf(ErrMsg, "failed to reset root transputer because\n   %s\n",
                     lnkops_errorstring);
         close_server(MISC_EXIT, ErrMsg);
      }
      
      dbgmsg("reset root");
   }

   if (AnalyseSwitch) {
      ops_res = OPS_Analyse(ConnId, ProcId);
      
      if (ops_res != STATUS_NOERROR) {
         sprintf(ErrMsg, "failed to analyse root transputer because\n   %s\n",
                     lnkops_errorstring);
         close_server(MISC_EXIT, ErrMsg);
      }
      
      dbgmsg("analyse root");
      Core();
   }

   if (LoadSwitch) {
      ops_res = OPS_ErrorIgnore(ConnId);

      if (ops_res != STATUS_NOERROR) {
	 sprintf(ErrMsg, "failed to modify error mode because:\n   %s\n",
		     lnkops_errorstring);
         close_server(MISC_EXIT, ErrMsg);
      }

      Boot();
   }

   if (TestErrorSwitch)
      ops_res = OPS_ErrorDetect(ConnId);
   else
      ops_res = OPS_ErrorIgnore(ConnId);
      
   if (ops_res != STATUS_NOERROR) {
      sprintf(ErrMsg, "failed to modify error mode because:\n   %s\n",
		  lnkops_errorstring);
      close_server(MISC_EXIT, ErrMsg);
   }

   if (ServeSwitch)
      exit_status = Serve();
   else {
      if (ResetSwitch || AnalyseSwitch || LoadSwitch || TestErrorSwitch)
         exit_status = MISC_EXIT;
      else {
         infomsg("(no action taken)\n");
      }
   }
   
   ResetTerminal();

   return exit_status;
}
