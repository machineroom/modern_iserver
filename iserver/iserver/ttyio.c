/* Copyright INMOS Limited 1988,1990 */

/* CMSIDENTIFIER */
static char *CMS_Id = "PRODUCT:ITEM.VARIANT-TYPE;0(DATE)";

#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef SUN
#include <fcntl.h>
#include <sys/termios.h>
#include <sys/filio.h>
#endif

#ifdef SOLARIS
#include <fcntl.h>
#include <sys/termios.h>
#include <sys/filio.h>
#endif

#ifdef SGI
#include <fcntl.h>
#include <termios.h>
#endif

#ifdef VMS
#include <ssdef.h>
#include <iodef.h>
#include <descrip.h>
#include <ssdef.h>
#include <psldef.h>
#endif

#ifdef HELIOS
#include <nonansi.h>
#include <stdlib.h>
#include <attrib.h>
#endif

#include "server.h"
#include "iserver.h"
#include "pack.h"
#include "misc.h"
#include "ttyio.h"

#define ORG_MODE     0
#define GET_MODE     1
#define POLL_MODE    2

static bool     HostBeginCalled = false;

static int      TermMode = ORG_MODE;
#ifdef SUN
static struct termios OrgMode, CurMode;
static int            TtyDes;
#endif

#ifdef SOLARIS
static struct termios OrgMode, CurMode;
static int            TtyDes;
#endif

#ifdef SGI
static struct termios OrgMode, CurMode;
static int            TtyDes;
#endif

#ifdef HELIOS
static Attributes CurAttributes, OrgAttributes;
static Stream  *InputStream;
#endif

#ifdef VMS
static short int InputChan;     /* declare channel  */
$DESCRIPTOR(InputDescriptor, "TT:");    /* and descriptor for input  */

static struct IOSB_DESC {
   short int       Status;
   short int       Count;
   int             DeviceInfo;
}               iosb_desc;
#endif

void            HostEnd(void)
{
   if (HostBeginCalled == true) {
#ifdef SUN
      dbgmsg("HostEnd()");
      ioctl(TtyDes, TCSETS, &OrgMode);
      close(TtyDes);
      TtyDes = -1;
#endif
#ifdef SOLARIS
      dbgmsg("HostEnd()");
      ioctl(TtyDes, TCSETS, &OrgMode);
      close(TtyDes);
      TtyDes = -1;
#endif
#ifdef SGI
      dbgmsg("HostEnd()");
      ioctl(TtyDes, TCSETA, &OrgMode);
      close(TtyDes);
      TtyDes = -1;
#endif
#ifdef    HELIOS
      SetAttributes(InputStream, &CurAttributes);
#endif

      HostBeginCalled = false;
   }
}

void            ResetTerminal(void)
{
   if (HostBeginCalled == true) {
#ifdef SUN
      dbgmsg("ResetTerminal()");
      if (TermMode != ORG_MODE) {
         ioctl(TtyDes, TCSETS, &OrgMode);
         TermMode = ORG_MODE;
      }
#endif
#ifdef SOLARIS
      dbgmsg("ResetTerminal()");
      if (TermMode != ORG_MODE) {
         ioctl(TtyDes, TCSETS, &OrgMode);
         TermMode = ORG_MODE;
      }
#endif
#ifdef SGI
      dbgmsg("ResetTerminal()");
      if (TermMode != ORG_MODE) {
         ioctl(TtyDes, TCSETA, &OrgMode);
         TermMode = ORG_MODE;
      }
#endif
#ifdef HELIOS
      if (TermMode != ORG_MODE) {
         SetAttributes(InputStream, &OrgAttributes);
         TermMode = ORG_MODE;
      }
#endif
   }
}

void            HostBegin(void)
{
   if (HostBeginCalled == true)
      return;
      
   HostBeginCalled = true;

#ifdef SUN
   TtyDes = open("/dev/tty", O_RDWR);
   if (TtyDes == -1)
      close_server(MISC_EXIT, "Failed to open /dev/tty");
      
   ioctl(TtyDes, TCGETS, &OrgMode);
   ioctl(TtyDes, TCGETS, &CurMode);
#endif
#ifdef SOLARIS
   TtyDes = open("/dev/tty", O_RDWR);
   if (TtyDes == -1)
      close_server(MISC_EXIT, "Failed to open /dev/tty");
      
   ioctl(TtyDes, TCGETS, &OrgMode);
   ioctl(TtyDes, TCGETS, &CurMode);
#endif
#ifdef SGI
   TtyDes = open("/dev/tty", O_RDWR);
   if (TtyDes == -1)
      close_server(MISC_EXIT, "Failed to open /dev/tty");
      
   ioctl(TtyDes, TCGETA, &OrgMode);
   ioctl(TtyDes, TCGETA, &CurMode);
#endif
#ifdef HELIOS
   InputStream = fdstream(0);
   GetAttributes(InputStream, &OrgAttributes);
   GetAttributes(InputStream, &CurAttributes);
#endif
#ifdef VMS
   if (sys$assign(&InputDescriptor, &InputChan, PSL$C_USER, 0) != SS$_NORMAL) {
      close_server(MISC_EXIT, "Cannot assign TT:");
   }
#endif

}

/*
 * GetAKey
 */

