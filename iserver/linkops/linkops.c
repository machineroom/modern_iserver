/*------------------------------------------------------------------------------
--
--  File : linkops.c
--     linkops host module 
--
-- Copyright 1990, 1991 INMOS Limited
--
--  Date    : 4/3/92
--  Version : 1.3
--
-- 20/06/90 - dave edwards
--   originated & heartbeat function added
-- 04/09/90 - dave edwards
--   updated to conform to connection mgr spec as agreed
-- 04/10/90 - dave edwards
--   updated to return error messages into lnkops_errorstring rather
--     than print out
-- 10/10/90 - dave edwards
--   made timeout parameter of OPS_GetRequest a long int
-- 03/11/90 -dave edwards
--   'vaxified'
-- 04/12/90 - dave edwards
--   updated debug/info bits
-- 06/12/90 - dave edwards
--   made resources case insensitive
-- 25/02/91 - dave edwards
--   added buffer_depth parameter to OPS_Open
-- 06/03/91 - dave edwards
--   added local_method parameter to OPS_Open
-- 19/03/91 - dave edwards 
--   added sccs id stuff
-- 03/04/92 - nick stephen
--   added fix to allow local linkops to be used on
--   PC's without PC/TCP or PC-NFS
-- 06/06/92 - nick stephen
--   added fix to bob's fix to allow local linkops to be used on
--   PC's without PC/TCP or PC-NFS
-- 13/10/92 - stuart menefy
--   added fix to nick's fix to allow local linkops to be used on
--   PC's without PC/TCP or PC-NFS, and allow ...@local host to work
--
------------------------------------------------------------------------------*/

/* CMSIDENTIFIER */
static char *CMS_Id = "LINKOPS_H:LINKOPS_C.AAAA-FILE;1(15-JAN-92)";

#include <stdio.h>
#include <ctype.h>

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

/* prototypes for imported library bits*/
#include "types.h"
#include "linkops.h"
#include "lops.h"
#include "tops.h"
#include "conlib.h"
#include "debug.h"
#include "opserror.h"
#include "lmethod.h"

#define OPSMETHODundefined 0
#define OPSMETHODlinkio    1
#define OPSMETHODtcp       2

/**
 ** exported variables
 **/
 
char lnkops_errorstring[1024] = NULSTRING;
enum CommsModes OPScommsmode = COMMSclosed;

/**
 ** internal variables
**/    
static enum ErrorModes OPSerrorflagmode = ERRORignore;
static unsigned int OPSmethod = OPSMETHODundefined;
static unsigned char OPSerrorstatus = STATUS_NOERROR;

/**
 ** internal functions
**/    
static unsigned char Undefined ()
{
  (void) fprintf(stderr, "***FATAL LINKOPS ERROR:\n  undefined linkops vector - panic\n");
  exit(666);
}

/**
 ** vectors to linkops internal functions
**/
static struct VECTOR {
  unsigned char (*function) ();
};

/* close vector */
static struct VECTOR close_vector [] = {
  Undefined,
  LOPS_Close,
#ifdef OPStcp
  TOPS_Close 
#else
  Undefined
#endif
};

/* comms_async vector */
static struct VECTOR commsasync_vector [] = {
  Undefined,
  LOPS_CommsAsynchronous,
#ifdef OPStcp
  TOPS_CommsAsynchronous
#else
  Undefined
#endif
};

/* comms_sync vector */
static struct VECTOR commssync_vector [] = { 
  Undefined,  
  LOPS_CommsSynchronous,
#ifdef OPStcp
  TOPS_CommsSynchronous
#else
  Undefined
#endif
};

/*  errordetect vector */
static struct VECTOR errordetect_vector [] = {
  Undefined,
  LOPS_ErrorDetect,
#ifdef OPStcp
  TOPS_ErrorDetect
#else
  Undefined
#endif
};

/* errorignore vector */
static struct VECTOR errorignore_vector [] = {
  Undefined,
  LOPS_ErrorIgnore,
#ifdef OPStcp
  TOPS_ErrorIgnore
#else
  Undefined
#endif
};

/* bootwrite vector */
static struct VECTOR bootwrite_vector [] = {
  Undefined,
  LOPS_BootWrite,
#ifdef OPStcp
  TOPS_BootWrite
#else
  Undefined
#endif
};

/* reset vector */
static struct VECTOR reset_vector [] = {
  Undefined,
  LOPS_Reset,
#ifdef OPStcp
  TOPS_Reset
#else
  Undefined
#endif
};

/* analyse vector */
static struct VECTOR analyse_vector [] = {
  Undefined,
  LOPS_Analyse,    
#ifdef OPStcp
  TOPS_Analyse
#else
  Undefined
#endif
};

