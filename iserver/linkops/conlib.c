/*-----------------------------------------------------------------------------
--
-- File : conlib.c
--
--  host connection manager library
--   -> searches connection library database for particular resources
--
-- Copyright 1990, 1991 INMOS Limited
--
--  Date    : 4/17/91
--  Version : 1.3
--
-- 17/05/90 dave edwards
-- 07/05/90 dave edwards
-- 22/08/90 dave edwards - altered so searches for local resources first,
--                         only 1 initialisation routine to search for
--                         resource, resource+machine or just machine.
-- 04/09/90 dave edwards - made conformant to connection mgr spec as agreed
-- 04/10/90 dave edwards
--   updated to return error messages into lnkops_errorstring rather
--     than print out
-- 17/10/90 dave edwards - removed set & get filename, altered to look
--                         at env. variable ICONDB for filename
-- 08/11/90 dave edwards - 'vaxified'
-- 04/12/90 dave edwards - updated debug/info bits
-- 06/12/90 dave edwards - made search case insensitive
-- 31/01/91 dave edwards - made return BAD_OPERATION if database corrupt
-- 06/03/91 dave edwards - CM_GetLinkMethod returns int
-- 19/03/91 dave edwards - added sccs id stuff
-- 17/04/91 dave edwards - added error message if ICONDB too long
--
-----------------------------------------------------------------------------*/

/* CMSIDENTIFIER */
static char *CMS_Id = "LINKOPS_H:CONLIB_C.AAAA-FILE;1(15-JAN-92)";

#include <stdio.h>
#include <ctype.h>

#ifdef SUNOS
#include <strings.h>
#endif

#ifdef MSDOS
#include <string.h>
#endif

#ifdef VMS
#include <string.h>
#endif

#ifdef SOLARIS
#include <string.h>
#endif

#include "types.h"
#include "conlib.h"
#include "debug.h"
#include "opserror.h"
#include "lmethod.h"


/* prototypes for imported library bits*/
char *getenv ();

/* maximum length of a line */
#define MAXLINE 1024

/* string into which error messages written */
static char database_file[65] = NULSTRING;

/**
 ** imports
 **/
 
extern char *LMethodNames[];

char conlib_errorstring[1024] = NULSTRING;

/***
**** internal state variables (for inspector fn's)
***/
static enum MatchStates {MATCHlocalonly, MATCHremoteonly, MATCHlocalorremote};
static enum SearchStates {SEARCHnotinit, SEARCHnamedmachine, SEARCHanymachine, SEARCHjustmachine};
static enum SearchStates search_type;

static char state_resourcename[65] = "resource-init";             
static char state_isworking;
static char state_machinename[129] = "machine-init";
static char state_linkname[65] = "linkname-init";  
static int state_linkmethod;
static char state_mmsfile[65] = "mmsfile-init";
static char state_mmslink[65] = "mmslink-init";                
static char state_description[129] = "description-init";
                                 
/***
**** internal variables
***/
static char field_name[65] = "fieldname-init!!";
static char file_buffer[MAXLINE];
static FILE *file_id;
static BOOL show_errors = TRUE;

static char search_hostname [65] = "thishostmachine";
static char search_resourcename[65] = "dummyresource";
static char search_resourcelen = 0;
static char search_machinename[129]= "dummymachine";
static char search_machinelen = 0;                     
static short int search_pos = 0;     
static int search_pass = 0;
static int search_line = -1;
static unsigned char global_status;


/***
**** internal functions
***/


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
char ForceLower (x)
char x;
{
  /* force x to be lower case */
  if (isalpha (x) && isupper (x)) {
    return (tolower (x));
  } else {
    return (x);
  }
}

static void SkipWhiteSpace ()
{
  /* skip thru white spaces */
  while ((file_buffer[search_pos] == ' ') || (file_buffer[search_pos] == '\t')) {
    search_pos++;
  }
}

