/* Copyright INMOS Limited 1988,1990,1992 */

/* CMSIDENTIFIER */
static char *CMS_Id = "PRODUCT:ITEM.VARIANT-TYPE;0(DATE)";

#include <stdio.h>
#include <errno.h>
#include <string.h>

#ifndef VMS
#include <sys/types.h>
#include <sys/stat.h>
#else
#include <perror.h>
#include <unixio.h>
#endif

#ifdef LINUX
#include <unistd.h>
#endif

#include "server.h"
#include "iserver.h"
#include "pack.h"
#include "misc.h"
#include "files.h"
#include "record.h"
#include "filec.h"

static char     DataBuffer[MAX_SLICE_LENGTH + 1];
#ifndef PCTCP
extern int errno;
#endif
/*
 * SpOpen
 */

void            SpOpen(void)
{
   FILE  *fd;
   char              *name, org, mode;
   long              fileid;
   int               size;
   struct FILE_INFO  *info;

   dbgmsg("SP.OPEN");
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;

   name = &DataBuffer[0];
   size = (int) get_slice((unsigned char *) name);
   *(name + size) = 0;
   dbgmsg("\"%s\"", name);

   org = get_8();
   dbgmsg("org %d", org);
   mode = get_8();
   dbgmsg("mode %d", mode);

   if (strlen(name) == 0) {
      put_8(ER_ERROR);
      put_count(OutCount);
      return ;
   }

   if ((org != ORG_BINARY) && (org != ORG_TEXT)) {
      put_8(ER_ERROR);
      put_count(OutCount);
      return ;
   }

   if ((mode < 1) || (mode > 6)) {
      put_8(ER_ERROR);
      put_count(OutCount);
      return ;
   }

   fd = fopen(name, OpenModes[org-1][mode-1]);
   if (fd == NULL)
      put_8(ER_ERROR);
   else {
     fileid = RememberFile(fd, org);

     if (fileid == NO_SLOT)
        put_8(ER_NORESOURCE);
     else {
        /*
         * If we're opening for append, fseek to EOF since the ANSI spec
         * says that the initial position is `implementation specific'.
         * I always wanted to write portable code !
         */
         
        if ((mode == 3) || (mode == 6))
            fseek(fd, 0L, SEEK_END);
            
        put_8(ER_SUCCESS);
        put_32(fileid);
        
        info = &FileInfo[fileid];
        info->lastop = FIOP_NONE;

        dbgmsg("fd %ld", (int) fileid);
     }
   }
   
   put_count(OutCount);
}

/*
 * SpClose
 */

void            SpClose(void)
{
   long  fileid;
   FILE  *fd;
   int   forg=0;
   
   dbgmsg("SP.CLOSE");
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;

   fileid = get_32();
   
   dbgmsg("fd %ld", fileid);
   
   fd = FindFileDes(fileid, &forg);
   if (fd == NO_FILE)
      put_8(ER_BADID);
   else {
     if (ForgetFile(fileid) == EOF)
        put_8(ER_ERROR);
     else
        put_8(ER_SUCCESS);
   }

   put_count(OutCount);
}


/*
 * SpRead
 */

void            SpRead(void)
{
   char              *Data;
   int               Read; 
   long              fileid;
   int               size;
   int               forg=0;
   FILE              *fd;
   struct FILE_INFO  *info;
   
   dbgmsg("SP.READ");
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;

   fileid = get_32();
   
   dbgmsg("fd %ld", fileid);
   
#ifdef SUN
   if (fileid == FD_STDIN)
      ResetTerminal();
#endif /* SUN */

#ifdef SOLARIS
   if (fileid == FD_STDIN)
      ResetTerminal();
#endif /* SUN */

#ifdef SGI
   if (fileid == FD_STDIN)
      ResetTerminal();
#endif /* SUN */

   size = get_16();
   dbgmsg("request %d", size);
   
   fd = FindFileDes(fileid, &forg);
   if (fd == NO_FILE)
      put_8(ER_BADID);
   else {
      info = &FileInfo[fileid];
      
      if (info->lastop == FIOP_WRITE) {
         put_8(ER_NOPOSN);
      }
      else {
         Data = &DataBuffer[0];
         Read = fread(Data, 1, size, fd);
         dbgmsg("read %d", Read);

         put_8(ER_SUCCESS);
         put_slice(Read, Data);
         
         info->lastop = FIOP_READ;
      }
   }

   put_count(OutCount);
   return ;
}



