/* Copyright INMOS Limited 1988,1990 */

/* CMSIDENTIFIER */
/* PRODUCT:ITEM.VARIANT-TYPE;0(DATE) */

/* Macros */

#define MAX_FILES       128      /* Max no of open files */
#define NO_FILE         (FILE *) NULL
#define NO_SLOT         -1

/* Values returned by OpenFiles */
#define NO_STDIN        1
#define NO_STDOUT       2
#define NO_STDERR       3
#define OPEN_OK         4

#define FD_STDIN        0L
#define FD_STDOUT       1L
#define FD_STDERR       2L

#define ORG_BINARY      1
#define ORG_TEXT        2
#define ORG_VARIABLE    3
#define ORG_FIXED       4
#define N_ORGS          4

#define TYPE_DONTCARE   0
#define TYPE_STREAM     1
#define TYPE_RECORD     2

#define FMT_FORMATTED   0
#define FMT_UNFORMATTED 1

#define FIOP_NONE       0
#define FIOP_READ       1
#define FIOP_WRITE      2

#define LL_BUF_SIZE     512      /* Size of the formatted readahead buff */

/* Structure definitions */

struct FILE_INFO {
   FILE           *fd;           /* The file descriptor */
   int            org;           /* The file organisation */
   int            type;          /* The file type       */
   long           mrs;           /* Record file Max Record Size */
   unsigned char  *buff;         /* Record file record buffer */
   bool           dirty;         /* Record needs writing */
   long           recordsize;
   int            format;
   long           recno;
   int            lastop;        /* Previous operation (read or write) */
   bool           pasteof;       /* Have we read an EOF marker ? */
#ifdef VMS
   struct RAB     *rab;
   RecInfo        *poslist;
#else
   int            (*putfn)();
   int            (*getfn)();
   unsigned char  *llbuff;
   int            llind;
   int            lllen;
#endif /* VMS */
};

/* External variables */

extern struct FILE_INFO FileInfo[MAX_FILES];
extern char   *OpenModes[N_ORGS][6];

/* Functions */

#ifdef PROTOTYPES
#else
extern void CloseOpenFiles();
extern void SetupFiles();
extern void DupStdStreams();
extern long RememberFile();
extern int ForgetFile();
extern FILE *FindFileDes();
#endif /* PROTOTYPES */