/* ReadPast :
     modifies : search_pos, file_buffer, global_status
     effects  : jumps past any continuation lines belonging
                to the current record line.
                if successfull returns global_status = STATUS_NOERROR,
                otherwise = STATUS_BAD_OPERATION
*/
static void ReadPast ()
{

  char x;
  BOOL look = TRUE;

  global_status = STATUS_NOERROR;
  (void)sprintf(conlib_errorstring, NULSTRING);
  while (look) {
    x = file_buffer[search_pos];
    switch (x) {
      case '*' : 
                 /* must find restarting continuation */
                 if (fgets (file_buffer, MAXLINE, file_id) == NULL) {
                   if (show_errors == TRUE) {
                     (void) sprintf (conlib_errorstring, "connection database, file [%s], at line[%d]\n -> premature end of file\n", database_file, search_line);
                     global_status = STATUS_BAD_OPERATION;
                   } else {
                     global_status = STATUS_ENDOFFILE;
                   }
                   return;
                 } else {
                   search_pos = 0;
                   search_line++;
                 }
                 SkipWhiteSpace ();
                 if (file_buffer[search_pos] != '*') {
                   (void) sprintf (conlib_errorstring, "connection database, file[%s], at line[%d]\n -> expecting continuation character\n", database_file, search_line);
                   global_status = STATUS_BAD_OPERATION;
                   return;
                 } else {
                   search_pos++; /* move 1 char past continuation */
                 }
                 break;
                 
      case '\n':
      case NUL :
                 (void) sprintf(conlib_errorstring, NULSTRING);
                 global_status = STATUS_NOERROR;       
                 return;
                 break;
                 
      default  : search_pos++;
                 break;
    }
  }
  
}

/* SkipToFieldSep -
     modifies : search_pos, file_buffer, global_status
     effects  : moves to 1 character after the next field separator (jumps
                thru comment lines & loads new data where appropriate)
                if successful, global_status = STATUS_NOERROR
                else, global_status = STATUS_BAD_OPERATION
*/
static void SkipToFieldSep ()
{
  char x;
  BOOL find_line;
  
  global_status = STATUS_NOERROR;
  (void) sprintf(conlib_errorstring, NULSTRING);
  
  find_line = TRUE;
  while (find_line == TRUE) {
    /* check field separator is ok */
    x = file_buffer[search_pos];
    switch (x) {
      case NUL  : 
        (void) sprintf (conlib_errorstring, "connection database, file[%s], at line[%d]\n -> premature end of file, looking for field {%s}\n", database_file, search_line, field_name); 
        global_status = STATUS_BAD_OPERATION;
        return;
        break;
                  
      case '\n' :
        if (fgets (file_buffer, MAXLINE, file_id) == NULL) {
          if (show_errors == TRUE) {
            global_status = STATUS_BAD_TARGET_NAME;
          } else {
            global_status = STATUS_ENDOFFILE;
          }
          return;
        } else {
          search_pos = 0;
          search_line++;
        }
        SkipWhiteSpace ();
        /* skip thru white spaces */
        if (file_buffer[search_pos] == '*') {
          (void) sprintf (conlib_errorstring, "connection database, file[%s], at line[%d]\n -> can't start a line with continuation, looking for field {%s}\n", database_file, search_line, field_name);
          global_status = STATUS_BAD_OPERATION;
          return;
        }
        break;
                  
      case '#' :
        if (fgets (file_buffer, MAXLINE, file_id) == NULL) {
          if (show_errors == TRUE) {
            global_status = STATUS_BAD_TARGET_NAME;
          } else {
            global_status = STATUS_ENDOFFILE;
          }
          return;
        } else {
          search_pos = 0;
          search_line++;
        }
        SkipWhiteSpace ();
        if (file_buffer[search_pos] == '*') {
          (void) sprintf (conlib_errorstring, "connection database, file[%s], at line[%d]\n -> can't start a line with continuation, looking for field {%s}\n", database_file, search_line, field_name);
          global_status = STATUS_BAD_OPERATION;
          return;
        }
        break;
      case '*' :
        /* read next line */
        if (fgets(file_buffer, MAXLINE, file_id) == NULL) {
          if (show_errors == TRUE) {
            (void) sprintf (conlib_errorstring, "connection database, file[%s], at line[%d]\n -> premature end of database file, looking for field {%s}\n", database_file, search_line, field_name);
            global_status = STATUS_BAD_OPERATION;
          } else {
            global_status = STATUS_ENDOFFILE;
          }
          return;
        } else {
          search_pos = 0;
          search_line++;
          find_line = FALSE;
        }
        SkipWhiteSpace ();
        
        /* must have a continuation character */
        if (file_buffer[search_pos] != '*') {
          (void) sprintf (conlib_errorstring, "connection database, file[%s], at line[%d]\n -> expecting continuation character at start of line, looking for field {%s}\n", database_file, search_line, field_name);
          global_status = STATUS_BAD_OPERATION;
          return;
        }
        break;
                  
      case '|':
        find_line = FALSE;
        break;
                  
      default :
        search_pos++;
        break;
    }
  }
  if ((x != '|') && (x != '*')) {       
    if ((x == '\n') || (x == NUL)) {
      (void) sprintf (conlib_errorstring, "connection database, file[%s], at line[%d]\n -> premature end of line, looking for field {%s}\n", database_file, search_line, field_name);
    } else {
      (void) sprintf (conlib_errorstring, "connection database, file[%s], at line[%d]\n -> bad field separator, looking for field {%s}\n", database_file, search_line, field_name);
    }
    global_status = STATUS_BAD_OPERATION;
    return;
  }
  search_pos++; /* skip over field separator */
  return;
}