/*
 * SpWrite
 */

void            SpWrite(void)
{
   char              *Data;
   int               Written;
   long              fileid;
   FILE              *fd;
   int               size;
   int               forg=0;
   struct FILE_INFO  *info;
   
   dbgmsg("SP.WRITE");
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;

   fileid = get_32();
   
   dbgmsg("fd %ld", fileid);
   fd = FindFileDes(fileid, &forg);
   
   if (fd == NO_FILE) {
      put_8(ER_BADID);
      put_16(0);
   }
   else {
      info = &FileInfo[fileid];
      
      if (info->lastop == FIOP_READ) {
         put_8(ER_NOPOSN);
         put_16(0);
      }
      else {
         Data = &DataBuffer[0];
         size = (int) get_slice((unsigned char *) Data);
         dbgmsg("%d chars", size);
      
#ifdef    VMS
         /* VMS RMS generates a record for *each* item  */
         if (fwrite(Data, size, 1, fd) != 1)
            Written = 0;
         else
            Written = size;
#else
         Written = fwrite(Data, 1, size, fd);
#endif
      
         if (fd == stdout)
            fflush(stdout);
      
         dbgmsg("wrote %d", Written);
         put_8(ER_SUCCESS);
         put_16((unsigned long) Written);
         
         info->lastop = FIOP_WRITE;
      }
   }
   put_count(OutCount);
}


/*
 * SpGetBlock
 */

void            SpGetBlock(void)
{
   char              *Data;
   int               Read;
   long              fileid;
   int               size;
   int               forg=0;
   FILE              *fd;
   struct FILE_INFO  *info;
   
   dbgmsg("SP.GETBLOCK");
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;

   fileid = get_32();
   dbgmsg("fd %ld", fileid);
   
#ifdef SUN
   if (fileid == FD_STDIN)
      ResetTerminal();
#endif /* SUN */

#ifdef SOLARIS
   if (fileid == FD_STDIN)
      ResetTerminal();
#endif /* SUN */

#ifdef SGI
   if (fileid == FD_STDIN)
      ResetTerminal();
#endif /* SUN */

   size = get_16();
   dbgmsg("request %d", size);
   
   fd = FindFileDes(fileid, &forg);
   if (fd == NO_FILE)
      put_8(ER_BADID);
   else {
      info = &FileInfo[fileid];
      
      if (info->lastop == FIOP_WRITE) {
         put_8(ER_NOPOSN);
      }
      else {
         Data = &DataBuffer[0];
         Read = fread(Data, 1, size, fd);
         dbgmsg("read %d", Read);
         put_8((Read == 0) ? ER_ERROR : ER_SUCCESS);
         put_slice(Read, Data);
         
         info->lastop = FIOP_READ;
      }
   }

   put_count(OutCount);
}


/*
 * SpPutBlock
 */

void            SpPutBlock(void)
{
   char              *Data;
   int               Written;
   long              fileid;
   FILE              *fd;
   int               size;
   int               forg=0;
   struct FILE_INFO  *info;
   
   dbgmsg("SP.PUTBLOCK");
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;

   fileid = get_32();
   fd = FindFileDes(fileid, &forg);

   dbgmsg("fd %ld", fileid);
   if (fd == NO_FILE) {
      put_8(ER_BADID);
      put_16(0);
   }
   else {
      info = &FileInfo[fileid];
      
      if (info->lastop == FIOP_READ) {
         put_8(ER_NOPOSN);
         put_16(0);
      }
      else {
         Data = &DataBuffer[0];
         size = (int) get_slice((unsigned char *) Data);
         dbgmsg("%d chars", size);

#ifdef    VMS
         /* VMS RMS generates a record for *each* item  */
         if (fwrite(Data, size, 1, fd) != 1)
            Written = 0;
         else
            Written = size;
#else
         Written = fwrite(Data, 1, size, fd);
#endif
         if (fd == stdout)
            fflush(stdout);

         dbgmsg("wrote %d", Written);
         put_8((Written == 0) ? ER_ERROR : ER_SUCCESS);
         put_16((unsigned long) Written);
         
         info->lastop = FIOP_WRITE;
      }
   }

   put_count(OutCount);
}


