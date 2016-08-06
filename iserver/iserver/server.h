/* Copyright INMOS Limited 1988,1990 */

/* CMSIDENTIFIER */
/* ISERVER:SERVER_H.AAAA-FILE;2(02-MAR-92)[UNDER-DEVELOPMENT] */

static char Copyright[] = "Copyright INMOS Limited 1988,1990,1991.\n"; 

#define PROGRAM_NAME  "iserver"
#ifdef DEVELOPMENT
#define VERSION_NAME  "1.50 *EXPERIMENTAL*"
#else
#define VERSION_NAME  "1.50v of 15 October 1992"
#endif /* DEVELOPMENT */

#define MAJOR_ID       1
#define MINOR_ID       50

#define VERSION_ID ((MAJOR_ID*10)+(MINOR_ID/10))

#include <stdbool.h>
#include "sp.h"

#define TRANSACTION_BUFFER_SIZE   4096
#define MAX_SLICE_LENGTH          (TRANSACTION_BUFFER_SIZE - 2 - 1)     /* largest data item in
                                                                         * Tbuf  */
                                                                         
#define MAX_ITEM_SIZE             1024

#define BOOT_BUFFER_LENGTH        (1024 * 8)

#define DEFAULT_CORE_SIZE         (1024 * 8)    /* peeked at analyse  */

#define MAX_COMMAND_LINE_LENGTH   512
#define MAX_BOOT_FILE_LENGTH      256
#define MAX_STRING_LENGTH         256

/* machine specific stuff  */
#ifndef MSDOS
#ifndef VMS
#ifndef SUN
#ifndef SOLARIS
#ifndef SGI
#ifndef LINUX
#define UNDEFINED_HOST
#endif
#endif
#endif
#endif
#endif
#endif

#ifdef SUN
#include "bsd.h"
#endif

#ifdef SOLARIS
#include "bsd.h"
#endif

#ifdef SGI
#include "bsd.h"
#endif

#ifdef MSDOS
#include "msdos.h"
#endif

#ifdef VMS
#include "vms.h"
#endif

#ifdef LINUX
#include "bsd.h"
#endif

#ifdef UNDEFINED_HOST
#include "nohost.h"
#endif

/* all this is for SpId  */
#define BOX_X      0
#define BOX_PC     1
#define BOX_NEC    2
#define BOX_VAX    3
#define BOX_SUN3   4
#define BOX_S370   5
#define BOX_SUN4   6
#define BOX_SUN386 7
#define BOX_APOLLO 8
#define BOX_SGI    9
#define BOX_LINUX  10

#define OS_X       0
#define OS_DOS     1
#define OS_HELIOS  2
#define OS_VMS     3
#define OS_SUN40   4
#define OS_CMS     5
#define OS_LINUX   6


#ifdef sun3
#define HOST         "Sun3/SunOS4"
#define HOST_ID      BOX_SUN3
#define OS_ID        OS_SUN40
#endif

#ifdef sun4
#define HOST         "Sun4/SunOS4"
#define HOST_ID      BOX_SUN4
#define OS_ID        OS_SUN40
#endif

#ifdef sol2
#define HOST         "Sun4/Solaris2"
#define HOST_ID      BOX_SUN4
#define OS_ID        OS_SUN40
#endif

#ifdef sun386
#define HOST         "Sun386/SunOS4"
#define HOST_ID      BOX_SUN386
#define OS_ID        OS_SUN40
#endif

#ifdef SGI
#define HOST         "SGI/IRIX"
#define HOST_ID      BOX_SGI
#define OS_ID        OS_SUN40
#endif

#ifdef MSDOS
#ifdef NEC
#ifdef PCNFS
#define HOST         "NEC-PC/MS-DOS PC-NFS"
#endif
#ifdef PCTCP
#define HOST         "NEC-PC/MS-DOS PC/TCP"
#endif
#else /* Not NEC */
#ifdef PCNFS
#define HOST         "IBM-PC/MS-DOS PC-NFS"
#endif
#ifdef PCTCP
#define HOST         "IBM-PC/MS-DOS PC/TCP"
#endif
#ifdef NOCOMMS
#define HOST         "IBM-PC/MS-DOS"
#endif
#endif /* NEC */
#define HOST_ID      BOX_PC
#define OS_ID        OS_DOS
#endif

#ifdef VMS
#define HOST         "VAX/VMS"
#define HOST_ID      BOX_VAX
#define OS_ID        OS_VMS
#endif

#ifdef HELIOS
#define HOST         "HELIOS 1.0"
#define HOST_ID      BOX_X
#define OS_ID        OS_HELIOS
#endif

#ifdef LINUX
#define HOST         "Linux"
#define HOST_ID      BOX_LINUX
#define OS_ID        OS_LINUX
#endif

#ifndef HOST_ID
#define HOST         "???"
#define HOST_ID BOX_X
#endif

#ifndef OS_ID
#define OS_ID OS_X
#endif

#include "lmethod.h"

#ifndef BOARD_ID
#define BOARD_ID HW_X
#endif
