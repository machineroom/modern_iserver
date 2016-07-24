/* Copyright INMOS Limited 1988,1990 */

/* CMSIDENTIFIER */
static char *CMS_Id = "PRODUCT:ITEM.VARIANT-TYPE;0(DATE)";

#include <stdio.h>
#include <string.h>

#include "server.h"
#include "iserver.h"
#include "pack.h"
#include "misc.h"

/*
 * SpCommand
 */

void            SpCommand()
{
   bool            All;
   int             Size;
   char           *Cl;
   dbgmsg("SP.COMMAND");
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;
   All = get_8()?TRUE:FALSE;
   dbgmsg("%d", (int) All);

   if (All == TRUE)
      Cl = RealCommandLine;
   else
      Cl = DoctoredCommandLine;
      
   dbgmsg("command line: %s\n", Cl);

   put_8(ER_SUCCESS);
   Size = strlen(Cl);
   put_slice(Size, Cl);
   put_count(OutCount);
}

/*
 * SpCore
 */

void            SpCore()
{
   long            Offset;
   int             Off, Length;

   dbgmsg("SP.CORE");
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;

   if (AnalyseSwitch == 0) {
      dbgmsg("not peeked");
      put_8(ER_ERROR);
      put_count(OutCount);
   }
   else {
      Offset = get_32();
      Off = (int) Offset;
      dbgmsg("offset %ld", Offset);

      Length = get_16();
      dbgmsg("length %d", Length);

      if ((Off >= CoreSize) || (Off < 0) || (Length < 0)) {
         put_8(ER_ERROR);
         put_count(OutCount);
         return;
      }

      if (Length + Off > CoreSize) {
         Length = CoreSize - Off;
         put_8(ER_SUCCESS);
         put_slice(Length, &CoreDump[Offset]);
         put_count(OutCount);
      }
      else {
         put_8(ER_SUCCESS);
         put_slice(Length, &CoreDump[Offset]);
         put_count(OutCount);
      }
   }
}

/*
 * SpId
 */

void            SpId()
{
   char            Version, Host, OS, Board;

   dbgmsg("SP.ID");
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;

   Version = VERSION_ID;
   Host = HOST_ID;
   OS = OS_ID;
   Board = BOARD_ID;
   
   dbgmsg("version=%d, host=%d, os=%d, board=%d", Version, Host, OS, Board);

   put_8(ER_SUCCESS);
   put_8(Version);
   put_8(Host);
   put_8(OS);
   put_8(Board);
   put_count(OutCount);
}




void            SpUnknown()
{
   infomsg("[Encountered unknown primary tag (%d)]\n", Tbuf[2]);
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;
   put_8(ER_UNIMPLEMENTED);
   put_count(OutCount);
}



/*
 * Eof
 */
