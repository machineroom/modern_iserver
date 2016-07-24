/* Copyright INMOS Limited 1988,1990 */

/* CMSIDENTIFIER */
/* PRODUCT:ITEM.VARIANT-TYPE;0(DATE) */

/* Useful macros */

/* External variables */


/* Functions */

#ifdef PROTOTYPES
#else
extern void HostEnd();
extern void HostBegin();
extern char GetAKey();
extern int PollKey();
#endif /* PROTOTYPES */
