/* Copyright INMOS Limited 1988,1990 */

/* CMSIDENTIFIER */
static char *CMS_Id = "PRODUCT:ITEM.VARIANT-TYPE;0(DATE)";

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <setjmp.h>

#ifndef VMS
#include <fcntl.h>
#endif

#ifdef LINUX
#include <unistd.h>
#include <stdlib.h>
#endif

#include "server.h"
#include "iserver.h"
#include "sh.h"
#include "files.h"
#include "linkops.h"
#include "opserror.h"
#include "misc.h"   
#include "ttyio.h"   
#include "parsecl.h"

#define ACCESS_TO_SHELL

#define iswhitespace(ch) ((ch == ' ') || (ch == '\t') || (ch == '\0') || (ch == '\n'))

static int doquit();
static int dodebug();
static int dohelp();
static int doshow();
static int doopen();
static int dooptions();
static int doiserver();
static int dosource();
static int initcmds();
static int execute();

int addarg();

struct CMD {
   char *cmdname;
   int  (*action)();
};

struct EXTCMD {
   char *cmdname;
   char *cmdline;
   int  (*fn)();
};

struct CMD intcmds[]={
                   {"exit",         doquit},
                   {"help",         dohelp},
                   {"iserver",      doiserver},
                   {"open",         doopen},
                   {"options",      dooptions},
                   {"quit",         doquit},
                   {"show",         doshow},
                   {"shtrace",      dodebug},
                   {"source",       dosource},
                   {NULL,           NULL}       /* This MUST be last */
                  };

struct EXTCMD extcmds[MAXCMDS];

static char OptionStr[MAX_STRING_LENGTH];

int debug;
static int LinkValid=1;

static char StdIn[MAX_STRING_LENGTH], StdOut[MAX_STRING_LENGTH], StdErr[MAX_STRING_LENGTH];

#define MAX_SOURCES  8           /* Max no of recursive sourcing */
static FILE   *ShIn[MAX_SOURCES];
static FILE   *ShOut;
static int    ShInNo=0;
static int    IsTty;

int InShell=0;
jmp_buf SafePlace;


static char *copystr(str)
char *str;
{
   char  *new;
   
   new = malloc(strlen(str)+1);
   if (new == NULL)
      return NULL;

   strcpy(new, str);
   
   return new;
}

int shell(commandfile)
char *commandfile;
{
   char  cmdline[MAX_STRING_LENGTH];
   char  *cmdargv[MAXARG];
   int   cmdargc;
   int   cmdres;
   int   res;
   
   ShInNo = 0;
   
   if ((ShIn[ShInNo] = fopen(HOST_TTY_NAME,"r")) == NULL) {
      fprintf(stderr,"Failed to open terminal stream for shell.\n");
      exit(MISC_EXIT);
   }
   
   if ((ShOut = fopen(HOST_TTY_NAME,"w")) == NULL) {
      fprintf(stderr,"Failed to open terminal stream for shell.\n");
      exit(MISC_EXIT);
   }
   
   setbuf(ShIn[ShInNo],NULL);  /* Unbuffered */
   setbuf(ShOut,NULL);  /* Unbuffered */

   debug=(int) false;
   initcmds(commandfile);
   
   strcpy(OptionStr, "");
   
   fprintf(ShOut, "\nUsing %s on server %s\n\n", LinkName, ServerName);
   
   IsTty = isatty(fileno(ShIn[ShInNo]));

   while (true)
   {
      if (res=setjmp(SafePlace)) {
         ResetTerminal();
         CloseOpenFiles();
         
         OPS_Restart(ConnId);
         
         if (debug >= 1 )
            fprintf(ShOut, "\nexit(%d)", res);
      }
      else {
         InShell = 1;
      
         if (IsTty)
            fprintf(ShOut, "\n# ");
   
         if (fgets(cmdline,MAX_STRING_LENGTH, ShIn[ShInNo]) == NULL) {
            /* End of file, have we got a previous `shell' to */
            /* fall back on */
            if (ShInNo == 0)
               break;         /* No we havn't */
               
            fclose(ShIn[ShInNo]);
            ShInNo--;
            IsTty = isatty(fileno(ShIn[ShInNo]));
            continue;
         }
            
         if (strcmp(cmdline, "\n") != 0) {
            strcpy(StdIn,  HOST_TTY_NAME);
            strcpy(StdOut, HOST_TTY_NAME);
            strcpy(StdErr, HOST_TTY_NAME);

            cmdargc = makeargs(cmdline, cmdargv);
            cmdres = execute(cmdargc, cmdargv);
            freeargs(cmdargc, cmdargv);
            
            if (cmdres == BAD)
               break;
         }
      }
   }
   
   InShell = 0;
   
   return OK;
}