/* CompareBufferTo -
     modifies : search_pos, file_buffer, global_status
     effects  : skips past any white space then tries to
                match 'compare_string' to the buffer.
                returns TRUE if matches, FALSE otherwise.
                if matches, the end field separator is
                checked.
                if successfull, global_status = STATUS_NOERROR,
                else = STATUS_BAD_OPERATION
                
*/
static BOOL CompareBufferTo (compare_string, compare_len, new_string)
char *compare_string;
int compare_len;
char *new_string;
{
  BOOL compare;
  char x;
  int i;
         
  global_status = STATUS_NOERROR;
  (void)sprintf(conlib_errorstring, NULSTRING);
  
  SkipToFieldSep();
  if (global_status != STATUS_NOERROR) {
    return (FALSE);
  }
  
  /* compare field, if matches will fall thru, if 
     doesn't match will return FALSE             */
  compare = TRUE;
  i = 0;
  while ((compare == TRUE) && (i <= compare_len)) {
    x = file_buffer[search_pos];
    if (ForceLower(x) != ForceLower (compare_string[i]) ) {
      /* no match */
      if ((x == '\n') || (x == NUL)) {
        (void) sprintf (conlib_errorstring, "connection database, file[%s], at line[%d]\n -> premature end of database file, looking for field {%s}\n", database_file, search_line, field_name);
        global_status = STATUS_BAD_OPERATION;
      }
      if ((i == 0) && ((x == '|') || (x == '*'))) {
        (void) sprintf (conlib_errorstring, "connection database, file[%s], at line[%d]\n -> field {%s} cannot be null\n", database_file, search_line, field_name);
        global_status = STATUS_BAD_OPERATION;
      }  
      return (FALSE);
    } else {
      new_string[i] = x;
      search_pos++;
     i++;
    }
  }
  new_string[i] = NUL;
  
  if (i < 1) {
    (void) sprintf (conlib_errorstring, "connection database, file[%s], at line[%d]\n -> field {%s} cannot be null\n", database_file, search_line, field_name);
    global_status = STATUS_BAD_OPERATION;
    return (FALSE);
  }
  
  /* all of supplied string has matched, go to end of field
     to ensure nothing but white spaces = full match      */
  SkipWhiteSpace ();
  
  /* now past white spaces, so next char should be a field
     separator  - if isnt string didn't match */
  x = file_buffer[search_pos];
  switch (x) {
    case '|' :
      return (TRUE);
      break;
      
    case '*' : 
      return (TRUE);
      break;
      
    default  :
      /* still is part of record = didn't match */
      if ((x == '\n') || (x == NUL)) {
        (void) sprintf (conlib_errorstring, "connection database, file[%s], at line[%d]\n -> premature end of database file, looking for field {%s}\n", database_file, search_line, field_name);
        global_status = STATUS_BAD_OPERATION;
      }
      return (FALSE);
      break;
  }
  return (FALSE); /* this statement will never be executed - its just to keep lint happy */
    
}

/* UnpackBoolFromBuffer ()
     modifies : search_pos, file_buffer, global_status
     effects  : skips past any white space & tries to read
                a boolean field value which is
                returned. the end of field separator is
                inspected.
                if successfull, global_status = STATUS_NOERROR,
                else = STATUS_BAD_OPERATION
*/
static BOOL UnpackBoolFromBuffer()
{

  char x;
  BOOL value;
  
  global_status = STATUS_NOERROR;
  (void)sprintf(conlib_errorstring, NULSTRING);
  
  SkipToFieldSep();
  if (global_status != STATUS_NOERROR) {
    return (FALSE);
  }
  
  /* read boolean field */
  x = file_buffer[search_pos];
  switch (x) {
    case 't' :
    case 'T' : search_pos++;
               value = TRUE;
               break;
               
    case 'f' : 
    case 'F' : search_pos++;
               value = FALSE;
               break;
                  
    default  :
               if ((x == '\n') || (x == NUL)) {
                 (void) sprintf (conlib_errorstring, "connection database, file[%s], at line[%d]\n -> premature end of line, looking for field {%s}\n", database_file, search_line, field_name);
               } else {
                 (void) sprintf (conlib_errorstring, "connection database, file[%s], at line[%d]\n -> illegal boolean value, looking for field {%s}\n", database_file, search_line, field_name);
               }
               global_status = STATUS_BAD_OPERATION;
               return (FALSE);
               break;
  }
  SkipWhiteSpace ();
  
  /* check field separator */
  x = file_buffer[search_pos];
  if ((x != '|') && (x != '*')) {
    if ((x == '\n') || (x == NUL)) {
      (void) sprintf (conlib_errorstring, "connection database, file[%s], at line[%d]\n -> premature end of line, looking for field {%s}\n", database_file, search_line);
    } else {
      (void) sprintf (conlib_errorstring, "connection database, file[%s], at line[%d]\n -> bad field separator, looking for field {%s}\n", database_file, search_line, field_name);
    }
    global_status = STATUS_BAD_OPERATION;
    return (FALSE);
  } else {
    return (value);
  }
  
}


