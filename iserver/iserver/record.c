/* Copyright INMOS Limited 1988,1990,1991 */

/* CMSIDENTIFIER */
static char *CMS_Id = "ISERVER:RECORD_C.AAAA-FILE;2(02-MAR-92)[UNDER-DEVELOPMENT]";

#include <stdio.h>
#ifndef VMS
#include <malloc.h>
#endif
#include "server.h"
#include "iserver.h"
#include "record.h"
#include "files.h"
#include "pack.h"
#include "misc.h"
#ifdef VMS
#include <rms.h>
#include <rmsdef.h>
#include "vaxrms.h"
#endif /* VMS */

#ifdef LINUX
#include <string.h>
#endif

#define CR  '\r'
#define NL  '\n'
#define EOF_CH(mrs) ((mrs%96)+32)    /* Will produce a printable char */

#ifndef VMS
static char *special_gets();
static int form_seq_write_rec();
static int unform_seq_write_rec();
static int direct_write_rec();
static int form_seq_read_rec();
static int unform_seq_read_rec();
static int direct_read_rec();
#endif

void SpOpenRec(void)
{
   char              fname[MAX_SLICE_LENGTH+1];
   int               namelen;
   int               fileorg;
   int               filemode;
   int               filetype;
   int               format;
   long              mrs;
   long              fileid;
   FILE              *fd;
   struct FILE_INFO  *info;
#ifdef VMS
   struct RAB        *rab;
   RecInfo           *rinfo;
   unsigned int      rfm,rat;
#endif /* VMS */
   
   dbgmsg("SP.OPENREC");

   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;
   
   namelen  = get_slice((unsigned char *) fname);
   fname[namelen] = '\0';
   fileorg  = (int) get_8();
   filemode = (int) get_8();
   filetype = (int) get_8();
   format   = (int) get_8();
   mrs      = get_32();
   
   dbgmsg("file=\"%s\" %d:%d:%d:%d %ld", fname, fileorg, filemode, 
                                                filetype, format, mrs);                                                    
   
   /* Validate request */
   if ((strlen(fname) == 0) || (fileorg < 3) || (fileorg > 4) ||
                                 (filetype < 0) || (filetype > 2) ||
                                   (filemode < 1) || (filemode > 6) ||
                                    (format < 0) || (format > 1)) {
      SpFail(ER_ERROR);
      return;
   }
      
   if (filetype == TYPE_DONTCARE)
      filetype = TYPE_RECORD;
   
#ifdef VMS
   if (fileorg == ORG_VARIABLE)
      rfm = FAB$C_VAR;
   rfm = (fileorg == ORG_VARIABLE)?FAB$C_VAR:FAB$C_FIX;
   rat = (filetype == TYPE_RECORD)?FAB$M_FTN:0;    /* FTN:FAB$M_CR */
   
   /* We ignore format under VMS */
   
   /* decide whether to open or create file */
   if ((filemode == 1) || (filemode == 4))
      rab = vmsopen(fname, rfm, rat);
   else
      rab = vmscreate(fname, rfm, rat, rfm);

   if (rab == (struct RAB *) NULL) {
      SpFail(ER_NOFILE);
      return;
   }
   
   rinfo = newinfo();
   if (rinfo == (RecInfo *) NULL) {
      vmsclose(rab);
      SpFail(ER_ERROR);
   }
   else {
      rinfo->baserec = 0;
      rinfo->prev = (FilePos *) NULL;
      rinfo->next = (FilePos *) NULL;
   }
   
   fd = stdout;         /* Must be something ! */
#else
   fd = fopen(fname, OpenModes[fileorg-1][filemode-1]);
   if (fd == NULL) {
      SpFail(ER_NOFILE);
      return;
   }
#endif /* VMS */
   
   /* File opened OK */
   fileid = RememberFile(fd, fileorg);
   if (fileid == NO_SLOT) {
      SpFail(ER_NORESOURCE);
      return;
   }
   
   /* Things are looking good Houston....beep */
   info = &FileInfo[fileid];
   info->buff = (unsigned char *) malloc((int) mrs+3); /* Room for CR, NL, EOS */
   if (info->buff == (unsigned char *) NULL) {      
      ForgetFile(fileid);
      SpFail(ER_NORESOURCE);
      return;
   }
   
   info->mrs = mrs;
   info->recordsize = -1;
   info->format = format;
   info->type = filetype;
   info->recno = 0;
   info->lastop = FIOP_NONE;
   info->pasteof = false;
   
#ifdef VMS
   info->rab = rab;
   info->poslist = rinfo;
#else
   if (fileorg == ORG_VARIABLE) {
      info->putfn = (format == FMT_FORMATTED)?form_seq_write_rec:unform_seq_write_rec;
      info->getfn = (format == FMT_FORMATTED)?form_seq_read_rec:unform_seq_read_rec;
      
      /* Need to allocate a read-ahead buffer for formatted files */
      if (format == FMT_FORMATTED) {
         info->llbuff = (unsigned char *) malloc(LL_BUF_SIZE);
         info->llind = LL_BUF_SIZE;
         if (info->llbuff == (unsigned char *) NULL) {
            free(info->buff);
            ForgetFile(fileid);
            SpFail(ER_NORESOURCE);
            return;
         }
      }
      else {
         info->llbuff = NULL;
         info->llind = 0;
      }
   }
   else {
      info->putfn = direct_write_rec;
      info->getfn = direct_read_rec;
      info->llbuff = NULL;
      info->llind = 0;
   }
#endif /* VMS */
   
   put_8(ER_SUCCESS);
   put_32(mrs);
   put_32(fileid);
   put_32(filetype);
   
   put_count(OutCount);
}

