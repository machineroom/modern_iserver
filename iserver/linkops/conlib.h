/*-----------------------------------------------------------------------------
--
-- CMSIDENTIFIER
-- PRODUCT:ITEM.VARIANT-TYPE;0(DATE) 
--
-- File : conlib.h
--
--  host connection manager library
--   -> searches connection library database for particular resources
--
-- Copyright 1990, 1991 INMOS Limited
--
-- Date    : 3/26/91
-- Version : 1.2
--
--  8/10/90 dave edwards - originated
-- 17/10/90 dave edwards - removed set & get filename
-- 06/03/91 dave edwards - CM_GetLinkMethod returns int
--
-----------------------------------------------------------------------------*/

#include <stdbool.h>

/* use to obtain error message */
extern char conlib_errorstring[1024];

/* prototypes */
extern unsigned char CM_InitSearch ();
extern void CM_EndSearch ();
extern unsigned char CM_DoSearch ();
extern int CM_GetLineNumber ();
extern void CM_GetResource ();
extern bool CM_GetIsWorking ();
extern void CM_GetMachine ();
extern void CM_GetLinkName ();
extern int CM_GetLinkMethod ();
extern void CM_GetMMSLink ();
extern void CM_GetMMSFile ();
extern void CM_GetDescription ();
