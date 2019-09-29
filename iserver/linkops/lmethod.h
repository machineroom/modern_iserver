/*-----------------------------------------------------------------------------
--
-- CMSIDENTIFIER
-- PRODUCT:ITEM.VARIANT-TYPE;0(DATE) 
--
-- File    : lmethod.h
--  host connection manager link access method constants
--
-- Copyright 1990, 1991 INMOS Limited
--
-- Date    : 3/26/91
-- Version : 1.2
--
-- 22/05/90 - dave edwards
--   originated
-- 06/03/91 - dave edwards
--   used old board id values from very old iserver.h
--
-----------------------------------------------------------------------------*/

/* number of supported methods */
#define MAX_LMETHODS 17

/* define lmethod constants, values up to 127 reserved for INMOS */
#define HW_X      0
#define B004      1
#define B008      2 
#define B010      3
#define B011      4
#define B014      5
#define DRX11     6
#define QT0       7
#define B015      8
#define IBM_CAT   9
#define B016      10
#define UDP       11     
#define TCP_LINKOPS 12
#define B017      13  
#define TSP       14  
#define TD	  15
#define C011	  16

