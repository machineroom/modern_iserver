/* Copyright INMOS Limited 1988,1990 */

/* CMSIDENTIFIER */
static char *CMS_Id = "PRODUCT:ITEM.VARIANT-TYPE;0(DATE)";

#include "server.h"
#include "iserver.h"
#include "pack.h"
#include "info.h"

SpGetInfo()
{
   unsigned char  item;
   int            replymax;
   char           name[MAX_SLICE_LENGTH];
   
   dbgmsg("SP.GETINFO");
   
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;
   
   item = get_8();
   replymax = get_16();
   
   switch (item) {
      case INFO_SWITCH:
         put_8(ER_SUCCESS);
         put_8(SWITCH_CHAR);
         break;
         
      case INFO_EOL:
         put_8(ER_SUCCESS);
         put_slice(strlen(NEWLINE_SEQUENCE), NEWLINE_SEQUENCE);
         break;
         
      case INFO_STDERR:
         put_8(ER_SUCCESS);
         put_8(REDIRECT_STDERR);
         break;
         
      case INFO_SERVERID:
         put_8(ER_SUCCESS);
         sprintf(name,"%s %s", PROGRAM_NAME, VERSION_NAME);
         put_slice(strlen(name), name);
         break;
         
      case INFO_SERVERMAJ:
         put_8(ER_SUCCESS);
         put_32((long) MAJOR_ID);
         break;
         
      case INFO_SERVERMIN:
         put_8(ER_SUCCESS);
         put_32((long) MINOR_ID);
         break;
         
      case INFO_PKTSIZE:
         put_8(ER_SUCCESS);
         put_32((long) TRANSACTION_BUFFER_SIZE);
         break;
         
      default:
         put_8(ER_ERROR);
         break;
   }
   
   put_count(OutCount);
}