/* UnpackStringFromBuffer ()
     modifies : search_pos, global_status
     effects  : skips past any white space.
                tries to read a string field value which is
                returned. the end of field separator is
                checked.
                if 'no_nulls' = TRUE, parsing a null string
                will result in failure.
                if successfull, global_status = STATUS_NOERROR,
                else = STATUS_BAD_OPERATION
*/
static void UnpackStringFromBuffer(string, string_len, no_nulls)
char string[];
int string_len;
BOOL no_nulls;
{

  int i, end_pos;
  char x;
  BOOL go;
  
  global_status = STATUS_NOERROR;
  (void)sprintf(conlib_errorstring, NULSTRING);
  
  SkipToFieldSep();
  if (global_status != STATUS_NOERROR) {
    return;
  }
  
  i = 0;
  x = file_buffer[search_pos];
  while ((x != '|') && (x != '*') && (x != NUL) && (i <= string_len)) {
    string[i] = x;
    search_pos++;
    i++;
    x = file_buffer[search_pos];
  }
  string[i] = NUL;
  
  /* strip off any non-significant right spaces */
  i--;
  end_pos = i;
  go = TRUE;
  while (go == TRUE) {
    if (i < 0) {
      go = FALSE;
      end_pos = (i + 1);
    } else {
      if ((string[i] != ' ') && (string[i] != '\t')) {
        end_pos = (i + 1);
        go = FALSE;
      }
    }
    i--;
  }
  string[end_pos] = NUL;                                   
  
  if ((no_nulls == TRUE) && (end_pos < 1)) {
    (void) sprintf (conlib_errorstring, "connection database, file[%s], at line[%d]\n -> field {%s} cannot be null\n", database_file, search_line, field_name);
    global_status = STATUS_BAD_OPERATION;
    return;
  }
  
  SkipWhiteSpace ();
  return;
}

/* UnpackCommentFromBuffer ()
     modifies : search_pos, file_buffer, global_status
     effects  : skips past any white space.
                reads a comment field from the buffer. reading
                is terminated by :
                  either reading 'string_len' characters,
                  reading a '*' or '|'
                  reading a NUL
                if successfull, global_status = STATUS_NOERROR,
                else = STATUS_BAD_OPERATION
*/
static void UnpackCommentFromBuffer(string, string_len)
char string[];
int string_len;
{

  BOOL go;
  int i, end_pos;
  char x;
  BOOL grab;
  
  global_status = STATUS_NOERROR;
  (void)sprintf(conlib_errorstring, NULSTRING);
  
  SkipToFieldSep();
  if (global_status != STATUS_NOERROR) {
    return;
  }
  
  grab = TRUE;
  i = 0;
  x = file_buffer[search_pos];
  while (grab) {
   if ((x == NUL) || (x == '|') || (x == '*') || (i > string_len)) {
     string[i] = NUL;
     grab = FALSE;
   } else {
     string[i] = x;
     search_pos++;
     i++;
     x = file_buffer[search_pos];
   }
  }
  if ((x != '*') && (x != '|')) {
    if (x == '\n') {
      (void) sprintf (conlib_errorstring, "connection database, file[%s], at line[%d]\n -> premature end of line, looking for field {%s}\n", database_file, search_line);
    } else {
      (void) sprintf (conlib_errorstring, "connection database, file[%s], at line[%d]\n -> bad field separator, looking for field {%s}\n", database_file, search_line, field_name);
    }
    global_status = STATUS_BAD_OPERATION;
    return;
  }
  
  /* strip off any non-significant right spaces */
  i--;
  end_pos = i;
  go = TRUE;
  while (go == TRUE) {
    if (i < 0) {
      go = FALSE;
      end_pos = (i + 1);
    } else {
      if ((string[i] != ' ') && (string[i] != '\t')) {
        end_pos = (i + 1);
        go = FALSE;
      }
    }
    i--;
  }
  string[end_pos] = NUL;                                   
  return;
}