void SpPutRec(void)
{
   long              fileid;
   FILE              *fd;
   long              recordsize;
   int               chunksize;
   long              offset;
   bool              dowrite=false;
   struct FILE_INFO  *info;
   int               res;
   
   dbgmsg("SP.PUTREC");

   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;
   
   fileid = get_32();
   recordsize = get_32();
   chunksize = get_16();
   offset    = get_32();
   
   if (get_8())
      dowrite = true;
      
   dbgmsg("fileid=%ld, recordsize=%ld, chunksize=%d, offset=%ld, write=%d",
            fileid, recordsize, chunksize, offset, dowrite);
      
   if ((fileid < 0) || (fileid > MAX_FILES)) {
      put_8(ER_BADID);
      put_count(OutCount);
      return;
   }

   info = &FileInfo[fileid];
   
   if (info->fd == NULL) {
      put_8(ER_BADID);
      put_count(OutCount);
      return;
   }
   
   if (info->lastop == FIOP_READ) {
      put_8(ER_NOPOSN);
      put_count(OutCount);
      return;
   }
   
   if (info->recordsize == -1)
      info->recordsize = recordsize;
      
   dbgmsg("recordsize=%ld, info->recordsize=%ld", recordsize, info->recordsize);
   dbgmsg("info->mrs=%ld", info->mrs);
   
   if ((recordsize > info->recordsize) || 
     (recordsize > info->mrs) || (offset+(long)chunksize > info->mrs)) {
      dbgmsg("ER_ERROR: recordsize screwup");
      put_8(ER_ERROR);
      put_count(OutCount);
      return;
   }
   
   get_slice(&(info->buff[offset]));
   info->dirty = true;
   
   if (dowrite == true) {
      info->recno++;
#ifdef VMS
      res = vmsput(info->rab, info->buff, (int) recordsize);
      vmssavepos(info);
#else
      res = info->putfn(info->fd, info->buff, recordsize);
#endif /* VMS */
      info->dirty = false;
      info->recordsize = -1;
   }
   else
      res = ER_SUCCESS;
   
   info->lastop = FIOP_WRITE;
   info->pasteof = false;
   
   put_8(res);
   put_count(OutCount);
}

