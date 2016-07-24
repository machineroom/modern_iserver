/* Copyright INMOS Limited 1991 */

#ifdef VMS
/* CMSIDENTIFIER */
static char *CMS_Id = "PRODUCT:ITEM.VARIANT-TYPE;0(DATE)";

#include <string.h>
#include <stdio.h>
#include <descrip.h>
#include <ssdef.h>
#include <rms.h>
#include <rmsdef.h>
#include "server.h"
#include "files.h"
#include "vaxrms.h"

static char *strdup(str)
char *str;
{
   char  *dst;
   
   dst = malloc(strlen(str)+1);
   if (dst == NULL)
      return NULL;
      
   strcpy(dst, str);
   
   return dst;
}

static struct FAB *newfab()
{
   struct FAB *new = malloc(sizeof(struct FAB));
   
   if (new != NULL) {
      memset(new, sizeof(struct FAB), 0);
      new->fab$b_bid = FAB$C_BID;
      new->fab$b_bln = FAB$C_BLN;
   }
         
   return new;
}

static struct RAB *newrab()
{
   struct RAB *new = malloc(sizeof(struct RAB));
   
   if (new != NULL) {
      memset(new, sizeof(struct RAB), 0);
      new->rab$b_bid = RAB$C_BID;
      new->rab$b_bln = RAB$C_BLN;
   }
         
   return new;
}

RecInfo *newinfo()
{
   RecInfo *new = malloc(sizeof(RecInfo));

   if (new != (RecInfo *) NULL) {
      new->baserec = 0;
      new->prev = (RecInfo *) NULL;
      new->next = (RecInfo *) NULL;
   }
   
   return new;
}

static void freefab(fab)
struct FAB *fab;
{
   if (fab->fab$l_fna != 0)
      free(fab->fab$l_fna);
      
   free(fab);
}

void freerab(rab)
struct RAB *rab;
{
   if (rab->rab$l_fab != 0)
      freefab(rab->rab$l_fab);
   free(rab);
}

struct RAB *vmscreate(fname, rfm, rat, mrs)
char *fname;
unsigned int rfm;
unsigned int rat;
unsigned int mrs;
{
   unsigned long  status;
   struct FAB     *fab;
   struct RAB     *rab;
   
   fab = newfab();
   if (fab == (struct FAB *) NULL)
      return (struct RAB *) NULL;
      
   rab = newrab();
   if (rab == (struct RAB *) NULL) {
      freefab(fab);
      return (struct RAB *) NULL;
   }
      
   /* Fill in the fields of the fab */
   fab->fab$b_fac = FAB$M_GET|FAB$M_PUT|FAB$M_BRO;
   fab->fab$l_fna = strdup(fname);
   fab->fab$b_fns = strlen(fname);
   fab->fab$l_fop = FAB$M_CBT|FAB$M_SUP;
   fab->fab$w_mrs = mrs;
   fab->fab$b_org = FAB$C_SEQ;
   fab->fab$b_rat = rat;
   fab->fab$b_rfm = rfm;
   
   status = sys$create(fab, 0, 0);
   if (!(status & 1)) {
      freerab(rab);
      return (struct FAB *) NULL;
   }
   
   /* What about the RAB then */
   rab->rab$l_fab = fab;
   status = sys$connect(rab, 0, 0);
   if (!(status & 1)) {
      freerab(rab);
      return (struct FAB *) NULL;
   }
   
   return rab;
}

struct RAB *vmsopen(fname, rfm, rat)
char *fname;
unsigned int rfm;
unsigned int rat;
{
   unsigned long  status;
   struct FAB     *fab;
   struct RAB     *rab;
   
   fab = newfab();
   if (fab == (struct FAB *) NULL)
      return (struct RAB *) NULL;
      
   rab = newrab();
   if (rab == (struct RAB *) NULL) {
      freefab(fab);
      return (struct RAB *) NULL;
   }
      
   /* Fill in the fields of the fab */
   fab->fab$b_fac = FAB$M_GET|FAB$M_PUT|FAB$M_BRO;
   fab->fab$l_fna = strdup(fname);
   fab->fab$b_fns = strlen(fname);
   fab->fab$b_org = FAB$C_SEQ;
   fab->fab$b_rat = rat;
   fab->fab$b_rfm = rfm;
   
   status = sys$open(fab, 0, 0);
   if (!(status & 1)) {
      freerab(rab);
      return (struct FAB *) NULL;
   }
   
   /* What about the RAB then */
   rab->rab$l_fab = fab;
   status = sys$connect(rab, 0, 0);
   if (!(status & 1)) {
      freerab(rab);
      return (struct FAB *) NULL;
   }
   
   return rab;
}