void freeargs(argc, argv)
int argc;
char **argv;
{
   int   arg;
   
   for (arg=0; arg<argc; arg++) {
      if (argv[arg] != NULL)
         free(argv[arg]);
      argv[arg] = NULL;
   }
}

int makeargs(cmdline, argv)
char *cmdline;
char **argv;
{
   int argc = 0;
   int i;
   int removed = 0;
   char *lp;
   char red,*rfname;
   char err;
   static char argstr[MAX_STRING_LENGTH];

   if (strlen(cmdline) > MAX_STRING_LENGTH)
      return -1;
      
   for (i = 0; i < MAX_STRING_LENGTH; i++)
      argstr[i] = '\0';

   strcpy(argstr, cmdline);
   lp = argstr;
   
   while (*lp)
   {
      /* find first character of this argument */
      while (iswhitespace(*lp))
         lp++;
         
      err = ' ';

      /* Check for I/O redirectors */
      if ((*lp == '<') || (*lp == '>')) {
         red = *lp++;
         if ((red == '>') && (*lp == '&'))
            err = *lp++;
            
         while (iswhitespace(*lp))
            lp++;
         
         rfname = lp;
         /* find the end of this argument */
         while (!iswhitespace(*lp))
            lp++;
         *lp++ = '\0';
         
         if (red == '<')
            strcpy(StdIn, rfname);
         else if (err == '&')
            strcpy(StdErr, rfname);
         else
            strcpy(StdOut, rfname);
      }
      else {
         /* Is it quoted ? */
         if (*lp == '"') {
            argv[argc++] = ++lp;
            
            /* Find the rest of the argument */
            while ((*lp != '"') && (*lp != '\0'))
               lp++;
         }
         else {
            argv[argc++] = lp;

            /* find the end of this argument */
            while (!iswhitespace(*lp))
               lp++;
         }

         *lp++ = '\0';
      }
   }
   
   for (i=0; i<argc; i++)
      argv[i] = copystr(argv[i]);

   return argc;
}

static int callsource(cmdline, shargc, shargv)
char *cmdline;
int shargc;
char **shargv;
{
   char  *argv[MAXARG];
   char  *cmdargv[MAXARG];
   int   argc=0;
   int   cmdargc;
   int   arg;
   int   res;

   argc = addarg(argc, argv, "source", MAXARG);
   
   cmdargc = makeargs(cmdline, cmdargv);
   for (arg=0; arg<cmdargc; arg++)
      argc = addarg(argc, argv, cmdargv[arg], MAXARG);
      
   for (arg=1; arg<shargc; arg++)
      argc = addarg(argc, argv, shargv[arg], MAXARG);
      
   if (argc != 2) {
      fprintf(ShOut, "can't pass arguments to an indirect file yet!\n");
      return OK;
   }
   
   res = dosource(argc, argv);
   
   freeargs(argc, argv);
   freeargs(cmdargc, cmdargv);
   
   return res;
}

static int callserver(cmdline, shargc, shargv)   
char *cmdline;
int shargc;
char **shargv;
{
   char  *argv[MAXARG];
   char  *optargv[MAXARG];
   char  *cmdargv[MAXARG];
   int   argc=0;
   int   optargc;
   int   cmdargc;
   int   arg;
   int   exit_status;
   int   badopen=0;
   
   if (debug > 6)
      fprintf(ShOut, "callserver, cmdline=<%s>, shargc=%d\n", cmdline, shargc);

   if (!LinkValid) {
      fprintf(ShOut, "No active resource. Use the 'open' command to specify one\n");
      return OK;
   }
   
   optargc = makeargs(OptionStr, optargv);
   cmdargc = makeargs(cmdline, cmdargv);
   
   if (debug > 6)
      fprintf(ShOut, "shargc=%d, optargc=%d, cmdargc=%d\n",
                        shargc, optargc, cmdargc);
                        
   if (shargc+cmdargc+optargc > MAXARG)
   {
      fprintf(ShOut, "sh: too many arguments to handle\n");
      return OK;
   }

   /* Now combine the three arrays */
   argc = addarg(argc, argv, "iserver", MAXARG);
   for (arg=0; arg<optargc; arg++)
      argc = addarg(argc, argv, optargv[arg], MAXARG);
   
   for (arg=1; arg<shargc; arg++)
      argc = addarg(argc, argv, shargv[arg], MAXARG);
   
   for (arg=0; arg<cmdargc; arg++)
      argc = addarg(argc, argv, cmdargv[arg], MAXARG);
      
   if (argc < 0) {
      fprintf(ShOut, "sh: could not build server command line\n");
      return OK;
   }
   
   if (debug >= 9) {
      fprintf(ShOut, "calling server with:");
      
      for (arg=0; arg<argc; arg++)
         fprintf(ShOut, " %s", argv[arg]);
         
      fprintf(ShOut, "\n");
   }
   
   /* Set up the predefined streams */
   if (OpenFiles(StdIn, StdOut, StdErr) != OPEN_OK)
      close_server(MISC_EXIT, "failed to open predefined streams\n");
      
   exit_status = server_proper(argc, argv);
   
/* CloseOpenFiles(); */

   if (debug >= 1 )
      fprintf(ShOut, "exit(%d)\n", exit_status);
   
   /* Free memory used by argv */
   freeargs(argc, argv);
   freeargs(optargc, optargv);
   freeargs(cmdargc, cmdargv);
   
   return OK;
}