/*
 * SpGets
 */

void            SpGets(void)
{
   char              *Data;
   long              fileid;
   int               size;
   int               forg=0;
   FILE              *fd;
   struct FILE_INFO  *info;

   dbgmsg("SP.GETS");
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;

   fileid = get_32();
   dbgmsg("fd %ld", fileid);

#ifdef SUN
   if (fileid == FD_STDIN)
      ResetTerminal();
#endif /* SUN */

#ifdef SOLARIS
   if (fileid == FD_STDIN)
      ResetTerminal();
#endif /* SUN */

#ifdef SGI
   if (fileid == FD_STDIN)
      ResetTerminal();
#endif /* SUN */

   size = get_16();
   dbgmsg("limit %d", size);
   
   size++;
   
   fd = FindFileDes(fileid, &forg);
   if (fd == NO_FILE)
      put_8(ER_BADID);
   else {
      info = &FileInfo[fileid];
      
      if (info->lastop == FIOP_WRITE) {
         put_8(ER_NOPOSN);
      }
      else {
         Data = &DataBuffer[0];
         if (fgets(Data, size, fd) == NULL) {
            put_8(ER_ERROR);
         }
         else {
            size = strlen(Data);
            if (*(Data + size - 1) == '\n') {
               *(Data + size) = 0;
               --size;
            }
            dbgmsg("got %d", size);
            put_8(ER_SUCCESS);
            put_slice(size, Data);
         }

         info->lastop = FIOP_READ;
      }
   }

   put_count(OutCount);
}


/*
 * SpPuts
 */

void            SpPuts(void)
{
   char              *Data;
   long              fileid;
   FILE              *fd;
   int               size;
   int               forg=0;
   int               res=0;
   struct FILE_INFO  *info;
   
   dbgmsg("SP.PUTS");
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;

   fileid = get_32();
   fd = FindFileDes(fileid, &forg);
   
   dbgmsg("fd %ld", fileid);

   if (fd == NO_FILE)
      put_8(ER_BADID);
   else {
      info = &FileInfo[fileid];
      
      if (info->lastop == FIOP_READ) {
         put_8(ER_NOPOSN);
      }
      else {
         Data = &DataBuffer[0];
         size = (int) get_slice((unsigned char *) Data);
         dbgmsg("%d chars", size);
      
         *(Data + size) = 0;
         if (size > 0)     /* VMS RTL has bug with zero length strings */
            res = fputs(Data, fd);
            
         if (res == EOF)
            put_8(ER_ERROR);
         else {
            res = fputs("\n", fd);
            if (res == EOF)
               put_8(ER_ERROR);
            else
               put_8(ER_SUCCESS);
         }
         
         info->lastop = FIOP_WRITE;
      }
   }

   put_count(OutCount);
}


/*
 * SpFlush
 */

void            SpFlush(void)
{
   long  fileid;
   FILE  *fd;
   int   forg=0;
   
   dbgmsg("SP.FLUSH");
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;

   fileid = get_32();
   dbgmsg("fd %ld", fileid);
   
   fd = FindFileDes(fileid, &forg);
   
   if (fd == NO_FILE)
      put_8(ER_BADID);
   else {
      if (fflush(fd) == EOF)
         put_8(ER_ERROR);
      else
         put_8(ER_SUCCESS);
   }

   put_count(OutCount);
}


/*
 * SpSeek
 */

