/* Copyright INMOS Limited 1988,1990 */

/* CMSIDENTIFIER */
static char *CMS_Id = "PRODUCT:ITEM.VARIANT-TYPE;0(DATE)";

#include <stdio.h>
#include "server.h"
#include "iserver.h"
#include "misc.h"
#include "boot.h"
#include "linkops.h"
#include "opserror.h"

/*
 * Boot  -  copy the boot file to the link
 */

void     Boot()
{
   FILE            *fd;
   char            buffer[BOOT_BUFFER_LENGTH];
   int             length = 0;
   long            size = 0;
   static char     Flicks[] = "|/-\\|/-\\";
   int             flick = 0;
   unsigned char   ops_res;

   /* Open the boot file */
   fd = fopen(BootFileName, "rb");
   if (fd == NULL) {
      sprintf(ErrMsg,"Boot: cannot find file \"%s\"", BootFileName);
      close_server(MISC_EXIT, ErrMsg);
   }

   infomsg("Booting root transputer...");

   /* Read the file in large chunks */
   while ((length = fread(buffer, 1, BOOT_BUFFER_LENGTH, fd)) > 0) {
      dbgmsg("%d", length);

      ops_res = OPS_BootWrite(ConnId, buffer, (unsigned long) length);

      if (ops_res != STATUS_NOERROR) {
         sprintf(ErrMsg,"Boot: write failed after %d bytes because:\n   %s\n",
                           size, lnkops_errorstring);
         close_server(MISC_EXIT, ErrMsg);
      }
                           
      size += length;
      
      if (isatty(1)) {
         infomsg("%c\b", Flicks[flick]);
         if (++flick == 8)       
            flick = 0;
      }
   }

   if (isatty(1))
      infomsg("\r                           \r");
   else
      infomsg("ok\n");
   
   fclose(fd);
   
   dbgmsg("booted %ld bytes", size);
}

