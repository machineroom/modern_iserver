/* Copyright INMOS Limited 1988,1990 */

/* CMSIDENTIFIER */
static char *CMS_Id = "PRODUCT:ITEM.VARIANT-TYPE;0(DATE)";

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include "server.h"
#include "files.h"
#include "misc.h"

struct FILE_INFO FileInfo[MAX_FILES];
char *OpenModes[N_ORGS][6]={
   {BINARY_1,BINARY_2,BINARY_3,BINARY_4,BINARY_5,BINARY_6}, /* Binary */
   {TEXT_1,TEXT_2,TEXT_3,TEXT_4,TEXT_5,TEXT_6},             /* Text */
   {BINARY_1,BINARY_2,BINARY_3,BINARY_4,BINARY_5,BINARY_6}, /* Variable */
   {BINARY_1,BINARY_2,BINARY_3,BINARY_4,BINARY_5,BINARY_6}  /* Fixed */ 
};

void CloseOpenFiles(void)
{
   int               i;
   int               res;
   struct FILE_INFO  *info;
   
   for (i=0; i<MAX_FILES; i++)
      if (FileInfo[i].fd != NO_FILE) {
         if (FileInfo[i].dirty == true) {
            info = &FileInfo[i];
#ifdef VMS
            res = vmsput(info->rab, info->buff, (int)info->recordsize);
#else
            res = info->putfn(info->fd, info->buff, info->recordsize);
#endif
         }
         
         fclose(FileInfo[i].fd);
         
         if (FileInfo[i].buff != NULL)
            free(FileInfo[i].buff);
            
#ifndef VMS
         if (FileInfo[i].llbuff != NULL)
            free(FileInfo[i].llbuff);
#endif
         
         FileInfo[i].fd = NO_FILE;
      }
}

int OpenFiles(in, out, err)
char *in;
char *out;
char *err;
{
   int   i;
   FILE  *Inf,*Outf,*Errf;
   
   (void) dbgmsg("opening predefined streams");
   
   if ((Inf=freopen(in,"r",stdin)) == NULL)
      return NO_STDIN;
   
   if ((Outf=freopen(out,"w",stdout)) == NULL)
      return NO_STDOUT;
   
   if ((Errf=freopen(err,"w",stderr)) == NULL)
      return NO_STDERR;
      
   FileInfo[FD_STDIN].fd = Inf;   
   FileInfo[FD_STDOUT].fd = Outf;
   FileInfo[FD_STDERR].fd = Errf;
   
   for (i=FD_STDIN; i<=FD_STDERR; i++) {
      FileInfo[i].type  = ORG_TEXT;
      FileInfo[i].dirty = false;
      FileInfo[i].mrs   = 0;
      FileInfo[i].buff  = (unsigned char *) NULL;
      
      if (isatty(fileno(FileInfo[i].fd)))
         setbuf(FileInfo[i].fd, NULL);
   }
   
   for (i=FD_STDERR+1; i<MAX_FILES; i++)
      FileInfo[i].fd = NO_FILE;
   
   dbgmsg("streams opened OK");
      
   return OPEN_OK;
}

void DupStdStreams(void)
{
   int   i;
   
   FileInfo[FD_STDIN].fd = stdin;
   FileInfo[FD_STDOUT].fd = stdout;
   FileInfo[FD_STDERR].fd = stderr;
   
   for (i=FD_STDIN; i<=FD_STDERR; i++) {
      FileInfo[i].type  = ORG_TEXT;
      FileInfo[i].dirty = false;
      FileInfo[i].mrs   = 0;
      FileInfo[i].buff  = (unsigned char *) NULL;
      
#ifndef ICB
      if (isatty(fileno(FileInfo[i].fd)))
         setbuf(FileInfo[i].fd, NULL);
#endif
   }
      
   for (i=FD_STDERR+1; i<MAX_FILES; i++)
      FileInfo[i].fd = NO_FILE;
   
   dbgmsg("streams setup OK");
}

long FindFreeSlot(void)
{
   register int i;
   
   for (i=0; i<MAX_FILES; i++)
      if (FileInfo[i].fd == NO_FILE)
         return (long) i;
         
   return NO_SLOT;
}

long RememberFile(fd, org)
FILE *fd;
int org;
{
   long  slot=FindFreeSlot();
   
   if (slot != NO_SLOT) {
      FileInfo[slot].fd = fd;
      FileInfo[slot].org = org;
      FileInfo[slot].dirty = false;
      FileInfo[slot].mrs   = 0;
      FileInfo[slot].buff  = (unsigned char *) NULL;
   }
   
   return slot;
}

int ForgetFile(fileid)
long fileid;
{
   int               res=EOF;
   struct FILE_INFO  *info;
   
   dbgmsg("ForgetFile(%ld)", fileid);
   
   info = &FileInfo[fileid];
   
   if (info->fd != NO_FILE) {
      if (info->dirty == true) {
#ifdef VMS
         res = vmsput(info->rab, info->buff, (int)info->recordsize);
#else
         res = info->putfn(info->fd, info->buff, info->recordsize);
#endif
      }
      dbgmsg("Closing stream 0x%lx", (long) info->fd);
      res = fclose(info->fd);                       
   }
      
   info->fd = NO_FILE;     
   
   return res;
}

FILE *FindFileDes(fileid,org)
long fileid;
int *org;
{
   if ((fileid < 0) || (fileid >= MAX_FILES))
      return NO_FILE;
      
   *org = FileInfo[fileid].org;
      
   return FileInfo[fileid].fd;
}

