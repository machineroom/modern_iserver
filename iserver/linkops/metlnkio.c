/*------------------------------------------------------------------------------
--
-- File    : metlnkio.c
--     switching link module - conforms exactly to linkio specification
--                             but provides full connection manager functionality
--
-- Copyright 1990, 1991 INMOS Limited
--
-- Date    : 3/26/91
-- Version : 1.2
--
-- 06/06/90 - dave edwards
--   originated
-- 04/09/90 - dave edwards
--   updated to conform to connection mgr spec as agreed
-- 03/12/90 - dave edwards
--   updated to include tcplinkio module
-- 04/12/90 - dave edwards
--   updated debug/info bits
-- 06/12/90 - dave edwards
--   made resource names case insensitive
-- 15/02/91 - dave edwards 
--   fixed remote access bug
-- 18/02/91 - dave edwards
--   fixed METAmethod bug re. tcp access
-- 05/03/91 - dave edwards
--   made METAmethod non-static
-- 06/03/91 - dave edwards
--   made link method value be an int based on old iserver board values
-- 19/03/91 - dave edwards 
--   added sccs id stuff
-- 13/10/92 - stuart menefy
--   added checks to LOPS_Open to ensure TCP installed on PC
--
------------------------------------------------------------------------------*/

/* CMSIDENTIFIER */
static char *CMS_Id = "PRODUCT:ITEM.VARIANT-TYPE;0(DATE)";

#include <stdio.h>
#include <ctype.h>

#include "types.h"
#include "opserror.h"
#include "conlib.h"
#include "debug.h"
#include "lmethod.h"
#include "linkops.h"

#ifdef VMS
#include <string.h>
#else
#ifdef MSDOS
#include <string.h>
#else
#ifdef SOLARIS
#include <string.h>
#else
#include <strings.h>
#endif
#endif
#endif

#ifdef LINUX
#include <string.h>
#endif

/* Link method flags */
#ifdef DOS
#define LNKb004
#define LNKb008
#endif

#ifdef ICB
#define LNKb004
#define LNKb008
#endif

#include "linkio.h"

/***
**** exported variables
***/
bool METAsearch_database = true;
int METAmethod = HW_X;

/**
 ** imports
 **/

/* from lmethod.c */
extern char *LMethodNames[];
extern int LMethodValues[];

/**
 ** internal variables
 **/

/***
**** internal functions
***/
static int Undefined ()
{
  (void) fprintf(stderr, "***FATAL ERROR : metlnkio -\n####undefined routine####\n");
  return (-1);
}

/***
**** setup pointers to functions
***/

struct VECTOR {
  int (*function)();
};                                        

bool   method_isavailable[MAX_LMETHODS];
struct VECTOR openlink_vector[MAX_LMETHODS];
struct VECTOR closelink_vector[MAX_LMETHODS];
struct VECTOR readlink_vector[MAX_LMETHODS];
struct VECTOR writelink_vector[MAX_LMETHODS];
struct VECTOR resetlink_vector[MAX_LMETHODS];
struct VECTOR analyselink_vector[MAX_LMETHODS];
struct VECTOR testerror_vector[MAX_LMETHODS];
struct VECTOR testread_vector[MAX_LMETHODS];
struct VECTOR testwrite_vector[MAX_LMETHODS];

#ifdef LNKtcp
extern int TCPOpenLink ();
extern int TCPCloseLink ();
extern int TCPReadLink ();
extern int TCPWriteLink ();
extern int TCPResetLink ();
extern int TCPAnalyseLink ();
extern int TCPTestError ();
extern int TCPTestRead ();
extern int TCPTestWrite ();
#endif

