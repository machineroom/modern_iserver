/* Copyright INMOS Limited 1988,1990 */

/* CMSIDENTIFIER */
static char *CMS_Id = "PRODUCT:ITEM.VARIANT-TYPE;0(DATE)";

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <termios.h>

#include "server.h"
#include "iserver.h"
#include "pack.h"
#include "misc.h"
#include "ttyio.h"

#define ORG_MODE     0
#define GET_MODE     1
#define POLL_MODE    2

static int      TermMode = ORG_MODE;

// A Linux only version
static struct termios orig_settings, current_settings;

void HostEnd(void)
{
}

void ResetTerminal(void)
{
   dbgmsg("ResetTerminal()");
   if (TermMode != ORG_MODE) {
      tcsetattr (0, TCSANOW, &orig_settings);
      TermMode = ORG_MODE;
   }
}

void HostBegin(void)
{
   // put terminal into raw mode
   if (TermMode == ORG_MODE) {
      tcgetattr (0, &orig_settings);
      current_settings = orig_settings;
      current_settings.c_lflag &= ~ICANON;
      current_settings.c_lflag &= ~ECHO;
      tcsetattr (0, TCSANOW, &current_settings);
      TermMode = GET_MODE;
   }
}

char GetAKey(void)
{
   char            c;

   HostBegin();
   c = getchar();
   if (c==10) {
      c=13;  // Linux hacky... emulate DOS
   }

   return (c);
}

int PollKey(void)
{
   int             res=false;
   
   HostBegin();

#ifdef MSDOS
   if (kbhit())
      res = true;
#endif

   return res;
}
