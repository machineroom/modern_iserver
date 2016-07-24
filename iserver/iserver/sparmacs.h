

/* Parmacs stuf needed by iserver to pass messages though */


#ifndef SPARMACS_H
#define SPARMACS_H

typedef struct
{
  int length;
  int sender;
  int target;
  int type;
  int ctrl;
} PHEADER;
 
/* PHEADER type values */
#define FOUND      1
#define NO_DATA    0     /* used to indicate no data in probe/wait  */
#define UNDEFINED -1
#define DONT_CARE -1
#define ERROR     -1
#define TERMINATE -2
#define PRB       -3     /* probe flag                              */
#define WMSG      -4     /* wait message flag                       */
#define NOCONVERT -5     /* dont swop hi/low bytes for chars        */
#define POLL_TERMINATE -6
#define PMHOST_ID -7
#define DUMP      -8

/* PHEADER ctrl values */
#define WSSEND    -10    /* Workstation control messages to iserver */
#define WSRSEND   -11
#define WSRECV    -12
#define WSRECVR   -13
#define WSPROBE   -14
#define WSSTART   -15    /* Start header of long message            */
#define WSRSTART  -16    /* Start header of long synchronuos msg    */
#define SYSSEND   -17    /* Special system msg through poll         */
#define SYSRECV   -18

/* Max message size through iserver */
#define MAX_MSG       500


#endif 
