/* Copyright INMOS Limited 1988,1990 */

/* CMSIDENTIFIER */
static char *CMS_Id = "PRODUCT:ITEM.VARIANT-TYPE;0(DATE)";

#ifdef MSDOS
#include <stdlib.h>
#include <dos.h>

#include "server.h"
#include "iserver.h"
#include "pack.h"

#ifdef ICB
#include <conio.h>

#define huge
#endif

static void DosUnknown(tag)
int tag;
{
   int   PacketLength;

   infomsg("Encountered unknown msdos command tag (%d)]\n", tag);

   put_8(ER_UNIMPLEMENTED);
}

static void DosSendBlock()
{
   register char huge      *ptr;
   unsigned int            length;
   register unsigned int   i;

   ptr = (char huge *) get_32();
   length = get_16();

   dbgmsg("DOS.SENDBLOCK: %d bytes at %lp", length, ptr);

   for (i = 0; i < length; i++)
      *ptr++ = (char) get_8();

   put_8(ER_SUCCESS);
   put_16(length);
}

static void DosReceiveBlock()
{
   register char huge      *ptr;
   unsigned int            length; 
   unsigned register int   i;

   dbgmsg("DOS.RECEIVEBLOCK");
   ptr = (char huge *) get_32();
   length = get_16();

   put_8(ER_SUCCESS);
   put_16(length);

   for (i = 0; i < length; i++)
      put_8(*ptr++);
}

static void DosCallInterrupt()
{
   int             intnum;
   int             i;
   struct SREGS    _segregs, *segregs = &_segregs;
   union REGS      _inregs, *inregs = &_inregs, _outregs, *outregs = &_outregs;


   dbgmsg("DOS.CALLINTERRUPT");
   intnum = get_16();

   inregs->x.ax = (int)(get_32() & 0xffffl);
   inregs->x.bx = (int)(get_32() & 0xffffl);
   inregs->x.cx = (int)(get_32() & 0xffffl);
   inregs->x.dx = (int)(get_32() & 0xffffl);
   inregs->x.di = (int)(get_32() & 0xffffl);
   inregs->x.si = (int)(get_32() & 0xffffl);
   segregs->cs = (int)(get_32() & 0xffffl);
   segregs->ds = (int)(get_32() & 0xffffl);
   segregs->es = (int)(get_32() & 0xffffl);
   segregs->ss = (int)(get_32() & 0xffffl);

   int86x(intnum, inregs, outregs, segregs);

   put_8(ER_SUCCESS);

   put_8(outregs->x.cflag);

   put_32((unsigned long) outregs->x.ax);
   put_32((unsigned long) outregs->x.bx);
   put_32((unsigned long) outregs->x.cx);
   put_32((unsigned long) outregs->x.dx);
   put_32((unsigned long) outregs->x.di);
   put_32((unsigned long) outregs->x.si);
   put_32((unsigned long) segregs->cs);
   put_32((unsigned long) segregs->ds);
   put_32((unsigned long) segregs->es);
   put_32((unsigned long) segregs->ss);
}

static void DosReadRegisters()
{
   struct SREGS    _segregs, *segregs = &_segregs;

   dbgmsg("DOS.READREGISTERS");
   segread(segregs);

   put_8(ER_SUCCESS);

   put_32((unsigned long) segregs->cs);
   put_32((unsigned long) segregs->ds);
   put_32((unsigned long) segregs->es);
   put_32((unsigned long) segregs->ss);
}

static void DosPortWrite()
{
   int            port;
   unsigned char  value;

   dbgmsg("DOS.PORTWRITE");

   port = get_16();
   value = get_8();
   
   outp(port, value);
   
   put_8(ER_SUCCESS);
}

static void     DosPortRead()
{
   int            port;
   unsigned char  value;

   dbgmsg("DOS.PORTREAD");
   
   port = get_16();
   value = inp(port);
   
   put_8(ER_SUCCESS);
}

void SpMsdos()
{
   int   subfunc;
   
   dbgmsg("SP.MSDOS");
   InBuf = &Tbuf[3];
   OutBuf = &Tbuf[2];
   OutCount = 0;
   
   subfunc = get_8();

   switch (subfunc) {
      case 0:
         DosSendBlock();
         break;

      case 1:
         DosReceiveBlock();
         break;

      case 2:
         DosCallInterrupt();
         break;

      case 3:
         DosReadRegisters();
         break;

      case 4:
         DosPortWrite();
         break;

      case 5:
         DosPortRead();
         break;

      default:
         DosUnknown(subfunc);
         break;
   }

   put_count(OutCount);
}

#endif /* MSDOS */
