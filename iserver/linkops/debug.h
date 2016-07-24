/*------------------------------------------------------------------------------
--
-- CMSIDENTIFIER
-- PRODUCT:ITEM.VARIANT-TYPE;0(DATE) 
--
-- File : debug.h
--     debug output routines
--
-- Copyright 1990 INMOS Limited
--
-- Date    : 3/26/91
-- Version : 1.2
--
-- 08/10/90 dave edwards - originated
-- 05/12/90 dave edwards - altered to include macros
--
------------------------------------------------------------------------------*/

/* needs types.h definition of BOOL */

extern BOOL debug_davee_lux;
extern BOOL info_davee_lux;

#define InternalDebugMessage(x) if (debug_davee_lux) { x; }
#define InternalInfoMessage(x) if (info_davee_lux) { x; }

#ifdef MES
#define DebugMessage(x) InternalDebugMessage(x)
#else
#define DebugMessage(x) /* donothing */
#endif

#ifdef MES
#define InfoMessage(x) InternalInfoMessage(x)
#else
#define InfoMessage(x) /* donothing */
#endif

#ifdef MES
#define ErrorMessage(x) { x; }
#else
#define ErrorMessage(x) /* donothing */
#endif

extern void SetDebugMode ();
extern BOOL GetDebugMode ();
extern void SetInfoMode ();
extern BOOL GetInfoMode (); 