int vmsclose(rab)
struct RAB *rab;
{
   unsigned long  status;
   struct FAB     *fab = rab->rab$l_fab;
   
   status = sys$close(fab, 0, 0);
   if (!(status & 1)) {
      return ER_ERROR;
   }
   
   freerab(rab);
      
   return ER_SUCCESS;
}

int vmsput(rab, buff, len)
struct RAB *rab;
char *buff;
int len;
{
   unsigned long  status;
   
   rab->rab$l_rbf = buff;
   rab->rab$w_rsz = len;
   
   status = sys$put(rab, 0, 0);
   if (!(status & 1)) {
      return ER_ERROR;
   }
   
   return ER_SUCCESS;
}

int vmsget(rab, buff, len)
struct RAB *rab;
char *buff;
int len;
{
   unsigned long  status;
   
   rab->rab$l_ubf = buff;
   rab->rab$w_usz = len;
   
   status = sys$get(rab, 0, 0);
   if (!(status & 1)) {
      return ER_ERROR;
   }
   
   return ER_SUCCESS;
}

static int vmsrewind(rab)
struct RAB *rab;
{
   unsigned long  status;
   
   status = sys$rewind(rab, 0, 0);
   if (!(status & 1)) {
      return ER_ERROR;
   }
   
   return ER_SUCCESS;
}

int vmsseek(fileid, offset, origin)
int fileid;
long offset;
int origin;
{
   RecInfo           *rinfo;
   struct FILE_INFO  *info = &FileInfo[fileid];
   long              ind;
   
   /* Only allowed to backspace a single record */
   
   if ((origin == SEEK_SET) && (offset == 0))
      return vmsrewind(FileInfo[fileid].rab);
      
   if (origin != SEEK_CUR)
      return ER_ERROR;
      
   if (offset != -1)
      return ER_ERROR;
      
   rinfo = info->poslist;
   if (info->recno == rinfo->baserec)
      rinfo = rinfo->prev;
      
   info->recno--;
   ind = info->recno - rinfo->baserec;
   
   memcpy(info->rab->rab$w_rfa, &rinfo->pos[ind], sizeof(FilePos));
      
   return ER_SUCCESS;
}

int vmssavepos(info)
struct FILE_INFO *info;
{
   register long  recno = info->recno;
   register long  base;
   RecInfo  *rinfo = info->poslist;
   RecInfo  *new;
   
   if (recno >= (rinfo->baserec + POSITIONS_PER_BLOCK)) {
      /* Build a new entry and chain it in */
      base += POSITIONS_PER_BLOCK;
      
      if (rinfo->next == (RecInfo *) NULL) {
         new = newinfo();
         if (new == (RecInfo *) NULL)
            return ER_ERROR;
         
         new->prev = rinfo;
         rinfo->next = new;
         new->baserec = base;
      
         rinfo = new;
      }
      else
         rinfo = rinfo->next;
         
      info->poslist = rinfo;
   }

   memcpy(&rinfo->pos[recno-base], info->rab->rab$w_rfa, sizeof(FilePos));
   
   return ER_SUCCESS;
}

#ifdef STANDALONE_TEST
main()
{
   struct RAB *rab;
   
   rab = docreate("form_seq.out", FAB$C_VAR, FAB$M_FTN, 0);
   doput(rab, "hello", 5);
   doclose(rab);

   rab = docreate("form_dir.out", FAB$C_FIX, FAB$M_FTN, 20);
   doput(rab, "hello", 20);
   doclose(rab);

   rab = docreate("unform_seq.out", FAB$C_VAR, 0, 0);
   doput(rab, "hello", 5);
   doclose(rab);

   rab = docreate("unform_dir.out", FAB$C_FIX, 0, 20);
   doput(rab, "hello", 20);
   doclose(rab);
}
#endif /* STANDALONE_TEST */

#endif /* VMS */
