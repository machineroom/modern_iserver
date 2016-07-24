/*------------------------------------------------------------------------------
--
-- File : debug.c
--     debug output routines
--
-- Copyright 1990, 1991 INMOS Limited
--
-- Date    : 3/26/91
-- Version : 1.2
--
-- 11/06/90 dave edwards
-- 04/12/90 dave edwards - altered debug/info bits to use macro in header file
-- 19/03/91 dave edwards - added sccs id stuff
--
------------------------------------------------------------------------------*/

/* CMSIDENTIFIER */
static char *CMS_Id = "PRODUCT:ITEM.VARIANT-TYPE;0(DATE)";

#include <stdio.h>

#include "types.h"

BOOL debug_davee_lux = FALSE;
BOOL info_davee_lux = FALSE;

void SetDebugMode (value)
BOOL value;
{
  debug_davee_lux = value;
}

BOOL GetDebugMode ()
{
  return (debug_davee_lux);
}

void SetInfoMode (value)
BOOL value;
{
  info_davee_lux = value;
}

BOOL GetInfoMode ()
{
  return (info_davee_lux);
}

