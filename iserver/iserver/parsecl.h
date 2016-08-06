/* Copyright INMOS Limited 1988,1990 */

/* CMSIDENTIFIER */
/* PRODUCT:ITEM.VARIANT-TYPE;0(DATE) */

/* Macros */

/* Command line option tags */
#define BAD_OPT            (-1)
#define NO_OPT             (-2)
#define OPT_ANALYSE        1
#define OPT_BOOT           2
#define OPT_COPY           3
#define OPT_ERROR          4
#define OPT_INFO           5
#define OPT_LINK           6
#define OPT_SESSION        7
#define OPT_CORESIZE       8
#define OPT_RESET          9
#define OPT_SERVE          10
#define OPT_IGNOREREST     11
#define OPT_VERBOSE        12
#define OPT_OPSINFO        13
#define OPT_OPSDEBUG       14
#define OPT_SESSIONARG     15
#define OPT_RETRY          16

/* This can be OR'ed in with the switch number */
#define NO_PARA 0x0800

#define MAXARG       50


/* Structures and types */
struct CL_OPT {
   char  *optstr;
   int   para;
   int   token;
};


/* External variables */
extern char *FullArgv[MAXARG];
extern bool ServerArg[MAXARG];
extern int FullArgc;


/* Functions */

extern void ParseCommandLine(void);
extern void InitArgs(void);
extern void SpCmdArg(void);
extern void MakeCommandLine(int argc, char *argv[]);

