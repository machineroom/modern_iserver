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
#include "debug.h"

bool debug_davee_lux = false;
bool info_davee_lux = false;

void SetDebugMode (value)
bool value;
{
  debug_davee_lux = value;
}

bool GetDebugMode ()
{
  return (debug_davee_lux);
}

void SetInfoMode (value)
bool value;
{
  info_davee_lux = value;
}

bool GetInfoMode ()
{
  return (info_davee_lux);
}

