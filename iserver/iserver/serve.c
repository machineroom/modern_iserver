/* Copyright INMOS Limited 1988,1990 */

/* CMSIDENTIFIER */
static char *CMS_Id = "PRODUCT:ITEM.VARIANT-TYPE;0(DATE)";

#include <stdio.h>
#include "server.h"
#include "iserver.h"
#include "misc.h"
#include "ttyio.h"
#include "serve.h"
#include "linkops.h"
#include "opserror.h"
#include "hbeat.h"

int (*HeartBeat)();

/*
 * Serve  -  the main loop (read buffer from link, call a function, write
 * buffer to link)
 */

int      Serve()
{
   bool            terminate_flag = FALSE;
   int             result;
   unsigned char   ops_res;

   HeartBeat = NULL;
   ops_res = OPS_CommsAsynchronous(ConnId);
   
   if (ops_res != STATUS_NOERROR) {
      sprintf(ErrMsg,"linkops error: %s\n", lnkops_errorstring);
      close_server(MISC_EXIT, ErrMsg);
   }
   
   while (TRUE) {
      dbgmsg("-=-");
      ops_res = OPS_GetRequest(ConnId, Tbuf, ONESECOND/6, HeartBeat);
      
      if (ops_res == STATUS_TARGET_ERROR)
         close_server(ERROR_FLAG_EXIT,"Error flag raised by transputer\n");
      else if (ops_res != STATUS_NOERROR) {
         sprintf(ErrMsg,"protocol error: %s\n", lnkops_errorstring);
         close_server(MISC_EXIT, ErrMsg);
      }

      switch (Tbuf[2]) {
      case SP_OPEN:
         SpOpen();
         break;
      case SP_CLOSE:
         SpClose();
         break;
      case SP_READ:
         SpRead();
         break;
      case SP_WRITE:
         SpWrite();
         break;
      case SP_GETBLOCK:
         SpGetBlock();
         break;
      case SP_PUTBLOCK:
         SpPutBlock();
         break;
      case SP_GETS:
         SpGets();
         break;
      case SP_PUTS:
         SpPuts();
         break;
      case SP_FLUSH:
         SpFlush();
         break;
      case SP_SEEK:
         SpSeek();
         break;
      case SP_TELL:
         SpTell();
         break;
      case SP_EOF:
         SpEof();
         break;
      case SP_FERROR:
         SpError();
         break;
      case SP_REMOVE:
         SpRemove();
         break;
      case SP_RENAME:
         SpRename();
         break;
      case SP_GETKEY:
         SpGetkey();
         break;
      case SP_POLLKEY:
         SpPollkey();
         break;
      case SP_GETENV:
         SpGetenv();
         break;
      case SP_TIME:
         SpTime();
         break;
      case SP_SYSTEM:
         SpSystem();
         break;
      case SP_EXIT:
         terminate_flag = TRUE;
         result = SpExit();
         TestErrorSwitch = FALSE;
         break;
      case SP_COMMAND:
         SpCommand();
         break;
      case SP_CORE:
         SpCore();
         break;
      case SP_ID:
         SpId();
         break;
      case SP_GETINFO:
         SpGetInfo();
         break;
      case SP_ISATTY:
         SpIsaTTY();
         break;
      case SP_FILEEXISTS:
         SpExists();
         break;
      case SP_TRANSLATE:
         SpTranslate();
         break;
      case SP_FERRSTAT:
         SpErrStat();
         break;
      case SP_COMMANDARG:
         SpCmdArg();
         break;
      case SP_OPENREC:
         SpOpenRec();
         break;
      case SP_GETREC:
         SpGetRec();
         break;
      case SP_PUTREC:
         SpPutRec();
         break;
#ifndef MSDOS
      case SP_PARMACS  : SpParmacs();  break; /* RCW parmacs addition */
#endif
      case SP_PUTEOF:
         SpPutEOF();
         break;
#ifdef MSDOS
      case SP_MSDOS:
         SpMsdos();
         break;
#endif
      default:
         SpUnknown();
         break;
      }

      dbgmsg("%d:%d", Tbuf[0]+(Tbuf[1]*256), Tbuf[2]);
      ops_res = OPS_SendReply(ConnId, Tbuf);
      if (ops_res == STATUS_TARGET_ERROR)
         close_server(ERROR_FLAG_EXIT,"Error flag raised by transputer\n");
      else if (ops_res != STATUS_NOERROR) {
         sprintf(ErrMsg, "Failed to send response because:\n   %s\n",
                           lnkops_errorstring);
         close_server(MISC_EXIT, ErrMsg);
      }

      if (terminate_flag == TRUE)
         return (result);
   }
}
