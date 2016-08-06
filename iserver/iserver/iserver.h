/* Copyright INMOS Limited 1988,1990 */

/* CMSIDENTIFIER */
/* PRODUCT:ITEM.VARIANT-TYPE;0(DATE) */

/* External variables */

extern unsigned char Tbuf[TRANSACTION_BUFFER_SIZE];
extern char          RealCommandLine[MAX_COMMAND_LINE_LENGTH+1];
extern char          DoctoredCommandLine[MAX_COMMAND_LINE_LENGTH+1];
extern char          *CoreDump;
extern char          ErrMsg[MAX_STRING_LENGTH];
extern char          BootFileName[MAX_BOOT_FILE_LENGTH + 1];
extern char          LinkName[MAX_STRING_LENGTH];
extern char          ServerName[MAX_STRING_LENGTH];

extern int           LinkMethod;
extern int           CoreSize;
extern int           RetryTime;

extern bool          AnalyseSwitch;
extern bool          TestErrorSwitch;
extern bool          VerboseSwitch;
extern bool          LinkSwitch;
extern bool          ResetSwitch;
extern bool          ServeSwitch;
extern bool          LoadSwitch;
extern bool          DebugSwitch;
extern bool          SessionSwitch;
extern bool          OpsInfoSwitch;
extern bool          OpsDebugSwitch;

extern long          ConnId;
extern long          ProcId;

/* Functions */

extern int server_proper(int argc, char *argv[]);