/* poke16 vector */ 
static struct VECTOR poke16_vector [] = {
  Undefined,
  LOPS_Poke16,
#ifdef OPStcp
  TOPS_Poke16
#else
  Undefined
#endif
};

/* poke32 vector */
static struct VECTOR poke32_vector [] = {
  Undefined,
  LOPS_Poke32,
#ifdef OPStcp
  TOPS_Poke32
#else
  Undefined
#endif
};

/* peek16 vector */               
static struct VECTOR peek16_vector [] = {
  Undefined,
  LOPS_Peek16,
#ifdef OPStcp
  TOPS_Peek16
#else
  Undefined
#endif
};

/* peek32 vector */               
static struct VECTOR peek32_vector [] = {
  Undefined,
  LOPS_Peek32,
#ifdef OPStcp
  TOPS_Peek32
#else
  Undefined
#endif
};

/* getrequest vector */
static struct VECTOR getrequest_vector [] = {
  Undefined,
  LOPS_GetRequest,    
#ifdef OPStcp
  TOPS_GetRequest
#else
  Undefined
#endif
};

/* sendreply vector */
static struct VECTOR sendreply_vector [] = {
  Undefined,
  LOPS_SendReply,    
#ifdef OPStcp
  TOPS_SendReply
#else
  Undefined
#endif
};

/* restart vector */
static struct VECTOR restart_vector [] = {
  Undefined,
  LOPS_Restart,
#ifdef OPStcp
  TOPS_Restart
#else
  Undefined
#endif
};

static BOOL StringCaseCmp (s1, s2)
char *s1, *s2;
{
  int i = 0;
  char c1, c2;

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
      return (FALSE);
    } else {
      i++;
    }
  }
  
  if ((s1[i] == NUL) && (s2[i] == NUL)){
    return (TRUE);            
  } else {
    return (FALSE);
  }
}

/*********
********** exported functions
*********/    