void SpGetRec(void)
{
   long              fileid;
   FILE              *fd;
   int               chunksize;
   long              offset;
   bool              doread=false;
   struct FILE_INFO  *info;
   int               res=ER_SUCCESS;
   
   dbgmsg("SP.GETREC");

   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;
   
   fileid = get_32();
   chunksize = get_16();
   offset    = get_32();
   
   if (get_8())
      doread = true;
      
   dbgmsg("fileid=%d, chunksize=%d, offset=%ld, doread=%d",
               fileid, chunksize, offset, doread);
               
   if ((fileid < 0) || (fileid > MAX_FILES)) {
      put_8(ER_BADID);
      put_count(OutCount);
      return;
   }

   info = &FileInfo[fileid];
   
   if (info->lastop == FIOP_WRITE) {
      put_8(ER_NOPOSN);
      put_count(OutCount);
      return;
   }
   
   if ((offset > info->mrs) || (offset < 0)) {
      put_8(ER_ERROR);
      put_count(OutCount);
      return;
   }
      
   if (doread == true) {
      info->recno++;
#ifdef VMS
      res = vmsget(info);
      vmssavepos(info);
#else
      res = info->getfn(info);
#endif
   }
   
   dbgmsg("res=%d", res);
   
   if ((offset + (long)chunksize) > info->recordsize)
      chunksize = info->recordsize - offset;
      
   dbgmsg("recsiz=%ld, offset=%ld, chunksize=%d",
               info->recordsize, offset, chunksize);
   
   info->lastop = FIOP_READ;
   
   put_8(res);
   if (res == ER_SUCCESS) {
      put_32(info->recordsize);
      put_slice(chunksize, &(info->buff[offset]));
   }
   put_count(OutCount);
}

#ifndef VMS
static int unform_seq_read_rec(info)
struct FILE_INFO *info;
{
   char  buff[4];
   long  len;
   int   nread;
   int   bytes;
   int   toread;
   char  *buffer = (char *) info->buff;
   
   if (info->pasteof)
      return ER_EOF;
   
   nread = fread(buff, 4, 1, info->fd);
   if (nread != 1) {
      info->pasteof = true;
      return ER_EOF;
   }
      
   len = (long) unpack32(buff);
   
   dbgmsg("Record read is %ld bytes long", len);
   
   if (len > info->mrs) {
      info->recordsize = 0;
      return ER_RECTOOBIG;
   }
   
   if (len < 0) {
      info->recordsize = 0;
      return ER_ERROR;
   }

   toread = len;
   while (toread > 0) {
      bytes = (toread > MAX_CHUNK_SIZE)?MAX_CHUNK_SIZE:toread;
      nread = fread(buffer, bytes, 1, info->fd);
      if (nread != 1) {
         info->pasteof = true;
         return ER_EOF;
      }
         
      buffer += bytes;
      toread -= (long) bytes;
   }
   
   nread = fread(buff, 4, 1, info->fd);
   if (nread != 1) {
      info->pasteof = true;
      return ER_EOF;
   }
      
   if (len != (long) unpack32(buff))   
      return ER_ERROR;
      
   info->recordsize = len;
   
   return ER_SUCCESS;
}

static int form_seq_read_rec(info)
struct FILE_INFO *info;
{
   char  *res;
   char  *buff = (char *) info->buff;
   int   bytes;
   int   bitlen;
   long  toread = (info->mrs)+1;
   long  reclen = 0;
   char  eofch;
   int   i;
   
   dbgmsg("(form_seq_read_rec)");
   
   if (info->pasteof) {
      dbgmsg("PastEOF");
      return ER_EOF;
   }
   