void            SpSeek(void)
{
   long              Offset, Origin;
   int               orig;
   long              fileid;
   FILE              *fd;
   int               forg=0;
   int               res;
   
   dbgmsg("SP.SEEK");
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;

   fileid = get_32();
   fd = FindFileDes(fileid, &forg);
   
   dbgmsg("fd %ld, org %d", fileid, forg);

   Offset = get_32();
   dbgmsg("offset %ld", Offset);

   Origin = get_32();
   dbgmsg("orig %ld", Origin);

   switch ((int) Origin) {
      case 1:
         orig = SEEK_SET;
         break;
      case 2:
         orig = SEEK_CUR;
         break;
      case 3:
         orig = SEEK_END;
         break;
   }

   if (fd == NO_FILE)
      put_8(ER_BADID);
   else {
      /* Check to see if its a record structured file */
#ifdef VMS
      if ((forg == ORG_VARIABLE) || (forg == ORG_FIXED)) {
         res = vmsseek(fileid, Offset, orig);
         put_8(res);
         put_count(OutCount);
         return;
      }
#else
      if (forg == ORG_VARIABLE) {
         if (FileInfo[fileid].format == FMT_FORMATTED)
            res = form_seq_seek(fileid, Offset, orig);
         else
            res = unform_seq_seek(fileid, Offset, orig);
            
         put_8(res);
         
         put_count(OutCount);
         return;
      }
      else if (forg == ORG_FIXED) {
         Offset *= FileInfo[fileid].mrs;
      }
#endif /* VMS */

      if (fseek(fd, Offset, orig))
         put_8(ER_ERROR);
      else {
         put_8(ER_SUCCESS);
         
         FileInfo[fileid].lastop = FIOP_NONE;
      }
   }

   put_count(OutCount);
}


/*
 * SpTell
 */

void            SpTell(void)
{
   long              Position;
   long              fileid;
   FILE              *fd;
   int               forg=0;
   struct FILE_INFO  *info;
   
   dbgmsg("SP.TELL");
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;

   fileid = get_32();
   fd = FindFileDes(fileid, &forg);
   
   dbgmsg("fd %ld", fileid);

   if (fd == NO_FILE)
      put_8(ER_BADID);
   else {
      info = &FileInfo[fileid];
      if ((info->org == ORG_VARIABLE) || (info->org == ORG_FIXED))
         Position = FileInfo[fileid].recno;
      else
         Position = ftell(fd);

      if (Position == -1L)
         put_8(ER_ERROR);
      else {
         put_8(ER_SUCCESS);
         put_32(Position);
      }
   }

   put_count(OutCount);
}


/*
 * SpEof
 */

void            SpEof(void)
{
   long  fileid;
   FILE  *fd;
   int   forg=0;
   
   dbgmsg("SP.EOF");
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;

   fileid = get_32();
   fd = FindFileDes(fileid, &forg);
   
   dbgmsg("fd %ld", fileid);

   if (fd == NO_FILE)
      put_8(ER_BADID);
   else {
      if (feof(fd))
         put_8(ER_SUCCESS);
      else
         put_8(ER_ERROR);
   }

   put_count(OutCount);
}


/*
 * SpError
 */

void            SpError(void)
{
   long  Errno;
   char  String[128];
   long  fileid;
   FILE  *fd;
   int   size;
   int   forg=0;
   
   dbgmsg("SP.ERROR");
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;

   fileid = get_32();
   fd = FindFileDes(fileid, &forg);
   
   dbgmsg("fd %ld", fileid);

   if (fd == NO_FILE) {
      put_8(ER_BADID);
   }
   else {
      if (ferror(fd)) {
         put_8(ER_SUCCESS);
         Errno = errno;
         dbgmsg("errno %d", Errno);
         put_32(Errno);
#ifdef SUN
         String[0] = 0;
#else
#ifdef SOLARIS
         String[0] = 0;
#else
#ifdef SGI
         String[0] = 0;
#else
         strcpy(&String[0], strerror(errno));
#endif
#endif
#endif
         dbgmsg("error \"%s\"", String);
         size = strlen(String);
         put_slice(size, String);
      }
      else
         put_8(ER_ERROR);
   }
   put_count(OutCount);
}


/*
 * SpRemove
 */

