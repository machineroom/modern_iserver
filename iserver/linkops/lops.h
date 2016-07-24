/*------------------------------------------------------------------------------
--
-- CMSIDENTIFIER
-- PRODUCT:ITEM.VARIANT-TYPE;0(DATE) 
--
-- File    : lops.h
--     linkops functions implemented using (meta) linkio library
--
-- Copyright 1990 INMOS Limited
--
-- Date    : 3/26/91
-- Version : 1.2
--
-- 08/10/90 - dave edwards
--
------------------------------------------------------------------------------*/

extern unsigned char LOPS_Open ();
extern unsigned char LOPS_Close ();
extern unsigned char LOPS_CommsAsynchronous();
extern unsigned char LOPS_CommsSynchronous ();
extern unsigned char LOPS_ErrorDetect ();
extern unsigned char LOPS_ErrorIgnore ();
extern unsigned char LOPS_BootWrite (); 
extern unsigned char LOPS_Reset ();
extern unsigned char LOPS_Analyse ();
extern unsigned char LOPS_GetRequest ();
extern unsigned char LOPS_SendReply ();
extern unsigned char LOPS_Poke16 ();
extern unsigned char LOPS_Poke32 ();
extern unsigned char LOPS_Peek16 ();
extern unsigned char LOPS_Peek32 ();
extern unsigned char LOPS_Restart ();
