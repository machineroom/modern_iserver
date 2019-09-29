/*-----------------------------------------------------------------------------
--
-- File    : lmethod.c
--
-- fixed table of link access method names & values
--  implemented as a C module rather than header
--  as several modules need to use it.
--
-- Copyright 1990, 1991 INMOS Limited
--
-- Date    : 3/26/91
-- Version : 1.2
--
-- 5/06/90 dave edwards
--   originated
-- 06/03/91 - dave edwards
--  altered to use old board id values from very old iserver.h
-- 19/03/91 dave edwards 
--  added sccs id stuff
--
-----------------------------------------------------------------------------*/

/* CMSIDENTIFIER */
static char *CMS_Id = "PRODUCT:ITEM.VARIANT-TYPE;0(DATE)";

#include "lmethod.h"

/* define strings found in connection database file */
char *LMethodNames[] = {
  "undefined", /* HW_x */
  "b004",      /* b004 */
  "b008",      /* b008 */
  "b010",      /* b010 */          
  "b011",      /* b011 */          
  "b014",      /* b014 */
  "drx11",     /* drx11 */
  "qt0",       /* qt0  */
  "b015",      /* b015 */          
  "ibm_cat",   /* IBM_CAT */          
  "b016",      /* b016 */             
  "udp",       /* udp !*/
  "tcp",       /* tcp  */
  "b017",      /* b017 */
  "tsp",        /* TSP */
  "td",
  "c011"        /* rPI/C011 */
};
