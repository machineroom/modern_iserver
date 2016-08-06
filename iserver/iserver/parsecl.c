/* Copyright INMOS Limited 1988,1990 */

/* CMSIDENTIFIER */
static char *CMS_Id = "PRODUCT:ITEM.VARIANT-TYPE;0(DATE)";

#include <stdio.h>
#include <setjmp.h>
#include <ctype.h>
#include "server.h"
#include "iserver.h"
#include "pack.h"
#include "sh.h"
#include "parsecl.h"
#include "misc.h"
#include <string.h>
#include <stdlib.h>
    
char *FullArgv[MAXARG];
bool ServerArg[MAXARG];
int  FullArgc;

static struct CL_OPT opts[] = {
                                 {"sa", false, OPT_ANALYSE},
                                 {"sb", true,  OPT_BOOT},   
                                 {"sc", true,  OPT_COPY},
                                 {"se", false, OPT_ERROR},
                                 {"si", false, OPT_INFO}, 
                                 {"sk", true,  OPT_RETRY},
                                 {"sl", true,  OPT_LINK},
                                 {"P", true,  OPT_LINK},
                                 {"sm", false, OPT_SESSION},
                                 {"sp", true,  OPT_CORESIZE},
                                 {"sr", false, OPT_RESET},   
                                 {"ss", false, OPT_SERVE},
                                 {"st", false, OPT_IGNOREREST},
                                 {"sz1", false, OPT_OPSINFO},
                                 {"sz2", false, OPT_OPSDEBUG},
                                 {"sz", false, OPT_VERBOSE},
                                 {NULL, 0, 0}
                              };

static int nextopt(int argc, char **argv, int *optinfo, char *buff);

static bool QuotedArg(str)
char           *str;
{
   while (*str) {
      if ((*str == ' ') || (*str == '\t'))
         return true;

      str++;
   }

   return false;
}

/* InitArgs -- Initialise arg strings */
void InitArgs(void)
{
   int   arg;
   
   for (arg=0; arg<MAXARG; arg++)
      FullArgv[arg] = NULL;
}

/* serveropt -- see if the string is a server option string */
static int serveropt(str)
char *str;
{
   if ((str[0] == SWITCH_CHAR) && 
        ((str[1] == 's') || (str[1] == 'S')))
      return 1;
      
   return 0;
}

/* ParseCommandLine -- see if you can guess */
void ParseCommandLine(void)
{
   int   arg;
   bool  looking=true;
   int   opt=1;
   int   token;
   char  para[MAX_STRING_LENGTH];
   int   i;

   RetryTime       = -1;      /* Disabled */
   AnalyseSwitch   = 0;
   TestErrorSwitch = 0;
   VerboseSwitch   = 0;
   LinkSwitch      = 0;
   ResetSwitch     = 0;
   ServeSwitch     = 0;
   LoadSwitch      = 0;
   DebugSwitch     = 0;
   SessionSwitch   = 0;
   OpsInfoSwitch   = 0;
   OpsDebugSwitch  = 0;

   CoreSize = DEFAULT_CORE_SIZE;

   BootFileName[0] = '\0';
   LinkName[0] = '\0';
   
   freeargs(FullArgc, FullArgv);

   FullArgc = makeargs(RealCommandLine, FullArgv);
   if (FullArgc < 0) {
      sprintf(ErrMsg, "Command line is too long, must be less than %d characters",
                              MAX_STRING_LENGTH);
      close_server(MISC_EXIT, ErrMsg);
   }
   
   for (arg=0; arg<FullArgc; arg++)
      ServerArg[arg] = true;  /* Assume it is, until we decide otherwise */
      
   while (opt < FullArgc) {
      token = nextopt(FullArgc, FullArgv, &opt, para);
      
      switch (token) {
         case BAD_OPT:
            sprintf(ErrMsg, "Badly formed option: %c%s", 
                                    SWITCH_CHAR, para);
                                    
            close_server(MISC_EXIT, ErrMsg);
            break;
            
         case NO_PARA:
            sprintf(ErrMsg,"missing parameter after %c%s option",
                                    SWITCH_CHAR, para);
                                    
            close_server(MISC_EXIT, ErrMsg);
            break;
            
         case NO_OPT:
            ServerArg[opt-1] = false;
            break;
            
         case OPT_ANALYSE:
            AnalyseSwitch++;
            break;
            
         case OPT_BOOT:
            ResetSwitch++;
            LoadSwitch++;
            ServeSwitch++;
            VerboseSwitch++;
            strcpy(BootFileName, para);
            break;
            
         case OPT_COPY:
            LoadSwitch++;
            strcpy(BootFileName, para);
            break;
            
         case OPT_ERROR:
            TestErrorSwitch++;
            break;
            
         case OPT_INFO:
            VerboseSwitch++;
            break;
            
         case OPT_LINK:
            LinkSwitch++;
            strcpy(LinkName, para);
            break;
            
         case OPT_SESSION:
            SessionSwitch++;
            break;
            
         case OPT_RETRY:
            RetryTime = atoi(para);
            break;
            
         case OPT_CORESIZE:
            CoreSize = atoi(para);
            if (CoreSize == 0) {
               sprintf(ErrMsg, "expected a number after %csp option",
                           SWITCH_CHAR);
               close_server(MISC_EXIT, ErrMsg);
            }
            break;
            
         case OPT_RESET:
            ResetSwitch++;
            break;
            
         case OPT_SERVE:
            ServeSwitch++;
            break;
            
         case OPT_IGNOREREST:
            for (i=opt; i<FullArgc; i++)
               ServerArg[i] = false;
            opt=FullArgc;
            break;
            
         case OPT_OPSDEBUG:
            OpsDebugSwitch++;
            /* Fall through into OPT_OPSINFO */
         case OPT_OPSINFO:
            OpsInfoSwitch++;
            /* Fall through into OPT_VERBOSE */
         case OPT_VERBOSE:
            DebugSwitch++;
            break;
            
         default:
            close_server(MISC_EXIT, "Something wrong in ParseCommandLine()");
            break;
      }
   }
   
   /* Build the doctored command line */
   DoctoredCommandLine[0] = '\0';
   
   for (arg=0; arg<FullArgc; arg++) {
      if (!ServerArg[arg]) {
         if (QuotedArg(FullArgv[arg])) {
            strcat(DoctoredCommandLine, "\"");
            strcat(DoctoredCommandLine, FullArgv[arg]);
            strcat(DoctoredCommandLine, "\"");
         }
         else
            strcat(DoctoredCommandLine, FullArgv[arg]);

         strcat(DoctoredCommandLine, " ");
      }
   }
   
   i = strlen(DoctoredCommandLine);
   if (i <= 0)
   {
       DoctoredCommandLine[0] = '\0';
   }
   else
   {
       DoctoredCommandLine[i - 1] = '\0';
   }
}