void            SpRemove(void)
{
   int    size;
   struct stat buf;

   dbgmsg("SP.REMOVE");
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;

   size = (int) get_slice((unsigned char *) DataBuffer);
   DataBuffer[size] = 0;
   dbgmsg("\"%s\"", DataBuffer);
   if (DataBuffer[0] == 0)
      put_8(ER_ERROR);
   else {
      /* Check to see if the file exists */
      if (stat(DataBuffer, &buf) != 0)
         put_8(ER_NOFILE);
      else {
#ifdef SUN
         if (unlink(DataBuffer))
#else
         if (remove(DataBuffer))
#endif
            put_8(ER_ERROR);
         else
            put_8(ER_SUCCESS);
      }
   }
   put_count(OutCount);
}


/*
 * SpRename
 */

void            SpRename(void)
{
   char  *Oldname, *Newname;
   int   size;

   dbgmsg("SP.RENAME");
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;
   Oldname = &DataBuffer[0];
   size = (int) get_slice((unsigned char *) Oldname);
   *(Oldname + size) = 0;
   dbgmsg("old \"%s\"", Oldname);

   Newname = Oldname + size + 1;
   size = (int) get_slice((unsigned char *) Newname);
   *(Newname + size) = 0;
   dbgmsg("new \"%s\"", Newname);

   if (*Oldname == 0) {
      put_8(ER_ERROR);
   }
   else {
      if (*Newname == 0) {
         put_8(ER_ERROR);
      }
      else {
         if (rename(Oldname, Newname)) {
            put_8(ER_ERROR);
         }
         else {
            put_8(ER_SUCCESS);
         }
      }
   }
   put_count(OutCount);
}


/*
 * SpIsaTTY
 */
 
void SpIsaTTY(void)
{
   long  fileid;
   FILE  *fd;
   int   forg=0;
   
   dbgmsg("SP.ISATTY");
   
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;
   
   fileid = get_32();
   fd = FindFileDes(fileid, &forg);
   
   dbgmsg("fd %ld", fileid);
   
   if (fd == NO_FILE)
      put_8(ER_BADID);
   else if (isatty(fileno(fd))) {
         put_8(ER_SUCCESS);
         put_8(NONZERO);
   }
   else
      put_8(ER_ERROR);
      
   put_count(OutCount);
}


/*
 * SpExists
 */
 
void SpExists(void)
{
   struct stat info;
   int         size;

   dbgmsg("SP.EXISTS");

   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;

   size = (int) get_slice((unsigned char *) DataBuffer);
   DataBuffer[size] = '\0';

   dbgmsg("\"%s\"", DataBuffer);

   if (DataBuffer[0] == '\0')
      put_8(ER_ERROR);
   else {
      put_8(ER_SUCCESS);
      
      if (stat(DataBuffer, &info) == 0)
         put_8(NONZERO);
      else
         put_8(ZERO);
   }
   
   put_count(OutCount);
}


/*
 * SpErrStat
 */
 
void SpErrStat(void)
{
   long  errnum;
   long  fileid;
   int   messlen;
   char  str[MAX_STRING_LENGTH];
   FILE  *fd;
   int   size;
   int   forg=0;
   extern int  sys_nerr;
#ifndef LINUX
   extern char *sys_errlist[];
#endif
   
   dbgmsg("SP.ERRSTAT");

   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;

   fileid = get_32();
   fd = FindFileDes(fileid, &forg);
   
   messlen = get_16();
   if (messlen > MAX_ITEM_SIZE)
      messlen = MAX_ITEM_SIZE;
   
   dbgmsg("fd %ld, maxlen %d", fileid, messlen);

   if (ferror(fd)) {
      errnum = errno;
      dbgmsg("errno %d", errnum);
#ifdef SUN
      if (errnum > sys_nerr)
         str[0] = '\0';
      else
         strcpy(str, sys_errlist[errnum]);
#else
      strcpy(str, strerror(errno));
#endif
      size = strlen(str);
      if (size > messlen) {
         put_8(ER_TRUNCATED);
         str[messlen] = '\0';
         size = messlen;
      }
      else
         put_8(ER_SUCCESS);

      put_32(errnum);

      dbgmsg("error \"%s\"", &str[0]);

      put_slice(size, str);
   }
   else
      put_8(ER_ERROR);

   put_count(OutCount);
}