static bool StringCaseCmp (s1, s2)
char *s1, *s2;
{
  int i = 0;
  char c1, c2;

  /* if one arg is NUL strings are not equal */
  if ((s1[0] == NUL) ^ (s2[0] == NUL)) {
    return (false);
  }
  
  while ( (s1[i] != NUL) && (s2[i] != NUL) ) {
    c1 = s1[i];
    if ( isalpha(c1) && isupper(c1) ) {
      c1 = tolower(c1);
    }
    
    c2 = s2[i];
    if ( isalpha(c2) && isupper(c2) ) {
      c2 = tolower(c2);           
    }
    
    if (c1 != c2) {
      return (false);
    } else {
      i++;
    }
  }
  
  if ((s1[i] == NUL) && (s2[i] == NUL)){
    return (true);            
  } else {
    return (false);
  }
}

/***
**** exported functions (see linkio spec)
***/
int OpenLink (name)
char *name;
{

  char internal_resource[65];
  char internal_machine[129];
  char linkname[66];
  char machine[129];
  int result, i, j;
  bool search, found_but_unopened, contains_at, quoted_thismachine, local_resource;
  
#define HOSTNAMELEN 65
  char hostname[HOSTNAMELEN];

  /* setup pointers to functions */
  for (i = 0; i < MAX_LMETHODS; i++) {
    method_isavailable[i] = false;
    openlink_vector[i].function = Undefined;
    closelink_vector[i].function = Undefined;
    readlink_vector[i].function = Undefined;
    writelink_vector[i].function = Undefined;
    resetlink_vector[i].function = Undefined;
    analyselink_vector[i].function = Undefined;
    testerror_vector[i].function = Undefined;
    testread_vector[i].function = Undefined;
    testwrite_vector[i].function = Undefined;
  }
    
#ifdef LNKtsp
  method_isavailable[TSP] = true;
  openlink_vector[TSP].function    = TSPOpenLink;
  closelink_vector[TSP].function   = TSPCloseLink;
  readlink_vector[TSP].function    = TSPReadLink;
  writelink_vector[TSP].function   = TSPWriteLink;
  resetlink_vector[TSP].function   = TSPResetLink;
  analyselink_vector[TSP].function = TSPAnalyseLink;
  testerror_vector[TSP].function   = TSPTestError;
  testread_vector[TSP].function    = TSPTestRead;
  testwrite_vector[TSP].function   = TSPTestWrite;
#endif

#ifdef LNKC011
  method_isavailable[C011] = true;
  openlink_vector[C011].function    = C011OpenLink;
  closelink_vector[C011].function   = C011CloseLink;
  readlink_vector[C011].function    = C011ReadLink;
  writelink_vector[C011].function   = C011WriteLink;
  resetlink_vector[C011].function   = C011ResetLink;
  analyselink_vector[C011].function = C011AnalyseLink;
  testerror_vector[C011].function   = C011TestError;
  testread_vector[C011].function    = C011TestRead;
  testwrite_vector[C011].function   = C011TestWrite;
#endif
     
#ifdef LNKtcp
  method_isavailable[TCP_LINKOPS] = true;
  openlink_vector[TCP_LINKOPS].function     = TCPOpenLink;
  closelink_vector[TCP_LINKOPS].function    = TCPCloseLink;
  readlink_vector[TCP_LINKOPS].function     = TCPReadLink;
  writelink_vector[TCP_LINKOPS].function    = TCPWriteLink;
  resetlink_vector[TCP_LINKOPS].function    = TCPResetLink;
  analyselink_vector[TCP_LINKOPS].function  = TCPAnalyseLink;
  testerror_vector[TCP_LINKOPS].function    = TCPTestError;
  testread_vector[TCP_LINKOPS].function     = TCPTestRead;
  testwrite_vector[TCP_LINKOPS].function    = TCPTestWrite;
#endif
      
#ifdef LNKb004
  method_isavailable[B004] = true;
  openlink_vector[B004].function    = B004OpenLink;
  closelink_vector[B004].function   = B004CloseLink;
  readlink_vector[B004].function    = B004ReadLink;
  writelink_vector[B004].function   = B004WriteLink;
  resetlink_vector[B004].function   = B004ResetLink;
  analyselink_vector[B004].function = B004AnalyseLink;
  testerror_vector[B004].function   = B004TestError;
  testread_vector[B004].function    = B004TestRead;
  testwrite_vector[B004].function   = B004TestWrite;
#endif

#ifdef LNKb008
  method_isavailable[B008] = true;
  openlink_vector[B008].function    = B008OpenLink;
  closelink_vector[B008].function   = B008CloseLink;
  readlink_vector[B008].function    = B008ReadLink;
  writelink_vector[B008].function   = B008WriteLink;
  resetlink_vector[B008].function   = B008ResetLink;
  analyselink_vector[B008].function = B008AnalyseLink;
  testerror_vector[B008].function   = B008TestError;
  testread_vector[B008].function    = B008TestRead;
  testwrite_vector[B008].function   = B008TestWrite;
#endif


  METAmethod = HW_X;
   
  if (METAsearch_database == true) {
    /* initialise database search, setup resource name */
    if ((name == NULL) || (*name == NUL)) {
      DebugMessage (fprintf (stderr, "Debug       : null resource name given\n") );
      ErrorMessage (fprintf (stderr, "Error       : invalid resource name\n") );
      return (ER_LINK_SYNTAX);
    }
    
    /**
     ** parse resource name into separate internal_resource
     ** and internal_machine parts
    **/
    {
      bool strip_resource, strip_machine;
      
      internal_resource[0] = NUL;
      internal_machine[0] = NUL;
      
      strip_resource = true;
      strip_machine = false;
      contains_at = false;
      i = 0;
      while (strip_resource == true) {
        if (i > 64) {                                 
          DebugMessage (fprintf (stderr, "Debug       : resource name too long\n") );
          ErrorMessage (fprintf (stderr, "Error       : invalid resource name\n") );
          return (ER_LINK_SYNTAX);
        }
        if (name[i] == NUL) {
          internal_resource[i] = NUL;
          if (i < 1) {
            DebugMessage (fprintf (stderr, "Debug       : null resource name given\n") );
            ErrorMessage (fprintf (stderr, "Error       : invalid resource name\n") );
            return (ER_LINK_SYNTAX);
          } else {
            strip_resource = false; 
          }
          
        } else {
          if (name[i] == '@') {
            if (i < 1) {
              DebugMessage (fprintf (stderr, "Debug       : resource name before @\n") );
              ErrorMessage (fprintf (stderr, "Error       : invalid resource name\n") );
              return (ER_LINK_SYNTAX);
            } else {
              internal_resource[i] = NUL;
              strip_resource = false;
              strip_machine = true;
              contains_at = true;
              i++;
            }
          
          } else {
            internal_resource[i] = name[i];
            i++;
          } 
        }
      }
     
      if (strip_machine == false) {
      } else {
        j = 0;
        while (strip_machine == true) {
          if (j > 64) {
            DebugMessage (fprintf (stderr, "Debug       : machine name too long\n") );
            ErrorMessage (fprintf (stderr, "Error       : invalid resource name\n") );
            return (ER_LINK_SYNTAX);
          }
          if (name[i] == NUL) {
            internal_machine[j] = NUL;
            if (j < 1) {                                  
               DebugMessage (fprintf (stderr, "Debug       : null resource name given\n") );
               ErrorMessage (fprintf (stderr, "Error       : invalid resource name\n") );
              return (ER_LINK_SYNTAX);
            } else {
              internal_machine[j] = NUL;
              strip_machine = false;
            }
            
          } else {
            internal_machine[j] = name[i];
            i++;
            j++;
          }
        }
      }
    }
    
    /**
     ** see if given resource name refers just to this
     ** local machine
     **
    **/
#ifdef LNKtcp
    /**
     ** deduce name of this machine
     **/
    if (gethostname(hostname, HOSTNAMELEN) < 0) {
      ErrorMessage (fprintf (stderr, "Error       : module[metlnkio.c], function [OpenLink]\n -> gethostname failed\n") );
      return (ER_LINK_SOFT);
    }
#else
    /* no tcp resource so setup default name */
    strcpy (hostname, "localhost");
#endif /* LNKtcp */

    if (( StringCaseCmp (internal_machine, "localhost") == true) || ( StringCaseCmp (internal_machine, hostname) == true)) {
      quoted_thismachine = true;
      DebugMessage (fprintf (stderr, "Debug       : search for resource on this machine only\n") );
    } else {
      quoted_thismachine = false;
      DebugMessage (fprintf (stderr, "Debug       : search for resource on any machine\n") );
    }   
    
    if (contains_at == true) {
      if (quoted_thismachine == false) {
#ifdef LNKtcp
        /**
         ** machine name given quotes another machine so try
         ** opening a connection to it via TCP
        **/
        InfoMessage ( fprintf (stderr, "Info        : attempting TCP linkio open to resource [%s] ...\n", name) );
        if (method_isavailable[TCP_LINKOPS] == false) {
          InfoMessage ( fprintf (stderr, "Warning          : module[metlnkio.c], function [OpenLink]\n -> not compiled to drive a [%s] resource\n", LMethodNames[TCP_LINKOPS]) );
          return (ER_NO_LINK);
        } else {
          result = (*openlink_vector[TCP_LINKOPS].function) (name);
          if (result >= 0) {
            METAmethod = TCP_LINKOPS;
            return (result);
          } else {
            return (result);
          }
        }
#else
        InfoMessage ( fprintf (stderr, "Info        : can't access [%s] as not compiled to use TCP\n", name) );
        ErrorMessage (fprintf (stderr, "Error       : can't access resource [%s]\n", name) );
        return (ER_LINK_SOFT);
#endif /* LNKtcp */
      } else {
        (void) strcpy (internal_machine, hostname);
      }
    } else {
      /* no machine name quoted so look for local or remote resource */
    }
      
    result = CM_InitSearch (internal_resource, internal_machine, hostname);
    switch (result) {
      case STATUS_NOERROR:
        break;
        
      case STATUS_BAD_OPERATION:
        ErrorMessage (fprintf (stderr, "%s", conlib_errorstring) );
        return (ER_LINK_SOFT);
        break;
        
      default:
        ErrorMessage ( fprintf (stderr, "%s", conlib_errorstring) );
        return (result);
        break;
    }
    DebugMessage (fprintf (stderr, "Debug       : initialised database search\n") );
    
  }
  
  search = true;
  found_but_unopened = false;
  
  while (search == true) {
    /**
     ** decide whether to search database for an entry
    **/
    if (METAsearch_database == false) {
      /* dont search database, just read off current values */
      search = false; /* only do 1 check */
    } else {
      /* perform resource search */
      result = CM_DoSearch ();
      if (result != STATUS_NOERROR) {
        ErrorMessage (fprintf(stderr, "%s", conlib_errorstring) );
        switch (result) {
          case STATUS_BAD_TARGET_NAME:
            (void) CM_EndSearch ();
            ErrorMessage (fprintf (stderr, "%s", conlib_errorstring) );
            if (found_but_unopened == true) {
              ErrorMessage (fprintf(stderr, "Error       : can't access resource [%s]\n", name) );
            } else {
              ErrorMessage (fprintf (stderr, "Error       : can't find resource [%s]\n", name) );
            } 
            return (ER_LINK_BAD);
            break;
          
          case STATUS_BAD_OPERATION:
            (void) CM_EndSearch ();
            return (ER_LINK_SOFT);
            break;
            
          default:
            (void) CM_EndSearch ();
            ErrorMessage (fprintf (stderr, "%s", conlib_errorstring) );
            ErrorMessage (fprintf (stderr, "Error       : called by module [metlnkio.c], function [OpenLink]\n") );
            return (ER_LINK_SOFT);
            break;                
         }
      }
    }
    
    /**
     ** a match has now been made, check
     ** its fields & attempt an open 
    **/
    if (CM_GetIsWorking () == true) {
      METAmethod = CM_GetLinkMethod ();
      (void) CM_GetMachine (machine);
      (void) CM_GetLinkName (linkname);
      
      if (method_isavailable [METAmethod] == false) {
        InfoMessage ( fprintf (stderr, "Warning     : connection database weirdness, at line [%d]\n -> not compiled to drive a [%s] resource\n", CM_GetLineNumber (), LMethodNames[METAmethod]) );
        return (ER_LINK_SYNTAX);
      } else {
        /**
         ** deduce whether to access by TCP or by local driver.
         ** Again, we only call gethostname() if we can't be sure its a local resource
         **/
        if ( StringCaseCmp (machine, "localhost") == true) {
          local_resource = true;
        } else {
          /**
           ** I suppose we can call gethostname() now !
           **/
          if (gethostname(hostname, HOSTNAMELEN) < 0) {
            ErrorMessage (fprintf (stderr, "Error       : module[metlnkio.c], function [OpenLink]\n -> gethostname failed\n") );
            return (ER_LINK_SOFT);
          }
          
          if (StringCaseCmp (machine, hostname) == true) {
            local_resource = true;
          } else {
            local_resource = false;
          }
        } 
      
        if ((local_resource == true) && (METAmethod == TCP_LINKOPS)) {
          InfoMessage ( fprintf (stderr, "Warning     : connection database weirdness, at line [%d]\n -> can't access local resource via TCP\n", CM_GetLineNumber) );
        } else {
          /**
           ** ok so far, attempt to open connection
          **/
          found_but_unopened = true;
           
          if ((METAmethod == TCP_LINKOPS) || (local_resource == false) ){
#ifdef LNKtcp
            char full_name[129];
             
            strcpy (full_name, name);
            strcat (full_name, "@");
            strcat (full_name, machine);
            METAmethod = TCP_LINKOPS;
            InfoMessage ( fprintf (stderr, "Info        : attempting open to resource [%s] via method [%s]\n", full_name, LMethodNames[METAmethod]) );
            /* call underlying routine & return linkid */
            result = (*openlink_vector[METAmethod].function) (full_name);
#endif /* LNKtcp */
          } else {
            InfoMessage ( fprintf (stderr, "Info        : attempting open to resource [%s] via method [%s]\n", name, LMethodNames[METAmethod]) );
            /* call underlying routine & return linkid */
            result = (*openlink_vector[METAmethod].function) (linkname);
          }
          if (result >= 0) {
            search = false;              /* opened connection ok */
          } else {
            if (METAsearch_database == true) {
              found_but_unopened = true; /* failed to open, try another */
            } else {
              METAmethod = HW_X;
              return (ER_LINK_CANT);    /* failed to open, return */
            }
          }
        } 
      } 
    }
  }
  
  if (METAsearch_database == true) {
    (void) CM_EndSearch ();
  }

  OPScommsmode = COMMSsynchronous;
  return (result);
  
}

int CloseLink (linkid)
int linkid;
{
  int result;
  result = (*closelink_vector[METAmethod].function) (linkid);
  METAsearch_database = true;
  METAmethod = HW_X;
  return (result);
}

int ReadLink (linkid, buffer, count, timeout)
int linkid;
char *buffer;
unsigned int count;
int timeout;
{
  return (*readlink_vector[METAmethod].function) (linkid, buffer, count, timeout);
}

int WriteLink (linkid, buffer, count, timeout)
int linkid;
char *buffer;
unsigned int count;
int timeout;
{
  return (*writelink_vector[METAmethod].function) (linkid, buffer, count, timeout); 
}

int ResetLink (linkid)
int linkid;
{
  return (*resetlink_vector[METAmethod].function) (linkid);
}

int AnalyseLink (linkid)
int linkid;
{
  return (*analyselink_vector[METAmethod].function) (linkid);
}

int TestError (linkid)
int linkid;
{
  return (*testerror_vector[METAmethod].function) (linkid);
}

int TestRead (linkid)
int linkid;
{
  return (*testread_vector[METAmethod].function) (linkid);
}

int TestWrite (linkid)
int linkid;
{
  return (*testwrite_vector[METAmethod].function) (linkid);
}











