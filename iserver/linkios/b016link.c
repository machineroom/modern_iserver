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

static int     ActiveLink = NULL_LINK;
static long int Bytes = 0L;

int B016OpenLink(Name)
char *Name;
{
   static char     DefaultDevice[] = "/dev/bxvi0";

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

int B016CloseLink(LinkId)
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

int B016ReadLink(LinkId, Buffer, Count, Timeout)
int LinkId;
char *Buffer;
unsigned int Count;
int Timeout;
{
   register int    ret;

#ifdef TRACE
   printf("B016ReadLink: %d chars requested\n", Count);
#endif

   if (LinkId != ActiveLink)
      return ER_LINK_BAD;

   if (Count < 1)
      return ER_LINK_CANT;

   if (!SetTimeout(LinkId, Timeout))
      return ER_LINK_CANT;

   ret = read(LinkId, Buffer, Count);

   if (ret == -1) {
      if (errno == ETIME)       /* Just a timeout ? */
         ret = ER_LINK_NOSYNC;
      else
         ret = ER_LINK_CANT;
   }
   else {
      Bytes += (long int) ret;

      /* Rewind if we've sent enough */
      if (Bytes > REWIND_THRESHOLD) {
         if (lseek(LinkId, 0L, L_SET) == -1L)
            ret = ER_LINK_CANT;

         Bytes = 0L;
      }
   }

#ifdef TRACE
   printf("B016ReadLink: %d chars read\n", ret);
#endif

   return ret;
}

int B016WriteLink(LinkId, Buffer, Count, Timeout)
int LinkId;
char *Buffer;
unsigned int Count;
int Timeout;
{
   register int    ret;

#ifdef TRACE
   printf("B016WriteLink: %d chars requested\n", Count);
#endif

   if (LinkId != ActiveLink)
      return ER_LINK_BAD;

   if (Count < 1)
      return ER_LINK_CANT;

   if (!SetTimeout(LinkId, Timeout))
      return ER_LINK_CANT;

   ret = write(LinkId, Buffer, Count);

   if (ret == -1) {
      if (errno == ETIME)       /* Just a timeout ? */
         ret = ER_LINK_NOSYNC;
      else
         ret = ER_LINK_CANT;
   }
   else {
      Bytes += (long int) ret;

      /* Rewind if we've sent enough */
      if (Bytes > REWIND_THRESHOLD) {
         if (lseek(LinkId, 0L, L_SET) == -1L)
            ret = ER_LINK_CANT;

         Bytes = 0L;
      }
   }

#ifdef TRACE
   printf("B016WriteLink: %d chars written\n", ret);
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

int B016ResetLink(LinkId)
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

int B016AnalyseLink(LinkId)
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

int B016TestError(LinkId)
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

int B016TestRead(LinkId)
int LinkId;
{
   if (LinkId != ActiveLink)
      return ER_LINK_BAD;

   return 0;                  /* 'read_f' is not supported by B016 */
}

int B016TestWrite(LinkId)
int LinkId;
{
   if (LinkId != ActiveLink)
      return ER_LINK_BAD;

   return 0;                  /* 'write_f' is not supported by B016 */
}
