/*------------------------------------------------------------------------------
--
-- CMSIDENTIFIER
-- PRODUCT:ITEM.VARIANT-TYPE;0(DATE) 
--
-- File    : tops.h
--     tcp implementation of linkops protocol via BSD sockets
--
-- Copyright 1990 INMOS Limited
--
-- Date    : 3/26/91
-- Version : 1.2
--
-- 08/10/90 - dave edwards
--
------------------------------------------------------------------------------*/

extern unsigned char TOPS_Open ();
extern unsigned char TOPS_Close ();
extern unsigned char TOPS_CommsAsynchronous();
extern unsigned char TOPS_CommsSynchronous ();
extern unsigned char TOPS_ErrorDetect ();
extern unsigned char TOPS_ErrorIgnore ();
extern unsigned char TOPS_BootWrite (); 
extern unsigned char TOPS_Reset ();
extern unsigned char TOPS_Analyse ();
extern unsigned char TOPS_GetRequest ();
extern unsigned char TOPS_SendReply ();
extern unsigned char TOPS_Poke16 ();
extern unsigned char TOPS_Poke32 ();
extern unsigned char TOPS_Peek16 ();
extern unsigned char TOPS_Peek32 ();
extern unsigned char TOPS_Restart ();