   while (toread > 0) {
      bytes = (toread > MAX_CHUNK_SIZE)?MAX_CHUNK_SIZE:(int)toread;

      dbgmsg("going to fgets %d bytes", bytes);
      
#ifdef MSDOS
      res = special_gets(buff, bytes+2, info, &bitlen);
#else
      res = special_gets(buff, bytes+1, info, &bitlen);
#endif
      
      if (res == NULL) {
         info->pasteof = true;
         return ER_EOF;
      }
         
      reclen += (long) bitlen;
      
      dbgmsg("bitlen=%d, reclen=%ld", bitlen, reclen);
      
      /* See if we've got the terminator */
      dbgmsg("last chars of this chunk are 0x%02x, 0x%02x", 
                     buff[bitlen-1], buff[bitlen]);
      if ((bitlen != 0) && (buff[bitlen-1] == '\n')) {
         info->recordsize = reclen-1;
         
         return ER_SUCCESS;
      }
      
      buff += bitlen;
      toread -= (long) bitlen;
   }
   
   /* We've read info->mrs bytes without finding a terminator ! */
   info->recordsize = 0;
   return ER_NOTERM;
}

static int direct_read_rec(info)
struct FILE_INFO *info;
{
   char  *buff = (char *) info->buff;
   int   bytes;
   int   nread;
   long  toread = info->mrs;
   
   info->recordsize = -1;
   
   if (info->pasteof)
      return ER_EOF;

   while (toread > 0) {
      bytes = (toread > MAX_CHUNK_SIZE)?MAX_CHUNK_SIZE:(int) toread;
      
      nread = fread(buff, bytes, 1, info->fd);
      if (nread != 1) {
         info->pasteof = true;
         return ER_EOF;
      }
         
      toread -= (long) bytes;
      buff += bytes;
   }
   
   info->recordsize = info->mrs;
   
   return ER_SUCCESS;
}

static int unform_seq_write_rec(fd, buffer, len)
FILE *fd;
char *buffer;
long len;
{
   char  buff[4];
   int   bytes;
   
   dbgmsg("unform_seq_write_rec");
   
   pack32(len, buff);

   if (fwrite(buff, 4, 1, fd) != 1)
      return ER_ERROR;

   while (len > 0) {
      bytes = (len > MAX_CHUNK_SIZE)?MAX_CHUNK_SIZE:(int) len;
      
      if (fwrite(buffer, bytes, 1, fd) != 1)
         return ER_ERROR;
         
      buffer += bytes;
      len -= (long) bytes;
   }

   if (fwrite(buff, 4, 1, fd) != 1)
      return ER_ERROR;
      
   return ER_SUCCESS;
}

static int form_seq_write_rec(fd, buffer, len)
FILE *fd;
char *buffer;
long len;
{
   int   bytes;
   int   ch;
   
   dbgmsg("form_seq_write_rec, len=%ld", len);
   
   while (len > 0L) {
      bytes = (len > MAX_CHUNK_SIZE)?MAX_CHUNK_SIZE:(int) len;
      
      if (fwrite(buffer, bytes, 1, fd) != 1)
         return ER_ERROR;
         
      buffer += bytes;
      len -= (long) bytes;
   }

#ifdef MSDOS
   if (fputc(CR, fd) != CR)
      return ER_ERROR;
#endif

   if (fputc(NL, fd) != NL)
      return ER_ERROR;
      
   return ER_SUCCESS;
}

static int direct_write_rec(fd, buffer, len)
FILE *fd;
char *buffer;
long len;
{
   int   bytes;
   
   dbgmsg("direct_write_rec");
   
   while (len > 0) {
      bytes = (len > MAX_CHUNK_SIZE)?MAX_CHUNK_SIZE:len;
      
      if (fwrite(buffer, bytes, 1, fd) != 1)
         return ER_ERROR;
         
      buffer += bytes;
      len -= (long) bytes;
   }
   
   return ER_SUCCESS;
}