static int callshell(argc, argv)
int argc;
char **argv;
{
   char  cmdline[MAX_STRING_LENGTH];
   int   i;
   
   cmdline[0] = '\0';
   for (i=0; i<argc; i++) {
      strcat(cmdline, argv[i]);
      strcat(cmdline, " ");
   }
   
   system(cmdline);
   return OK;
}

static int execute(argc, argv)
int argc;
char **argv;
{
   int   i;
   int   arg;

   if (argv[0][0] == '\0')
      return OK;

   /* Check internal commands first */
   for (i=0; intcmds[i].cmdname != NULL; i++) {
      if (debug > 6)
         fprintf(ShOut, "comparing %s with %s (internal)\n",
                                 argv[0], intcmds[i].cmdname);
                                 
      if (strcmp(argv[0], intcmds[i].cmdname) == 0)
         return (intcmds[i].action)(argc, argv);
   }

   /* Is it an external command */
   for (i=0; extcmds[i].cmdname != NULL; i++) {
      if (debug > 6)
         fprintf(ShOut, "comparing %s with %s (external)\n",
                                 argv[0], intcmds[i].cmdname);
                                 
      if (strcmp(argv[0], extcmds[i].cmdname) == 0) {
         return extcmds[i].fn(extcmds[i].cmdline, argc, argv);
/*       return callserver(extcmds[i].cmdline, argc, argv); */
      }
   }
   
#ifdef ACCESS_TO_SHELL
   /* Its no command we know, pass it to the host's shell */
   return callshell(argc, argv);
#else
   /* Its not a command, pass it to the server */
   return callserver("", argc, argv);
#endif /* ACCESS_TO_SHELL */
}

static int doopen(argc,argv)
int argc;
char **argv;
{
   unsigned char  ops_res;
   
   if (argc != 2) {
      fprintf(ShOut, "usage: %s <resource name>\n", argv[0]);
      
      return OK;
   }
   
   if (LinkValid) {
      ops_res = OPS_Close(ConnId);
      LinkValid = 0;
   }
   
   /* Try and open the resource */
   do {
      ops_res = OPS_Open(argv[1], 1, &ConnId, &LinkMethod, ServerName, LinkName);
      if (ops_res == STATUS_NOERROR) {
         fprintf(ShOut, "New resource is %s on server %s\n", LinkName, ServerName);
         LinkValid = 1;
         break;
      }
      else {
         fprintf(ShOut, "Failed to open that resource because:\n   %s\n",
                        lnkops_errorstring);
         if (RetryTime == -1)
            break;
            
         fprintf(ShOut, "Will try again in %d seconds\n", RetryTime);
      }
      
      sleep((unsigned) RetryTime);
   } while (RetryTime != -1);
   
   return OK;
}

static int dosource(argc, argv)
int argc;
char **argv;
{
   if (argc != 2) {
      fprintf(ShOut, "usage: source <filename>\n");
      return OK;
   }

   /* Use next slot for the file handle */
   if ((ShInNo+1) >= MAX_SOURCES) {
      fprintf(ShOut, "too many nested shells\n");
      return OK;
   }
   
   ShInNo++;
   if ((ShIn[ShInNo] = fopen(argv[1], "r")) == NULL) {
      fprintf(ShOut, "error: can't open file: %s\n", argv[1]);
      ShInNo--;
      return OK;
   }
   
   IsTty = isatty(fileno(ShIn[ShInNo]));
   
   return OK;
}

static int dooptions(argc,argv)
int argc;
char **argv;
{
   int   i;
   int   len;
   
   if (argc == 1)
      fprintf(ShOut, "Option string: %s\n", OptionStr);
   else {
      strcpy(OptionStr, "");

      for (i=1; i<argc; i++) {
         strcat(OptionStr, argv[i]);
         strcat(OptionStr, " ");
      }
      
      len = strlen(OptionStr);
      if (len > 0)
         OptionStr[len-1] = '\0';
   }
   
   return OK;
}