static BOOL StringsEqual (s1, s2)
char *s1, *s2;
{
  int i = 0;
  char c1, c2;
  
  while (s2[i] != NUL) {
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
  
  while ((s1[i] == ' ') || (s1[i] == '\t')) {
    i++;
  }
  
  if (s1[i] != NUL) {
    return (FALSE);
  } else {
    if (i > 0) {
      return (TRUE);
    } else {
      return (FALSE);
    }
  }
}
  
static int ResolveLinkMethod (device_string)
char *device_string;
{
  BOOL resolved = FALSE;
  int i = 1;
  int method;
  
  global_status = STATUS_NOERROR;
  
  while (resolved == FALSE) {
  
    if (StringsEqual (device_string, LMethodNames[i]) == TRUE) {
      /* resolved ok */
      resolved = TRUE;
      method = i;
    } else {
      i++;
    }
    if (i == MAX_LMETHODS) {
      resolved = TRUE;
      global_status = STATUS_BAD_OPERATION;
      (void) sprintf (conlib_errorstring, "connection database, file[%s], at line[%d]\n -> illegal linkdev field - unknown method\n", database_file, search_line);
      method = 0;
    }
  }
  return (method);
}


static unsigned char GetOtherFields ()
{
  char tmp_linkdev[65];
  
  
  /* get other fields */
  (void) strcpy (field_name, "LinkName");
  UnpackStringFromBuffer (state_linkname, 64, TRUE);
  if (global_status != STATUS_NOERROR) {
    return (global_status);
  }
  
  /* read linkdev & resolve a numeric value */
  (void) strcpy (field_name, "LinkDev");
  UnpackStringFromBuffer (tmp_linkdev, 64, TRUE);     
  if (global_status != STATUS_NOERROR) {
    return (global_status);
  }
   
  (void) strcpy (field_name, "LinkDev");
  state_linkmethod = ResolveLinkMethod (tmp_linkdev);
  if (global_status != STATUS_NOERROR) {
    return (global_status);
  }
  
  (void) strcpy (field_name, "MMSFile");
  UnpackStringFromBuffer (state_mmsfile, 64, FALSE);
  if (global_status != STATUS_NOERROR) {
    return (global_status);
  }
  
  (void) strcpy (field_name, "MMSLink");
  UnpackStringFromBuffer (state_mmslink, 64, FALSE);     
  if (global_status != STATUS_NOERROR) {
    return (global_status);
  }
  
  (void) strcpy (field_name, "Description");
  UnpackCommentFromBuffer (state_description, 64);
  
  return (global_status);
}

static unsigned char OnePassSearch (match_state)
enum MatchStates match_state;
{
  BOOL found = FALSE;
  BOOL match;
  unsigned char result;
  
  while (found == FALSE) {
  
    /* READ DATABASE INTO BUFFER */
    if (fgets(file_buffer, MAXLINE, file_id) == NULL) {
      /* end of file - didnt find resource */
      if (show_errors == TRUE) {
        return (STATUS_BAD_TARGET_NAME);
      } else {
        return (STATUS_ENDOFFILE);
      }
      
    } else {
      search_pos = 0;
      search_line++;
    }
    (void) strcpy (field_name, "Resource");
    SkipWhiteSpace ();
    if (file_buffer[search_pos] == '*') {
      (void) sprintf (conlib_errorstring, "connection database, file[%s], at line[%d]\n -> can't start a line with a continutation\n", database_file, search_line);
      return(STATUS_BAD_OPERATION);
    }
  
    switch (search_type) {
      case SEARCHnamedmachine:
        /* COMPARE BOTH 'resource' & 'machine' FIELDS */
        match = CompareBufferTo(search_resourcename, search_resourcelen, state_resourcename);
        if (global_status != STATUS_NOERROR) {
          return (global_status);
        }
        
        if (match) {
          /* read isworking value */
          (void) strcpy (field_name, "IsWorking");
          state_isworking = UnpackBoolFromBuffer();
          if (global_status != STATUS_NOERROR) {
            return (global_status);
          }
          
          /* check machine name
           *
           * match against either a correct machine name, or
           * allow match against "localhost" if searching for local resources. (SIM 15/10/92)
           */
          (void) strcpy (field_name, "Machine");
          UnpackStringFromBuffer(state_machinename, 64, TRUE);
          if (global_status != STATUS_NOERROR) {
            return (global_status);
          }
          match = ( StringCaseCmp (search_machinename, state_machinename) == TRUE) ||
                  ((StringCaseCmp (state_machinename, "localhost") == TRUE) &&
                   (StringCaseCmp (search_machinename, search_hostname) == TRUE));
          
          if (match) {
            /* succesfully located */
            result = GetOtherFields ();
            if (result != STATUS_NOERROR) {
              return (result);
            }
            
            if ((StringCaseCmp (state_machinename, "localhost") == FALSE) && (StringCaseCmp (state_machinename, search_hostname) == FALSE )) {
              /* Resource is remote */
              switch (match_state) {
                case MATCHremoteonly:
                case MATCHlocalorremote:
                  /* ok to match against this resource */
                  found = TRUE;
                  break;
                  
                case MATCHlocalonly:
                  /* cant access resource as looking for local only */
                  /* skip past any continuation text (if there is any) */
                  ReadPast ();
                  if (global_status != STATUS_NOERROR) {
                    return (global_status);
                  } 
                  break;
              }
            
            } else {
              /* Resource is local */
              switch (match_state) {
                case MATCHremoteonly:
                  /* cant access local resource as looking for local only */
                  /* skip past any continuation text (if there is any) */
                  ReadPast ();
                  if (global_status != STATUS_NOERROR) {
                    return (global_status);
                  }
                  break;
                default:
                  /* ok to match against this resource */
                  found = TRUE;
                  break;
              }
            } 
            
          } else {
            /* skip past any continuation text (if there is any) */
            ReadPast ();
            if (global_status != STATUS_NOERROR) {
              return (global_status);
            }
          }
        } else {
          /* skip past any continuation text (if there is any) */
          ReadPast ();
          if (global_status != STATUS_NOERROR) {
            return (global_status);
          }
        }
        break;
        
      case SEARCHanymachine:
        /* COMPARE JUST 'resource' FIELD */
        match = CompareBufferTo(search_resourcename, search_resourcelen, state_resourcename);
        if (global_status != STATUS_NOERROR) {
          return (global_status);
        }
        
        if (match) {
          /* read isworking value */
          (void) strcpy (field_name, "IsWorking");
          state_isworking = UnpackBoolFromBuffer();
          if (global_status != STATUS_NOERROR) {
            return (global_status);
          }
          
          /* get machine name */
          (void) strcpy (field_name, "Machine");
          UnpackStringFromBuffer(state_machinename, 64, TRUE);
          if (global_status != STATUS_NOERROR) {
            return (global_status);
          }
          
          result = GetOtherFields ();
          if (result != STATUS_NOERROR) {
            return (result);
          }
          
          if ((StringCaseCmp (state_machinename, "localhost") == FALSE) && (StringCaseCmp (state_machinename, search_hostname) == FALSE )) {
            switch (match_state) {
              case MATCHremoteonly:
              case MATCHlocalorremote:
                /* ok to match against this resource */
                found = TRUE;
                break;
                
              case MATCHlocalonly:
                /* cant access tcp resource as looking for local only */
                /* skip past any continuation text (if there is any) */
                ReadPast ();
                if (global_status != STATUS_NOERROR) {
                  return (global_status);
                } 
                break;
            }
          } else {
            switch (match_state) {
              case MATCHremoteonly:
                /* cant access local resource as looking for local only */
                /* skip past any continuation text (if there is any) */
                ReadPast ();
                if (global_status != STATUS_NOERROR) {
                  return (global_status);
                }
                break;
              default:
                /* ok to match against this resource */
                found = TRUE;
            }
          }
          
        } else {
          /* skip past any continuation text (if there is any) */
          ReadPast ();
          if (global_status != STATUS_NOERROR) {
            return (global_status);
          }
        }
        break;
        
      case SEARCHjustmachine:
        /* read resource value */
        UnpackStringFromBuffer (state_resourcename, 64, TRUE);
        if (global_status != STATUS_NOERROR) {
          return (global_status);
        }
        
        /* read isworking value */
        (void) strcpy (field_name, "IsWorking");
        state_isworking = UnpackBoolFromBuffer();
        if (global_status != STATUS_NOERROR) {
          return (global_status);
        }
          
        /* check machine name */
        (void) strcpy (field_name, "Machine");
        match = CompareBufferTo(search_machinename, search_machinelen, state_machinename);
        if (global_status != STATUS_NOERROR) {
          return (global_status);
        }
        if (match) {
          /* succesfully located */
          found = TRUE;
          result = GetOtherFields ();
          if (result != STATUS_NOERROR) {
            return (result);
          }
        } else {
          /* skip past any continuation text (if there is any) */
          ReadPast ();
          if (global_status != STATUS_NOERROR) {
            return (global_status);
          }
        }
        break;
        
      default:
        (void) sprintf (conlib_errorstring, " : module [conlib.c], function [CM_OnePassSearch]\n -> not initialised correctly\n");
        return (STATUS_BAD_OPERATION);
        break; 
    }
  }
  return (STATUS_NOERROR);
}

/***
**** exported functions
***/

/* CM_InitSearch -
     requires  : not already initialised
     effects   : sets up the Connection Manager library ready to perform a
                 search for a resource. returns a linkops error code :
                 either STATUS_BAD_OPERATION or STATUS_NO_ERROR
*/
unsigned char CM_InitSearch (resource, machine, hostname)
char *resource;
char *machine;
char *hostname;
{

  BOOL resource_isnull, machine_isnull, hostname_isnull;
  char *database_ptr;
  
  search_line = 0;
  
  switch (search_type) {
    case SEARCHnotinit:
      break;
      
    case SEARCHanymachine :
    case SEARCHnamedmachine:
    case SEARCHjustmachine:
      (void) sprintf (conlib_errorstring, " : module [conlib.c], function [CM_InitSearch]\n -> already initialised\n");
      return (STATUS_BAD_OPERATION);
      break;
      
    default:
      (void) sprintf (conlib_errorstring, " : module [conlib.c], function [CM_InitSearch]\n -> panic! bad search_type\n");
      return (STATUS_BAD_OPERATION);
      break;
  }
  
  if ((resource == NULL) || (*resource == NUL)) {
    resource_isnull = TRUE;
  } else {
    resource_isnull = FALSE;
  }
  
  if ((machine == NULL) || (*machine == NUL)) {
    machine_isnull = TRUE;
  } else {
    machine_isnull = FALSE;
  }
  
  if ((hostname == NULL) || (*hostname == NUL)) {
    hostname_isnull = TRUE;
  } else {
    hostname_isnull = FALSE;
  }
    
  if ((resource_isnull == FALSE) && (machine_isnull == FALSE)) {                                  
    DebugMessage ( fprintf (stderr, "Debug       : search_type = SEARCHnamedmachine\n") );
    search_type = SEARCHnamedmachine;
    (void) strcpy (search_resourcename, resource);
    search_resourcelen = strlen(resource) - 1;
    (void) strcpy (search_machinename, machine);
    search_machinelen = strlen(machine) - 1;     
  } else {
    
    if ((resource_isnull == FALSE) && (machine_isnull == TRUE)) {
      DebugMessage ( fprintf (stderr, "Debug       : search_type = SEARCHanymachine\n") );
      search_type = SEARCHanymachine;
      (void) strcpy (search_resourcename, resource);
      search_resourcelen = strlen(resource) - 1;
    } else {
  
      if ((resource_isnull == TRUE) && (machine_isnull == FALSE)) {
        DebugMessage ( fprintf (stderr, "Debug       : search_type = SEARCHjustmachine\n") );
        search_type = SEARCHjustmachine;
        (void) strcpy (search_machinename, machine);
        search_machinelen = strlen(machine) - 1;
      } else {
        (void) sprintf (conlib_errorstring, " : module [conlib.c], function [CM_InitSearch]\n -> panic! resource & machine names are blank\n");
        return (STATUS_BAD_OPERATION);
      }
    }
  }
  
  if ((search_type != SEARCHjustmachine) && (hostname_isnull == TRUE)) {
    (void) sprintf (conlib_errorstring, " : module [conlib.c], function [CM_InitSearch]\n -> hostname is blank\n");
    return (STATUS_BAD_OPERATION);
  } else {
    (void) strcpy (search_hostname, hostname);
  }
  
  database_ptr = (char *)getenv ("ICONDB");
  if (database_ptr == (char *)NULL) {
    (void) sprintf (conlib_errorstring, " : no environment variable ICONDB\n");
    return (STATUS_BAD_OPERATION);
  } else {
    if (strlen(database_ptr) > 64) {
      (void) sprintf (conlib_errorstring, " : ICONDB too long (>64 chars)\n");
      return (STATUS_BAD_OPERATION);
    } else {
      (void) strncpy (database_file, database_ptr, 64);
    }
  }
  
  /* OPEN FILE */
  file_id = fopen (database_file, "r");
  if (file_id == NUL) {
    (void) sprintf (conlib_errorstring, " : can't open connection database file [%s]\n", database_file);
    search_type = SEARCHnotinit;
    return (STATUS_BAD_OPERATION);
  } else {
    search_pos = 0;
    search_pass = 1;
    show_errors = TRUE;
  }
  return (STATUS_NOERROR);
}


/* CM_EndSearch -
     requires : search initialised
     effects  : ends search.
*/
void CM_EndSearch ()
{
  if (search_type == SEARCHnotinit) {       
    (void) sprintf (conlib_errorstring, " : module [conlib.c], function [CM_EndSearch]\n -> not initialised\n");
  } else {
    (void) fclose(file_id);
    search_type = SEARCHnotinit;
    (void) strncpy (field_name, "fieldname-end", 65); 
    
    (void) strncpy (state_resourcename, "resource-end", 65);             
    (void) strncpy (state_machinename,  "machine-end", 129);
    (void) strncpy (state_resourcename, "resource-end", 65);
    (void) strncpy (state_machinename, "machine-end", 129);
    (void) strncpy (state_mmsfile, "mmsfile-end", 65);
    (void) strncpy (state_mmslink, "mmslink-end", 65);
    (void) strncpy (state_description, "description-end", 129);
    search_type  = SEARCHnotinit;
    search_pos = 0;     
    search_pass = 0;
    global_status = STATUS_NOERROR;
  }
}

/* CM_DoSearch -
     effects : tries to locate a resource in the database file.
               returns a linkops error code.
*/
unsigned char CM_DoSearch ()
{

  unsigned char result;
  
  switch (search_type) {
    case SEARCHanymachine:
    case SEARCHnamedmachine:
      switch (search_pass) {
        case 1:
          /* do 1st pass, looking at local resources only */
          show_errors = FALSE;
          DebugMessage ( fprintf (stderr, "Debug       : searching for local resources only\n") );
          result = OnePassSearch (MATCHlocalonly);
          if (result == STATUS_ENDOFFILE) {
            /* do 2nd pass, looking at remote resources only */
            show_errors = TRUE;
            search_pass = 2;
            /* move back to start of file */
            rewind(file_id);
            search_line = 0;
            DebugMessage ( fprintf (stderr, "Debug       : searching for remote resources only\n") );
            result = OnePassSearch (MATCHremoteonly);
            if (result == STATUS_ENDOFFILE) {
              return (STATUS_BAD_TARGET_NAME);
            } else {
              return (result);
            }
            
          } else {
            return (result);
          }
          break;
          
        case 2:
          /* do 2nd pass, looking at remote resources only */
          show_errors = TRUE;
          DebugMessage ( fprintf (stderr, "Debug       : searching for remote resources only\n") );
          result = OnePassSearch (MATCHremoteonly);
          if (result == STATUS_ENDOFFILE) {
            return (STATUS_BAD_TARGET_NAME);
          } else {
            return (result);
          }
          break;
        
        default:
          (void) sprintf (conlib_errorstring, " : module [conlib.c], function [CM_DoSearch]\n -> panic! bad search_pass value\n");
          return (STATUS_BAD_OPERATION);
          break;
      }
      break;
    
    case SEARCHjustmachine:
      switch (search_pass) {
        case 1:
          show_errors = TRUE;
          DebugMessage ( fprintf (stderr, "Debug       : searching for local or remote resources\n") ); 
          result = OnePassSearch (MATCHlocalorremote);
          if (result == STATUS_ENDOFFILE) {
            return (STATUS_BAD_TARGET_NAME);
          } else {
            return (result);
          }
          break;
         
        default: 
          (void) sprintf (conlib_errorstring, " : module [conlib.c], function [CM_DoSearch]\n -> panic! bad search_pass value\n");
          return (STATUS_BAD_OPERATION);
          break;
      }
      break;
      
   default:
     (void) sprintf (conlib_errorstring, " : module [conlib.c], function [CM_DoSearch]\n -> panic! bad search_pass value\n");
     return (STATUS_BAD_OPERATION);
     break;
  }
  return (STATUS_BAD_OPERATION); /* this is never executed - just to keep lint happy */
      
}
      
  
      
int CM_GetLineNumber ()
{
  return (search_line);
}

void CM_GetResource (resource_name)
char *resource_name;
{
  (void) strcpy (resource_name, state_resourcename);
}

BOOL CM_GetIsWorking ()
{
  return state_isworking;
}

void CM_GetMachine (machine_name)
char *machine_name;
{
  (void) strncpy (machine_name, state_machinename, 128);
}

void CM_GetLinkName (link_name)
char *link_name;
{
  (void) strncpy (link_name, state_linkname, 64);
}

int CM_GetLinkMethod ()
{
  return (state_linkmethod);
}

void CM_GetMMSLink (mms_linkname)
char *mms_linkname;
{
  (void) strcpy (mms_linkname, state_mmslink);
}

void CM_GetMMSFile (mms_filename)
char *mms_filename;
{
  (void) strncpy (mms_filename, state_mmsfile, 65);
}

void CM_GetDescription (description)
char *description;
{
  (void) strncpy (description, state_description, 128);
}