/*
   modifies : connection_id, server_name, link_name
              OPSerrorstatus, OPSmethod, OPSerrorflagmode
              (conlib) database_search + database inspectors
*/
unsigned char OPS_Open (resource, buffer_depth, connection_id, localmethod, server_name, link_name)
char *resource;
int buffer_depth;
long int *connection_id;
int *localmethod;
char *server_name;
char *link_name;
{
  BOOL contains_at, inuse = FALSE;
  BOOL found = FALSE, located = FALSE, local_resource;
  char internal_resourcename[65], internal_machinename[129];
  int iomethod;
  unsigned char result;
  
#define HOSTNAMELEN 65
  char hostname[HOSTNAMELEN];
  
  DebugMessage ( fprintf (stderr, "[OPS_Open\n") );
  
  strcpy (lnkops_errorstring, NULSTRING);
  
  if (OPScommsmode != COMMSclosed) {
    (void) sprintf (lnkops_errorstring, " : module [linkops.c], function [OPS_Open]\n -> bad comms mode\n");
    OPSerrorstatus = STATUS_BAD_COMMS_MODE;
    return (OPSerrorstatus);
  }
  
  if (buffer_depth < 1) {
    (void) sprintf (lnkops_errorstring, " : module [linkops.c], function [OPS_Open]\n -> bad buffer depth value\n");
    OPSerrorstatus = STATUS_BAD_OPERATION;
    return (OPSerrorstatus);
  }
  
  /* initialise database search, setup resource name */
  if ((resource == NULL) || (*resource == NUL)) {
    DebugMessage ( fprintf (stderr, "Debug       : null resource name given\n") );
    (void) sprintf (lnkops_errorstring, " : invalid resource name\n");
    OPSerrorstatus = STATUS_BAD_OPERATION;            
    return (OPSerrorstatus);
  } else {
    (void) strcpy (internal_resourcename, resource);
  }
  
  /**
   ** parse resource name into separate internal_resource
   ** and internal_machine parts
  **/
  {
    BOOL strip_resource, strip_machine;
    int i,j;
    
    internal_resourcename[0] = NUL;
    internal_machinename[0] = NUL;
    
    strip_resource = TRUE;
    strip_machine = FALSE;
    contains_at = FALSE;
    i = 0;
    while (strip_resource == TRUE) {
      if (i > 64) {                                 
        DebugMessage ( fprintf (stderr, "Debug       : resource name too long\n") );
        (void) sprintf (lnkops_errorstring, " : invalid resource name\n");
        OPSerrorstatus = STATUS_BAD_OPERATION;
        return (OPSerrorstatus);
      }
      if (resource[i] == NUL) {
        internal_resourcename[i] = NUL;
        if (i < 1) {
          DebugMessage ( fprintf (stderr, "Debug       : null resource name given\n") );
          (void) sprintf (lnkops_errorstring, " : invalid resource name\n");
          OPSerrorstatus = STATUS_BAD_OPERATION;
          return (OPSerrorstatus);
        } else {
          strip_resource = FALSE; 
        }
        
      } else {
        if (resource[i] == '@') {
          if (i < 1) {
            DebugMessage ( fprintf (stderr, "Debug       : resource name before @\n") );
            (void) sprintf (lnkops_errorstring, " : invalid resource name\n");
            OPSerrorstatus = STATUS_BAD_OPERATION;
            return (OPSerrorstatus);
          } else {
            internal_resourcename[i] = NUL;
            strip_resource = FALSE;
            strip_machine = TRUE;
            contains_at = TRUE;
            i++;
          }
        
        } else {
          internal_resourcename[i] = resource[i];
          i++;
        } 
      }
    }
   
    if (strip_machine == FALSE) {
    } else {
      j = 0;
      while (strip_machine == TRUE) {
        if (j > 64) {
          DebugMessage ( fprintf (stderr, "Debug       : machine name too long\n") );
          (void) sprintf (lnkops_errorstring, " : invalid resource name\n");
          OPSerrorstatus = STATUS_BAD_OPERATION;
          return (OPSerrorstatus);
        }
        if (resource[i] == NUL) {
          internal_machinename[j] = NUL;
          if (j < 1) {                                  
            DebugMessage ( fprintf (stderr, "Debug       : null resource name given\n") );
            (void) sprintf (lnkops_errorstring, " : invalid resource name\n");
            OPSerrorstatus = STATUS_BAD_OPERATION;
            return (OPSerrorstatus);
          } else {
            internal_machinename[j] = NUL;
            strip_machine = FALSE;
          }
          
        } else {
          internal_machinename[j] = resource[i];
          i++;
          j++;
        }
      }
    }
  }
  
#ifdef OPStcp
  /**
   ** Deduce name of this machine
   ** Only call gethostname() if we have network software installed.
   ** If we can't determine the hostname, set it to "localhost".
   **/
#ifdef PCNFS
  if (is_pc_nfs_installed()) {
#endif
#ifdef PCTCP
  if (vec_search()) {
#endif
     if (gethostname(hostname, HOSTNAMELEN) < 0) {
       (void) sprintf (lnkops_errorstring, " : module[linkops.c], function [OPS_Open]\n -> gethostname failed\n");
       OPSerrorstatus = STATUS_COMMS_FATAL;
       return (OPSerrorstatus);
     }
#if defined(PCNFS) || defined(PCTCP)
  }
  else {
    if ((contains_at == TRUE) && (StringCaseCmp (internal_machinename, "localhost") == FALSE)) {
      InfoMessage ( fprintf (stderr, "Info        : can't access remote resource [%s], TCP not installed\n", resource) );
      (void) sprintf (lnkops_errorstring, " : can't access resource [%s]\n", resource);
      OPSerrorstatus = STATUS_BAD_OPERATION;
      return (OPSerrorstatus);
    }
    /* No TCP, equivalent to specifying "localhost" on the command line */
    (void) strcpy (hostname, "localhost");
    (void) strcpy (internal_machinename, "localhost");
    contains_at = TRUE;
  }
#endif
  
  /**
   ** Catagorize the type of match we want.
   **/
  if (contains_at == TRUE) {
    if ((StringCaseCmp (internal_machinename, "localhost") == TRUE) ||
        (StringCaseCmp (internal_machinename, hostname   ) == TRUE)) {
      /**
       ** Specified this hosts name.
       **/
      DebugMessage ( fprintf (stderr, "Debug       : search for resource on this machine only\n") );
      (void) strcpy (internal_machinename, hostname);   /* Must pass real machine name to CM_Search, not "localhost" */
    } else {
      /**
       ** machine name given quotes another machine so try
       ** opening a connection to it via TCP
       **/
      DebugMessage ( fprintf (stderr, "Debug       : attempt direct connection to %s\n", internal_machinename) );
      result = TOPS_Open (internal_resourcename, internal_machinename, buffer_depth, connection_id, localmethod, link_name);
      if (result == STATUS_NOERROR) {
        OPScommsmode = COMMSsynchronous;
        OPSerrorflagmode = ERRORignore;
        OPSerrorstatus = STATUS_NOERROR;
        OPSmethod = OPSMETHODtcp;
        (void) strcpy (server_name, internal_machinename);
        InfoMessage ( fprintf (stderr, "Info        : allocated resource [%s] @ server [%s]\n",
                                (char *) resource, (char *) server_name ) );                  
        InfoMessage ( fprintf (stderr, "Info        : as device [%s] using localmethod [%d] with connection_id [%ld]\n",
                                (char *) link_name, *localmethod,
                                (long int) *connection_id) );
        DebugMessage ( fprintf (stderr, "]OPS_Open\n\n") );
        OPSerrorstatus = STATUS_NOERROR;
        return (OPSerrorstatus);
      } else {
        OPSmethod = OPSMETHODundefined;
        switch (result) {
          case STATUS_TARGET_UNAVAILABLE:
            (void) sprintf (lnkops_errorstring, " : resource [%s] in use\n", (char *) resource);
            break;
            
          case STATUS_BAD_TARGET_NAME:
            (void) sprintf (lnkops_errorstring, " : no such resource [%s]\n", (char *) resource);
            break;
            
          default:
            (void) sprintf (lnkops_errorstring, " : can't access resource [%s]\n", (char *)resource);
            break;                             
        }
        return (STATUS_COMMS_FATAL);
      }
    }
  } else {
    /**
     ** No machine name quoted so look for local or remote resource.
     **/
    DebugMessage ( fprintf (stderr, "Debug       : search for resource on local or remote machine\n") );
  }
#else
  /* setup name of this machine */
  if ((contains_at == TRUE) && (StringCaseCmp (internal_machinename, "localhost") == FALSE)) {
    InfoMessage ( fprintf (stderr, "Info        : can't access [%s] not compiled to use TCP\n", resource) );
    (void) sprintf (lnkops_errorstring, " : can't access resource [%s]\n", resource);
    OPSerrorstatus = STATUS_BAD_OPERATION;
    return (OPSerrorstatus);
  }
  DebugMessage ( fprintf (stderr, "Debug       : search for resource on this machine only\n") );
  (void) strcpy (hostname, "localhost");
#endif /* OPStcp */

  OPSerrorstatus = CM_InitSearch (internal_resourcename, internal_machinename, hostname);
  switch (OPSerrorstatus) {
    case STATUS_NOERROR:
      break;
      
    default:
      (void) strcpy (lnkops_errorstring, conlib_errorstring);
      return (OPSerrorstatus);
      break;
  }
  
  found = FALSE;
  located = FALSE;
  
  while (found == FALSE) {
    /**
     ** search for a matching entry 
    **/
    {
      OPSerrorstatus = CM_DoSearch ();
      if (OPSerrorstatus != STATUS_NOERROR) {
        (void) strcpy (lnkops_errorstring, conlib_errorstring);
        switch (OPSerrorstatus) {
          case STATUS_BAD_TARGET_NAME:
            (void) CM_EndSearch ();
            if (located == TRUE) {
              if (inuse == TRUE) {
                (void) sprintf (&lnkops_errorstring[strlen(conlib_errorstring)], " : resource [%s] in use\n", (char *) resource);
              } else {
                (void) sprintf(&lnkops_errorstring[strlen(conlib_errorstring)], " : can't access resource [%s]\n", resource);
              }
            } else {
              (void) sprintf(&lnkops_errorstring[strlen(conlib_errorstring)], " : no such resource [%s]\n", (char *) resource);
            }
            OPSerrorstatus = STATUS_BAD_TARGET_NAME;
            break;
            
          case STATUS_BAD_OPERATION:
            (void) CM_EndSearch ();
            OPSerrorstatus = STATUS_BAD_OPERATION;
            break;
            
          default:
            (void) CM_EndSearch ();
            (void) sprintf(&lnkops_errorstring[strlen(conlib_errorstring)], "called by linkops.c, function [OPS_Open]\n");
            OPSerrorstatus = STATUS_BAD_OPERATION;
            break;
        }
        DebugMessage ( fprintf (stderr, "]OPS_Open\n\n") );
        return (OPSerrorstatus);
      }
    }
    
    /**
     ** a match has been made, gets fields & if seems ok,
    **/
    if (CM_GetIsWorking () == TRUE) {
      located = TRUE;
      
      (void) CM_GetMachine (server_name);
      iomethod = CM_GetLinkMethod ();
      
      /**
       ** deduce whether to access by TCP or by local driver
      **/
      if ((StringCaseCmp (server_name, "localhost") == TRUE) ||
          (StringCaseCmp (server_name, hostname   ) == TRUE)) {
        local_resource = TRUE;
      } else {
          local_resource = FALSE;
      }
      
      if ((local_resource == TRUE) && (iomethod == TCP_LINKOPS)) {
        InfoMessage ( fprintf (stderr, "Warning     : connection database weirdness, at line [%d]\n -> can't access local resource via TCP\n", CM_GetLineNumber ()) );
        
      } else {
        /**
         ** record is valid so attempt to open a connection to it
        **/
        if (local_resource == TRUE) {
          /**
           ** resource is local, so try to access it as such
           **/
          OPSerrorstatus = LOPS_Open (internal_resourcename, NULSTRING, buffer_depth, connection_id, localmethod, link_name);
          OPSmethod = OPSMETHODlinkio;
        } else {
          /**
           ** resource is remote, so try to access via TCP
          **/
#ifdef OPStcp
          OPSmethod = OPSMETHODtcp;
          OPSerrorstatus = TOPS_Open (internal_resourcename, server_name, buffer_depth, connection_id, localmethod,link_name);
#else
          InfoMessage ( fprintf (stderr, "Warning     : can't access  [%s@%s] (not compiled to use TCP)\n", resource, server_name) );
          OPSerrorstatus = STATUS_BAD_OPERATION;
#endif /* OPStcp */
        }   
        /**
         ** check return status of open attempt
        **/
        switch (OPSerrorstatus) {
          case STATUS_NOERROR:
            found = TRUE;
            break;
          
          case STATUS_TARGET_UNAVAILABLE:
            OPSmethod = OPSMETHODundefined;
            inuse = TRUE;
            break;
          
          /* non-fatal error */
          case STATUS_BAD_OPERATION:
          case STATUS_BAD_SERVER:
          case STATUS_BAD_METHOD:
          case STATUS_BAD_TARGET_NAME:
            OPSmethod = OPSMETHODundefined;
            break;
            
          /* fatal error */
          default:
            (void) CM_EndSearch ();
            OPSmethod = OPSMETHODundefined;
            return (OPSerrorstatus);
            break;
        }
      }
    } else {
      DebugMessage ( fprintf (stderr, "Debug       : no match made\n") );
    }
  }
  (void) CM_EndSearch ();
  
  InfoMessage ( fprintf (stderr, "Info        : allocated resource [%s] @ server [%s]\n",
                    (char *) resource, (char *) server_name ) );                        
  InfoMessage ( fprintf (stderr, "Info        : as device [%s] using localmethod [%d] with connection_id [%ld]\n",
                       (char *) link_name, (int) *localmethod,
                       (long int) *connection_id ) );
  
  /* setup default state information */
  OPScommsmode = COMMSsynchronous;
  OPSerrorflagmode = ERRORignore;
  OPSerrorstatus = STATUS_NOERROR;
  DebugMessage ( fprintf (stderr, "]OPS_Open\n\n") );
  return (OPSerrorstatus);
  
}

/*
  modifies : OPSerrorstatus
    OPScommsmode = COMMSclosed;
    OPSerrorflagmode = ERRORignore;
    OPSmethod = OPSMETHODundefined;
    OPSerrorstatus = STATUS_NOERROR;
*/
unsigned char OPS_Close (connection_id)
long int connection_id;
{
  DebugMessage ( fprintf (stderr, "[OPS_Close\n") );
  
  strcpy (lnkops_errorstring, NULSTRING);
  
  if ((OPScommsmode == COMMSasynchronous) || (OPScommsmode == COMMSsynchronous)) {
    OPSerrorstatus = (*close_vector[OPSmethod].function) (connection_id);
    OPSmethod = OPSMETHODundefined;
  } else {
    (void) sprintf (lnkops_errorstring, " : module [linkops.c], function [OPS_Close]\n -> bad comms mode\n");
    OPSerrorstatus = STATUS_BAD_COMMS_MODE;
  }
  OPScommsmode = COMMSclosed;
  OPSerrorflagmode = ERRORignore;
  OPSmethod = OPSMETHODundefined;
  OPSerrorstatus = STATUS_NOERROR;
  DebugMessage ( fprintf (stderr, "]OPS_Close\n\n") );
  return (OPSerrorstatus);
}

/*
 modifies : OPSerrorstatus, OPScommsmode, OPSerrorflagmode
*/
unsigned char OPS_Restart (connection_id)
long int connection_id;
{
  DebugMessage ( fprintf (stderr, "[OPS_Restart\n") );
  
  strcpy (lnkops_errorstring, NULSTRING);
  
  if ((OPScommsmode == COMMSasynchronous) || (OPScommsmode == COMMSsynchronous)) {
    OPSerrorstatus = (*restart_vector[OPSmethod].function) (connection_id, &OPScommsmode, &OPSerrorflagmode);
  } else {
    (void) sprintf (lnkops_errorstring, " : module [linkops.c], function [OPS_Restart]\n -> bad comms mode\n");
    OPSerrorstatus = STATUS_BAD_COMMS_MODE;
  }
  DebugMessage ( fprintf (stderr, "]OPS_Restart\n\n") );
  return (OPSerrorstatus);
}

/*
  modifies : OPScommsmode mode 
*/
unsigned char OPS_CommsAsynchronous (connection_id)
long int connection_id;
{
  DebugMessage ( fprintf (stderr, "[OPS_CommsAsynchronous\n") );
  
  strcpy (lnkops_errorstring, NULSTRING);
  
  if ((OPScommsmode == COMMSasynchronous) || (OPScommsmode == COMMSsynchronous)) {
    OPSerrorstatus = (*commsasync_vector[OPSmethod].function) (connection_id, &OPScommsmode, OPSerrorflagmode);
    if (OPSerrorstatus == STATUS_TARGET_ERROR) {
      (void) sprintf (lnkops_errorstring, " : transputer error flag set\n");
    }
  } else {
    (void) sprintf (lnkops_errorstring, " : module [linkops.c], function [OPS_CommsAsynchronous]\n -> bad comms mode\n");
    OPSerrorstatus = STATUS_BAD_COMMS_MODE;
  }
  DebugMessage ( fprintf (stderr, "]OPS_CommsAsynchronous\n\n") );
  return (OPSerrorstatus);
}

/*
  modifies : OPScommsmode mode 
*/
unsigned char OPS_CommsSynchronous (connection_id)
long int connection_id;
{
  DebugMessage ( fprintf (stderr, "[OPS_CommsSynchronous\n") );
  
  strcpy (lnkops_errorstring, NULSTRING);
  
  if ((OPScommsmode == COMMSasynchronous) || (OPScommsmode == COMMSsynchronous)) {
    OPSerrorstatus = (*commssync_vector[OPSmethod].function) (connection_id, &OPScommsmode, OPSerrorflagmode);
    if (OPSerrorstatus == STATUS_TARGET_ERROR) {
      (void) sprintf (lnkops_errorstring, " : transputer error flag set\n");
    }
  } else {
    (void) sprintf (lnkops_errorstring, " : module [linkops.c], function [OPS_CommsSynchronous]\n -> bad comms mode\n");
    OPSerrorstatus = STATUS_BAD_COMMS_MODE;
  }
  DebugMessage ( fprintf (stderr, "]OPS_CommsSynchronous\n\n") );
  return (OPSerrorstatus);
}

/*
  modifies : OPSerrorflag mode 
*/
unsigned char OPS_ErrorDetect (connection_id)
long int connection_id;
{
  DebugMessage ( fprintf (stderr, "[OPS_ErrorDetect\n") );
  
  strcpy (lnkops_errorstring, NULSTRING);
  
  if (OPScommsmode == COMMSsynchronous) {
    OPSerrorstatus = (*errordetect_vector[OPSmethod].function) (connection_id, &OPSerrorflagmode);
  } else {
    (void) sprintf (lnkops_errorstring, " : module [linkops.c], function [OPS_ErrorDetect]\n -> bad comms mode\n");
    OPSerrorstatus = STATUS_BAD_COMMS_MODE;
  }
  DebugMessage ( fprintf (stderr, "]OPS_ErrorDetect\n\n") );
  return (OPSerrorstatus);
}

/*
  modifies : OPSerrorflag mode 
*/
unsigned char OPS_ErrorIgnore (connection_id)
long int connection_id;
{
  DebugMessage ( fprintf (stderr, "[OPS_ErrorIgnore\n") );
  
  strcpy (lnkops_errorstring, NULSTRING);
  
  if (OPScommsmode == COMMSsynchronous) {
    OPSerrorstatus = (*errorignore_vector[OPSmethod].function) (connection_id, &OPSerrorflagmode);
  } else {
    (void) sprintf (lnkops_errorstring, " : module [linkops.c], function [OPS_ErrorIgnore]\n -> bad comms mode\n");
    OPSerrorstatus = STATUS_BAD_COMMS_MODE;
  }
  DebugMessage ( fprintf (stderr, "]OPS_ErrorIgnore\n\n") );
  return (OPSerrorstatus);
}

/*
  modifies : OPSerrorstatus
*/
unsigned char OPS_BootWrite (connection_id, buffer, num_bytes)
long int connection_id;
unsigned char *buffer;
unsigned long num_bytes;
{
  DebugMessage ( fprintf (stderr, "[OPS_BootWrite\n") );
  if (OPScommsmode == COMMSsynchronous) {
    strcpy (lnkops_errorstring, NULSTRING);
    OPSerrorstatus = (*bootwrite_vector[OPSmethod].function) (connection_id, buffer, num_bytes, OPSerrorflagmode);
    if (OPSerrorstatus == STATUS_TARGET_ERROR) {
      (void) sprintf (lnkops_errorstring, " : transputer error flag set\n");
    }
  } else {
    (void) sprintf (lnkops_errorstring, " : module [linkops.c], function [OPS_BootWrite]\n -> bad comms mode\n");
    OPSerrorstatus = STATUS_BAD_COMMS_MODE;
  }
  DebugMessage ( fprintf (stderr, "]OPS_BootWrite\n\n") );
  return (OPSerrorstatus);
}

/*
  modifies : OPSerrorstatus
*/
unsigned char OPS_Reset (connection_id, processor_id)
long int connection_id;
long int processor_id;
{
  DebugMessage ( fprintf (stderr, "[OPS_Reset\n") );
  
  strcpy (lnkops_errorstring, NULSTRING);
  
  if (processor_id != TSERIES_PROCESSORID) {
    (void) sprintf (lnkops_errorstring, " : module [linkops.c], function [OPS_Reset]\n -> bad processor_id (T-Series)\n");
    return (STATUS_BAD_OPERATION);
  }
  if (OPScommsmode == COMMSsynchronous) {
    OPSerrorstatus = (*reset_vector[OPSmethod].function) (connection_id, processor_id);
  } else {
    (void) sprintf (lnkops_errorstring, " : module [linkops.c], function [OPS_Reset]\n -> bad comms mode\n");
    OPSerrorstatus = STATUS_BAD_COMMS_MODE;
  }
  DebugMessage ( fprintf (stderr, "]OPS_Reset\n\n") );
  return (OPSerrorstatus);
}

/*
  modifies : OPSerrorstatus
*/
unsigned char OPS_Analyse (connection_id, processor_id)
long int connection_id;
long int processor_id;
{
  DebugMessage ( fprintf (stderr, "[OPS_Analyse\n") );
  
  strcpy (lnkops_errorstring, NULSTRING);
  
  if (processor_id != TSERIES_PROCESSORID) {
    (void) sprintf (lnkops_errorstring, " : module [linkops.c], function [OPS_Analyse]\n -> bad processor_id (T-Series)\n");
    return (STATUS_BAD_OPERATION);
  }
  
  if (OPScommsmode == COMMSsynchronous) {
    OPSerrorstatus = (*analyse_vector[OPSmethod].function) (connection_id, processor_id);
  } else {
    (void) sprintf (lnkops_errorstring, " : module [linkops.c], function [OPS_Analyse]\n -> bad comms mode\n");
    OPSerrorstatus = STATUS_BAD_COMMS_MODE;
  }
  DebugMessage ( fprintf (stderr, "]OPS_Analyse\n\n") );
  return (OPSerrorstatus);
}

/*
  modifies : OPSerrorstatus
*/
unsigned char OPS_Poke16 (connection_id, processor_id, pokesize, address, buffer)
long int connection_id;
long int processor_id;
unsigned long pokesize;
short int address;
unsigned char *buffer;
{
  DebugMessage ( fprintf (stderr, "[OPS_Poke16\n") );
  
  strcpy (lnkops_errorstring, NULSTRING);
  
  if (processor_id != TSERIES_PROCESSORID) {
    (void) sprintf (lnkops_errorstring, " : module [linkops.c], function [OPS_Poke16]\n -> bad processor_id (T-Series)\n");
    return (STATUS_BAD_OPERATION);
  }
  
  if (OPScommsmode == COMMSsynchronous) {
    OPSerrorstatus = (*poke16_vector[OPSmethod].function) (connection_id, processor_id, pokesize, address, buffer); 
  } else {
    (void) sprintf (lnkops_errorstring, " : module [linkops.c], function [OPS_Poke16]\n -> bad comms mode\n");
    OPSerrorstatus = STATUS_BAD_COMMS_MODE;
  }
  DebugMessage ( fprintf (stderr, "]OPS_Poke16\n\n") );
  return (OPSerrorstatus);
}

/*
  modifies : OPSerrorstatus
*/
unsigned char OPS_Poke32 (connection_id, processor_id, pokesize, address, buffer)
long int connection_id;
long int processor_id;
unsigned long pokesize;
long int address;
unsigned char *buffer;
{
  DebugMessage ( fprintf (stderr, "[OPS_Poke32\n") );
  
  strcpy (lnkops_errorstring, NULSTRING);
  
  if (processor_id != TSERIES_PROCESSORID) {
    (void) sprintf (lnkops_errorstring, " : module [linkops.c], function [OPS_Poke32]\n -> bad processor_id (T-Series)\n");
    return (STATUS_BAD_OPERATION);
  }
  
  if (OPScommsmode == COMMSsynchronous) {
    OPSerrorstatus = (*poke32_vector[OPSmethod].function) (connection_id, processor_id, pokesize, address, buffer); 
  } else {
    (void) sprintf (lnkops_errorstring, " : module [linkops.c], function [OPS_Poke32]\n -> bad comms mode\n");
    OPSerrorstatus = STATUS_BAD_COMMS_MODE;
  }
  DebugMessage ( fprintf (stderr, "]OPS_Poke32\n\n") );
  return (OPSerrorstatus);
}

/*
   modifies : OPSerrorstatus, buffer
*/
unsigned char OPS_Peek16 (connection_id, processor_id, peeksize, address, buffer)
long int connection_id;
long int processor_id;
unsigned long peeksize;
short int address;
unsigned char *buffer;
{
  DebugMessage ( fprintf (stderr, "[OPS_Peek16\n") );
  
  strcpy (lnkops_errorstring, NULSTRING);
  
  if (processor_id != TSERIES_PROCESSORID) {
    (void) sprintf (lnkops_errorstring, " : module [linkops.c], function [OPS_Peek16]\n -> bad processor_id (T-Series)\n");
    return (STATUS_BAD_OPERATION);
  }
  
  if (OPScommsmode == COMMSsynchronous) {
    OPSerrorstatus = (*peek16_vector[OPSmethod].function) (connection_id, processor_id, peeksize, address, buffer); 
  } else {
    (void) sprintf (lnkops_errorstring, " : module [linkops.c], function [OPS_Peek16]\n -> bad comms mode\n");
    OPSerrorstatus = STATUS_BAD_COMMS_MODE;
  }
  DebugMessage ( fprintf (stderr, "]OPS_Peek16\n\n") );
  return (OPSerrorstatus);
}

/*
   modifies : OPSerrorstatus, buffer
*/
unsigned char OPS_Peek32 (connection_id, processor_id, peeksize, address, buffer)
long int connection_id;
long int processor_id;
unsigned long peeksize;
long int address;
unsigned char *buffer;
{
  DebugMessage ( fprintf (stderr, "[OPS_Peek32\n") );
  
  strcpy (lnkops_errorstring, NULSTRING);
  
  if (processor_id != TSERIES_PROCESSORID) {
    (void) sprintf (lnkops_errorstring, " : module [linkops.c], function [OPS_Peek32]\n -> bad processor_id (T-Series)\n");
    return (STATUS_BAD_OPERATION);
  }
  
  if (OPScommsmode == COMMSsynchronous) {
    OPSerrorstatus = (*peek32_vector[OPSmethod].function) (connection_id, processor_id, peeksize, address, buffer); 
  } else {
    (void) sprintf (lnkops_errorstring, " : module [linkops.c], function [OPS_Peek32]\n -> bad comms mode\n");
    OPSerrorstatus = STATUS_BAD_COMMS_MODE;
  }
  DebugMessage ( fprintf (stderr, "]OPS_Peek32\n\n") );
  return (OPSerrorstatus);
}

/*
 modifies : OPSerrorstatus
*/
unsigned char OPS_GetRequest (connection_id, buffer, timeout, heartbeat_fn)
long int connection_id;
unsigned char *buffer;
long int timeout;
unsigned char (*heartbeat_fn) ();
{
  DebugMessage ( fprintf (stderr, "[OPS_GetRequest\n") );
  
  strcpy (lnkops_errorstring, NULSTRING);
  
  if (OPScommsmode == COMMSasynchronous) {
    OPSerrorstatus = (*getrequest_vector[OPSmethod].function) (connection_id, buffer, timeout, heartbeat_fn, OPSerrorflagmode);
    if (OPSerrorstatus == STATUS_TARGET_ERROR) {
      (void) sprintf (lnkops_errorstring, " : transputer error flag set\n");
    }
  } else {
    (void) sprintf (lnkops_errorstring, " : module [linkops.c], function [OPS_GetRequest]\n -> bad comms mode = [%d]\n", OPScommsmode);
    OPSerrorstatus = STATUS_BAD_COMMS_MODE;
  }
  DebugMessage ( fprintf (stderr, "]OPS_GetRequest\n\n") );
  return (OPSerrorstatus);
}
  
/*
 modifies : OPSerrorstatus
*/
unsigned char OPS_SendReply (connection_id, buffer)
long int connection_id;
unsigned char *buffer;
{
  DebugMessage ( fprintf (stderr, "[OPS_SendReply\n") );
  
  strcpy (lnkops_errorstring, NULSTRING);
  
  if (OPScommsmode == COMMSasynchronous) {
    OPSerrorstatus = (*sendreply_vector[OPSmethod].function) (connection_id, buffer, OPSerrorflagmode);
    if (OPSerrorstatus == STATUS_TARGET_ERROR) {
      (void) sprintf (lnkops_errorstring, " : transputer error flag set\n");
    }
  } else {
    (void) sprintf (lnkops_errorstring, " : module [linkops.c], function [OPS_SendReply]\n -> bad comms mode\n");
    OPSerrorstatus = STATUS_BAD_COMMS_MODE;
  }
  DebugMessage ( fprintf (stderr, "]OPS_SendReply\n\n") );
  return (OPSerrorstatus);
}
  








