/* Copyright INMOS Limited 1988,1990 */

/* CMSIDENTIFIER */
/* PRODUCT:ITEM.VARIANT-TYPE;0(DATE) */

/* Macros */

#define MAXARG       50
#define MAXCMDS      256

#define OK           1
#define BAD          0

/* External variables */

extern int      shell();

extern int      debug;
extern int      InShell;
extern jmp_buf  SafePlace;

/* Functions */

#ifdef PROTOTYPES
#else
extern int makeargs();
extern void freeargs();
extern int addarg();
#endif /* PROTOTYPES */