static int doshow(argc, argv)
int argc;
char **argv;
{
   int   i;
   
   for (i=0; extcmds[i].cmdname != NULL; i++)
      fprintf(ShOut, "%s ->\n    iserver %s\n\n", extcmds[i].cmdname,
                                          extcmds[i].cmdline);
   
   return OK;
}

static int dohelp(argc, argv)
int argc;
char **argv;
{
   int   i;
   int   len,spaces;
   int   col=0;

   fprintf(ShOut, "Commands available:\n\n");

   for (i=0; intcmds[i].cmdname != NULL; i++)
   {
      len = strlen(intcmds[i].cmdname);

      if (col+len > 79)
      {
         col = 0;
         putc('\n',ShOut);
      }

      fprintf(ShOut, "%s", intcmds[i].cmdname);
      spaces = 8-(len%8);
      col = col+len+spaces;
      while (spaces--)
         putc(' ',ShOut);
   }

   for (i=0; extcmds[i].cmdname != NULL; i++)
   {
      len = strlen(extcmds[i].cmdname);

      if (col+len > 79)
      {
         col = 0;
         putc('\n',ShOut);
      }

      fprintf(ShOut, "%s", extcmds[i].cmdname);
      spaces = 8-(len%8);
      col = col+len+spaces;
      while (spaces--)
         putc(' ',ShOut);
   }

   putc('\n',stdout);

   return OK;
}

static int doquit(argc, argv)
int argc;
char **argv;
{
   return BAD;
}

static int dodebug(argc, argv)
int argc;
char **argv;
{
   int   level;

   if (argc > 2)
   {
      fprintf(ShOut, "usage: debug {on | off}\n");
      return OK;
   }

   if (argc == 1)
   {
      if (debug == 0)
         fprintf(ShOut, "debug is currently off\n");
      else
         fprintf(ShOut, "debug is currently on at level %d\n", debug);

      return OK;
   }

   if (strcmp("on", argv[1]) == 0)
      debug = 1;
   else if (strcmp("off", argv[1]) == 0)
      debug = 0;
   else
   {
      level = atoi(argv[1]);
      if (level > 9)
         fprintf(ShOut, "usage: debug {on | off | [ 0-9 ]}\n");
      else
         debug = level;
   }

   return OK;
}

static int initcmds(cmdfile)
char *cmdfile;
{
   int   cmdnum=1;
   char  line[MAX_STRING_LENGTH];
   char  *sp;
   FILE  *fd;

   extcmds[0].cmdname = "iserver";
   extcmds[0].cmdline = "";

   extcmds[1].cmdname = (char *)NULL;

   fd = fopen(cmdfile,"r");
   if (fd == NULL) {
      cmdfile = getenv("ISESSION");
      if (cmdfile != NULL)
         fd = fopen(cmdfile,"r");
   }
   
   if (fd == NULL) {
      fprintf(ShOut, "Command set not extended\n");

      return OK;
   }

   while (fgets(line, MAX_STRING_LENGTH, fd) != NULL)
   {
      line[strlen(line)-1] = '\0';

      for (sp = line; !iswhitespace(*sp); sp++)
         ;

      *sp = '\0';
      while (iswhitespace(*sp))
         sp++;

      extcmds[cmdnum].cmdname = malloc(strlen(line)+1);
      extcmds[cmdnum].cmdline = malloc(strlen(sp)+1);
      extcmds[cmdnum].fn = callserver;
      
      /* special case: @<stuff> means `source stuff' */
      if (*sp == '@') {
         sp++;
         extcmds[cmdnum].fn = callsource;
      }

      strcpy(extcmds[cmdnum].cmdname, line);
      strcpy(extcmds[cmdnum].cmdline, sp);
      
      cmdnum++;
   }

   fclose(fd);

   fprintf(ShOut, "%d new commands added\n", cmdnum-1);

   return OK;
}

int addarg(argc, argv, newarg, max)
int argc;
char **argv;
char *newarg;
int max;
{
   char  *new;
   
   if (debug > 6)
      fprintf(ShOut, "in addarg, argc=%d, max=%d, newarg=<%s>\n",
                                    argc, max, newarg);
   
   if ((argc < 0) || (argc >= max))
      return -1;
      
   new = copystr(newarg);
   if (new == NULL)
      return -1;
      
   argv[argc++] = new;
   
   if (debug > 6)
      fprintf(ShOut, "argc->%d\n", argc);

   return argc;
}

static int doiserver(argc, argv)
int argc;
char **argv;
{
   callserver("", argc, argv);
   return OK;
}