char            GetAKey(void)
{
   char            c;
   
   if (HostBeginCalled == false)
      HostBegin();
      
#ifdef SUN
   if (TermMode == ORG_MODE) {
      CurMode.c_iflag &= ~ICRNL;
      CurMode.c_lflag &= ~(ICANON | ECHO);
      CurMode.c_cc[VTIME] = 0;
      CurMode.c_cc[VMIN] = 1;
      ioctl(TtyDes, TCSETS, &CurMode);
      TermMode = GET_MODE;
   }
   else if (TermMode == POLL_MODE) {
      CurMode.c_cc[VTIME] = 0;
      CurMode.c_cc[VMIN] = 1;
      ioctl(TtyDes, TCSETS, &CurMode);
      TermMode = GET_MODE;
   }
   (void) read(TtyDes, &c, 1);
#endif
#ifdef SOLARIS
   if (TermMode == ORG_MODE) {
      CurMode.c_iflag &= ~ICRNL;
      CurMode.c_lflag &= ~(ICANON | ECHO);
      CurMode.c_cc[VTIME] = 0;
      CurMode.c_cc[VMIN] = 1;
      ioctl(TtyDes, TCSETS, &CurMode);
      TermMode = GET_MODE;
   }
   else if (TermMode == POLL_MODE) {
      CurMode.c_cc[VTIME] = 0;
      CurMode.c_cc[VMIN] = 1;
      ioctl(TtyDes, TCSETS, &CurMode);
      TermMode = GET_MODE;
   }
   (void) read(TtyDes, &c, 1);
#endif
#ifdef SGI
   if (TermMode == ORG_MODE) {
      CurMode.c_iflag &= ~ICRNL;
      CurMode.c_lflag &= ~(ICANON | ECHO);
      CurMode.c_cc[VTIME] = 0;
      CurMode.c_cc[VMIN] = 1;
      ioctl(TtyDes, TCSETA, &CurMode);
      TermMode = GET_MODE;
   }
   else if (TermMode == POLL_MODE) {
      CurMode.c_cc[VTIME] = 0;
      CurMode.c_cc[VMIN] = 1;
      ioctl(TtyDes, TCSETA, &CurMode);
      TermMode = GET_MODE;
   }
   (void) read(TtyDes, &c, 1);
#endif
#ifdef MSDOS
   c = getch();
#endif
#ifdef HELIOS
   if (TermMode == ORG_MODE) {
      RemoveAttribute(&CurAttributes, ConsoleEcho);
      AddAttribute(&CurAttributes, ConsoleRawInput);
      SetAttributes(InputStream, &CurAttributes);
      TermMode = GET_MODE;
   }
   (void) Read(InputStream, &c, 1, -1);
#endif
#ifdef VMS
   (void) SYS$QIOW(0, InputChan, (IO$_READVBLK | IO$M_NOECHO | IO$M_NOFILTR), &iosb_desc, 0, 0, &c, 1, 0, 0L, 0, 0);
#endif

   return (c);
}

/*
 * PollKey()
 */

int             PollKey(void)
{
   int             res=false;
#ifdef SUN
   long            nchars;
#endif
#ifdef SOLARIS
   long            nchars;
#endif
#ifdef SGI
   long            nchars;
#endif
#ifdef VMS
   struct CHARACTERISTICS {
      short           Count;
      char            ch;
      char            res1;
      long            res2;
   }               Chars;
#endif
   
   if (HostBeginCalled == false)
      HostBegin();

#ifdef MSDOS
   if (kbhit())
      res = true;
#endif

#ifdef SUN
   if (TermMode == ORG_MODE) {
      CurMode.c_iflag &= ~ICRNL;
      CurMode.c_lflag &= ~(ICANON | ECHO);
      CurMode.c_cc[VTIME] = 0;
      CurMode.c_cc[VMIN] = 1;
      ioctl(TtyDes, TCSETS, &CurMode);
      TermMode = GET_MODE;
   }
   else if (TermMode == POLL_MODE) {
      CurMode.c_cc[VTIME] = 0;
      CurMode.c_cc[VMIN] = 1;
      ioctl(TtyDes, TCSETS, &CurMode);
      TermMode = GET_MODE;
   }
   ioctl(TtyDes, FIONREAD, &nchars);
   if (nchars > 0)
      res = true;
#endif

#ifdef SOLARIS
   if (TermMode == ORG_MODE) {
      CurMode.c_iflag &= ~ICRNL;
      CurMode.c_lflag &= ~(ICANON | ECHO);
      CurMode.c_cc[VTIME] = 0;
      CurMode.c_cc[VMIN] = 1;
      ioctl(TtyDes, TCSETS, &CurMode);
      TermMode = GET_MODE;
   }
   else if (TermMode == POLL_MODE) {
      CurMode.c_cc[VTIME] = 0;
      CurMode.c_cc[VMIN] = 1;
      ioctl(TtyDes, TCSETS, &CurMode);
      TermMode = GET_MODE;
   }
   ioctl(TtyDes, FIONREAD, &nchars);
   if (nchars > 0)
      res = true;
#endif

#ifdef SGI
   if (TermMode == ORG_MODE) {
      CurMode.c_iflag &= ~ICRNL;
      CurMode.c_lflag &= ~(ICANON | ECHO);
      CurMode.c_cc[VTIME] = 0;
      CurMode.c_cc[VMIN] = 1;
      ioctl(TtyDes, TCSETA, &CurMode);
      TermMode = GET_MODE;
   }
   else if (TermMode == POLL_MODE) {
      CurMode.c_cc[VTIME] = 0;
      CurMode.c_cc[VMIN] = 1;
      ioctl(TtyDes, TCSETA, &CurMode);
      TermMode = GET_MODE;
   }
   ioctl(TtyDes, FIONREAD, &nchars);
   if (nchars > 0)
      res = true;
#endif

#ifdef VMS
   (void) SYS$QIOW(0, InputChan, (IO$_SENSEMODE | IO$M_TYPEAHDCNT), &iosb_desc,
                   0, 0, &Chars, sizeof(struct CHARACTERISTICS), 0, 0, 0, 0);

   if (Chars.Count > 0)
      res = true;
#endif

#ifdef HELIOS
   "How do I do this under Helios ?";
#endif

   return res;
}