/* SpCmdArg -- Give the server a command line argument */

void SpCmdArg(void)
{
   int   argno;
   int   length;
   int   arglen;
   
   dbgmsg("SP.COMMANDARG");
   
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;
   
   argno = get_16();
   length = get_16();
   
   if ((argno < 0) || (argno >= FullArgc) || (length > MAX_ITEM_SIZE))
      put_8(ER_ERROR);
   else {
      arglen = strlen(FullArgv[argno]);
      if (arglen > length) {
         put_8(ER_TRUNCATED);
         arglen = length;
      }
      else
         put_8(ER_SUCCESS);
         
      put_16(FullArgc);
      put_8(ServerArg[argno]);
      put_slice(arglen, FullArgv[argno]);
   }
   
   put_count(OutCount);
}

bool compch(c1, c2)
char c1;
char c2;
{
   if (isupper(c1))
      c1 = tolower(c1);
      
   if (isupper(c2))
      c2 = tolower(c2);
      
   if (c1 == c2)
      return true;
      
   return false;
}

bool isopt(s1, s2)
char *s1;
char *s2;
{
   int   len1=strlen(s1);
   int   len2=strlen(s2);
   int   i;
   
   if (len1 != len2)
      return false;
      
   for (i=0; i<len2; i++)
      if (compch(s1[i], s2[i]) == 0)
         return false;
         
   return true;
}

int nextopt(argc, argv, optinfo, buff)
int argc;
char **argv;
int *optinfo;
char *buff;
{
   int   arg;
   char  *argstr;
   int   i;
   bool  haspara;
   char  *str;
   
   /* take a look at the arg */
   arg = *optinfo;
   argstr = argv[arg];
   
   buff[0] = '\0';
   *optinfo = arg+1;
   
   if (*argstr == SWITCH_CHAR) {   /* it might be a server option */
      argstr++;
      str = argstr;
      for (i=0; opts[i].optstr != NULL; i++)
         if (isopt(argstr, opts[i].optstr)) {
            if (opts[i].para == false)
               return opts[i].token;
            
            strcpy(buff, opts[i].optstr);

            /* hopefully the next arg is our para */
            if (++arg >= argc)
               return NO_PARA; /* buff contains the arg str */
               
            argstr = argv[arg];
            if (argstr[0] == SWITCH_CHAR)
               return NO_PARA; /* buff contains the arg str */
               
            strcpy(buff, argstr);
            *optinfo = arg+1;
            
            return opts[i].token; /* buff contains the parameter str */
         }

      /* Test for -P<device> */
      if (*argstr == 'P')
      {
          argstr++;
          strcpy(buff, argstr);

          return OPT_LINK;
      }
      
   }
   
   /* Its not a server option */
   strcpy(buff, argstr);
   *optinfo = arg+1;
   
   return NO_OPT;
}

/*
 * MakeCommandLine --- Take argc and argv and build RealCommandLine
 */

void MakeCommandLine(int argc, char *argv[])
{
   bool  Quoted;
   char  *c;

   *RealCommandLine = 0;
   while (argc-- > 0) {
      if ((MAX_COMMAND_LINE_LENGTH - strlen(RealCommandLine)) < strlen(*argv)) {
         sprintf(ErrMsg, "Command line too long (at \"%s\")", *argv);
         
         close_server(MISC_EXIT, ErrMsg);
      }

      if (QuotedArg(*argv)) {
         Quoted = true;
         (void) strcat(RealCommandLine, "\"");
      }
      else
         Quoted = false;

      (void) strcat(RealCommandLine, *argv);
      if (Quoted)
         (void) strcat(RealCommandLine, "\"");
      (void) strcat(RealCommandLine, " ");
      ++argv;
   }

   /* Remove trailing spaces */
   for (c = RealCommandLine; *c; ++c)
      ;

   c--;
   while ((*c == ' ') || (*c == '\t'))
      c--;

   *(++c) = '\0';
}
