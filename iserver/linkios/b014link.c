/* Copyright INMOS Limited 1988,1990,1991 */
/* CMSIDENTIFIER */
static char *CMS_Id = "PRODUCT:ITEM.VARIANT-TYPE;0(DATE)";

#include <stdio.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/file.h>           /* Declarations for 'lseek' */

#include "ims_bcmd.h"           /* INMOS device driver 'ioctl' codes */
#include "linkio.h"

#include <errno.h>

/* #define TRACE */

#define NULL_LINK          -1
#define REWIND_THRESHOLD   (1024L * 1024L)

#define TRUE               1
#define FALSE              0

static int ActiveLink = NULL_LINK;
static long int Bytes = 0L;

static int SetTimeout(int, int);

int B014OpenLink(Name)
char *Name;
{
   static char     DefaultDevice[] = "/dev/bxiv0";

   /* Already open ? */
   if (ActiveLink != NULL_LINK)
      return ER_LINK_CANT;

   /* Use default name ? */
   if ((Name == NULL) || (*Name == '\0')) {
      if ((ActiveLink = open(DefaultDevice, O_RDWR)) >= 0)
         return ActiveLink;
   }
   else {
      if ((ActiveLink = open(Name, O_RDWR)) >= 0)
         return ActiveLink;
   }

   if (errno == EBUSY)
      return ER_LINK_BUSY;
   else if (errno == ENOENT)
      return ER_LINK_SYNTAX;
   else if ((errno == ENXIO) || (errno == ENODEV))
      return ER_NO_LINK;

   return ER_LINK_CANT;
}

int B014CloseLink(LinkId)
int LinkId;
{
   /*
    * Note: On a Sun-4, 'close' always returns EPERM, so the code below
    * simply ignores the return value.  Kludge of the millenium
    */

   if (LinkId != ActiveLink)
      return ER_LINK_BAD;

   ActiveLink = NULL_LINK;

   if (close(LinkId) == -1)
      return SUCCEEDED;       /* Should be ER_LINK_CANT */

   return SUCCEEDED;
}

int B014ReadLink(LinkId, Buffer, Count, Timeout)
int LinkId;
char *Buffer;
unsigned int Count;
int Timeout;
{
   register int    ret;

#ifdef TRACE
   printf("B014ReadLink: %d chars requested\n", Count);
#endif

   if (LinkId != ActiveLink)
      return ER_LINK_BAD;

   if (Count < 1)
      return ER_LINK_CANT;

   if (!SetTimeout(LinkId, Timeout))
      return ER_LINK_CANT;

   ret = read(LinkId, Buffer, Count);

   if (ret == -1)
      ret = ER_LINK_CANT;
   else {
      Bytes += (long int) ret;

      /* Rewind if we've sent enough - stops file position ever wrapping */
      if (Bytes > REWIND_THRESHOLD) {
         if (lseek(LinkId, 0L, L_SET) == -1L)
            ret = ER_LINK_CANT;

         Bytes = 0L;
      }
   }

#ifdef TRACE
   printf("B014ReadLink: %d chars read\n", ret);
#endif

   return ret;
}

int B014WriteLink(LinkId, Buffer, Count, Timeout)
int LinkId;
char *Buffer;
unsigned int Count;
int Timeout;
{
   register int    ret;

#ifdef TRACE
   printf("B014WriteLink: %d chars requested\n", Count);
#endif

   if (LinkId != ActiveLink)
      return ER_LINK_BAD;

   if (Count < 1)
      return ER_LINK_CANT;

   if (!SetTimeout(LinkId, Timeout))
      return ER_LINK_CANT;

   ret = write(LinkId, Buffer, Count);

   if (ret == -1)
      ret = ER_LINK_CANT;
   else {
      Bytes += (long int) ret;

      /* Rewind if we've sent enough - stops file position from wrapping */
      if (Bytes > REWIND_THRESHOLD) {
         if (lseek(LinkId, 0L, L_SET) == -1L)
            ret = ER_LINK_CANT;

         Bytes = 0L;
      }
   }

#ifdef TRACE
   printf("B014WriteLink: %d chars written\n", ret);
#endif

   return ret;
}

static int SetTimeout(LinkId, Timeout)
int LinkId;
int Timeout;
{
   union IMS_IO    io;
   static int      TheCurrentTimeout = -1;

   if (Timeout != TheCurrentTimeout) {
      io.set.op = SETTIMEOUT;
      io.set.val = Timeout;
      if (ioctl(LinkId, SETFLAGS, &io) == -1)
         return FALSE;

      TheCurrentTimeout = Timeout;
   }

   return TRUE;
}

int B014ResetLink(LinkId)
int LinkId;
{
   union IMS_IO    io;

   if (LinkId != ActiveLink)
      return ER_LINK_BAD;

   io.set.op = RESET;

   if (ioctl(LinkId, SETFLAGS, &io) == -1) {
#ifdef DB
      printf("errno = %d\n", errno);
#endif
      return ER_LINK_CANT;
   }

   return SUCCEEDED;
}

int B014AnalyseLink(LinkId)
int LinkId;
{
   union IMS_IO    io;

   if (LinkId != ActiveLink)
      return ER_LINK_BAD;

   io.set.op = ANALYSE;

   if (ioctl(LinkId, SETFLAGS, &io) == -1) {
#ifdef DB
      printf("errno = %d\n", errno);
#endif
      return ER_LINK_CANT;
   }

   return SUCCEEDED;
}

int B014TestError(LinkId)
int LinkId;
{
   union IMS_IO    io;

   if (LinkId != ActiveLink)
      return ER_LINK_BAD;

   if (ioctl(LinkId, READFLAGS, &io) == -1) {
#ifdef DB
      printf("errno = %d\n", errno);
#endif
      return ER_LINK_CANT;
   }

   return (int) io.status.error_f;
}

int B014TestRead(LinkId)
int LinkId;
{
   union IMS_IO    io;

   if (LinkId != ActiveLink)
      return ER_LINK_BAD;

   if (ioctl(LinkId, READFLAGS, &io) == -1) {
#ifdef DB
      printf("errno = %d\n", errno);
#endif
      return ER_LINK_CANT;
   }

   return (int) io.status.read_f;
}

int B014TestWrite(LinkId)
int LinkId;
{
   union IMS_IO    io;

   if (LinkId != ActiveLink)
      return ER_LINK_BAD;

   if (ioctl(LinkId, READFLAGS, &io) == -1) {
#ifdef DB
      printf("errno = %d\n", errno);
#endif
      return ER_LINK_CANT;
   }

   return (int) io.status.write_f;
}