int form_seq_seek(fileid, offset, origin)
long fileid;
long offset;
long origin;
{
   long              here;
   int               i;
   int               nread;
   long              nbytes;
   struct FILE_INFO  *info = &FileInfo[fileid];
   FILE              *fd = info->fd;
   char              buffer[BUFSIZ], *buff;
   bool              sof = false;
   bool              looking = true;
   
   dbgmsg("form_seq_seek(%ld, %ld, %d)", fileid, offset, origin);
   
   /* Invalidate current record */
   info->recordsize = -1;
   
   if ((info->llbuff != NULL) && (info->lastop == FIOP_READ)) {
      dbgmsg("moving back %d bytes", info->lllen - info->llind);
      
      fseek(fd, -(long) (info->lllen - info->llind), SEEK_CUR);
      
      /* Invalidate the read-ahead buffer */
      info->llind = info->lllen;
   }
   
   switch ((int) origin) {
      case SEEK_SET:       /* Rewind only */
         if (offset != 0)
            return ER_ERROR;
            
         rewind(fd);
         info->pasteof = false;
         info->lastop = FIOP_NONE;
         
         return ER_SUCCESS;
         
      case SEEK_CUR:       /* Backspace and no movement only */
         if (offset == 0) {
            fseek(fd, 0L, SEEK_CUR);
            info->pasteof = false;
            info->lastop = FIOP_NONE;
            return ER_SUCCESS;
         }
         else if (offset != -1)
            return ER_ERROR;
         break;

      case SEEK_END:       /* Not allowed */
         return ER_ERROR;
   }
   
   /* If we get this far, then we have chosen to backspace a SINGLE record */
   if (info->pasteof) {
      if (fseek(fd, 0L, SEEK_END) == -1)
         return ER_ERROR;

      info->pasteof = false;
      info->lastop = FIOP_NONE;
      
      return ER_SUCCESS;
   }
   
   while (looking) {
      /*
       * If we've looked back as far as the beginning of file
       * simply rewind the file
       */
      if (sof) {
         dbgmsg("SOF");
         
         rewind(fd);
         looking = false;
      }
      else {
         here = ftell(fd);
         
         nbytes = (here < BUFSIZ)?here:BUFSIZ;
         nbytes = (nbytes > info->mrs)?info->mrs:nbytes;
         
         if (fseek(fd, -nbytes, SEEK_CUR) == -1)
            return ER_ERROR;
            
         /* Remember if we've completely rewound the file */
         if (ftell(fd) == 0L)
            sof = true;
         
         --nbytes;
         nread = fread(buffer,1,(int) nbytes,fd);
         if (nread != (int) nbytes) {
            looking = false;
         }
            
         buff = &buffer[nread-1];
         for (i=0; i<nread; i++) {
            if (*buff-- == NL) {
               if (fseek(fd, -(long) i, SEEK_CUR) == -1)
                  return ER_ERROR;
                  
               looking = false;
               info->lastop = FIOP_NONE;

               return ER_SUCCESS;
            }
         }
      }
   }
   
   info->lastop = FIOP_NONE;
   
   return ER_SUCCESS;
}

int unform_seq_seek(fileid, offset, origin)
long fileid;
long offset;
int origin;
{
   char             buff[4];
   long             len;
   struct FILE_INFO *info = &FileInfo[fileid];
   
   dbgmsg("unform_seq_seek(%ld, %ld, %d)", fileid, offset, origin);

   /* Invalidate current record */
   info->recordsize = -1;

   /* Move to the starting record if necessary */
   if (origin == SEEK_SET) {
      rewind(info->fd);
      info->pasteof = false;
   }
   else if (origin == SEEK_END) {
      if (fseek(info->fd, 0L, SEEK_END) == -1)
         return ER_ERROR;
   }
   
   if (offset > 0) {
      /* moving forward, easy */
      while (offset != 0) {
         if (fread(buff, 4, SEEK_CUR, info->fd) != 1)
            return ER_ERROR;
            
         len = (long) unpack32(buff);
         if (fseek(info->fd, len+4, 1) == -1)
            return ER_ERROR;
            
         offset--;
      }
   }
   else if (offset < 0) {
      if (info->pasteof) {
         info->pasteof = false;
         offset++;
      }
      
      while (offset < 0) {
         if (fseek(info->fd, -4, SEEK_CUR) == -1)
            return ER_ERROR;
            
         if (fread(buff, 4, 1, info->fd) != 1)
            return ER_ERROR;
            
         len = (long) unpack32(buff);
         if (fseek(info->fd, -(len+8), SEEK_CUR) == -1)
            return ER_ERROR;
            
         offset++;
      }
   }
   
   info->lastop = FIOP_NONE;
   
   return ER_SUCCESS;
}
#endif /* VMS */

