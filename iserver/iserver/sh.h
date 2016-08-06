/* Copyright INMOS Limited 1988,1990 */

/* CMSIDENTIFIER */
/* PRODUCT:ITEM.VARIANT-TYPE;0(DATE) */

/* Macros */

#define MAXCMDS      256

#define OK           1
#define BAD          0

/* External variables */

extern int      shell();

extern int      debug;
extern int      InShell;
extern jmp_buf  SafePlace;

/* Functions */

extern int makeargs(char *cmdline, char **argv);
extern void freeargs(int argc, char **argv);
extern int addarg(int argc, char **argv, char *newarg, int max);