void SpPutEOF(void)
{
   long              fileid;
   int               res;
   int               forg=0;
   int               count;
   char              buff[4];
   FILE              *fd;
   struct FILE_INFO  *info;
   
   dbgmsg("SP.PUTEOF");
   
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;
   
   fileid = get_32();
   dbgmsg("fileid=%ld", fileid);
   
   info = &FileInfo[fileid];
   
   if (info == NULL) {
      put_8(ER_ERROR);
      put_count(OutCount);
      return;
   }
   
   fd = info->fd;
   fflush(fd);
#ifdef MSDOS
   res = chsize(fileno(fd), ftell(fd));
#endif
#ifdef VMS
   res = sys$truncate(info->rab);
   if (res & 1)
      res = 0;
#endif
#ifdef SUN
   res = ftruncate(fileno(fd), ftell(fd));
#endif
#ifdef SOLARIS
   res = ftruncate(fileno(fd), ftell(fd));
#endif
#ifdef SGI
   res = ftruncate(fileno(fd), ftell(fd));
#endif
   if (res != 0)
      res = ER_ERROR;
   else {
      res = ER_SUCCESS;
      info->pasteof = true;
   }
   
   dbgmsg("EOF pos=%ld, pasteof=%d", ftell(fd), info->pasteof);
      
   put_8(res);
   put_count(OutCount);
}

#ifndef VMS
static char *special_gets(buff, nbytes, info, linelen)
char *buff;
int nbytes;
struct FILE_INFO *info;
int *linelen;
{
   char           *dst = buff;
   char           *src;
   register char  ch;
   int            ind = info->llind;
   int	           len = info->lllen;
   int            bitlen = 0;
#ifdef MSDOS
   bool           gotCR = false;
#endif
   
   src = (char *) info->llbuff;
   
   while (nbytes > 0) {
      /* Do we have valid data in the read-ahead buffer */
      if (ind >= len) {
         dbgmsg("Fill read-ahead buffer");
         if (feof(info->fd)) {
            dbgmsg("EOF on read into read-ahead buffer");
            
            info->llind = ind;
            info->lllen = len;
            
            if (dst == buff)
               return NULL;
               
            *dst = '\0';
            *linelen = bitlen;

            dbgmsg("special_gets: len=%d", bitlen);
            
            return buff;
         }
         
         len = fread(info->llbuff, 1, LL_BUF_SIZE, info->fd);
         
         dbgmsg("read %d bytes into read-ahead buffer", len);
         ind = 0;
      }
      
      ch = src[ind++];
      *dst++ = ch;
      bitlen++;
#ifdef MSDOS
      if (gotCR && (ch == NL))
         break;
         
      gotCR = (ch == CR) ? true : false;
#else
      if (ch == NL)
         break;
#endif
      nbytes--;
   }
   
#ifdef MSDOS
   /* Under MS-DOS, the CR,LF pair should be turned into a single NL */
   dst--;
   dst--;
   bitlen--;
   *dst++ = NL;
#endif
   *dst = '\0';
   
   info->llind = ind;
   info->lllen = len;
   
   *linelen = bitlen;
   
   return buff;
}
#endif /* VMS */
