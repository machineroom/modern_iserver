/*------------------------------------------------------------------------------
--
-- CMSIDENTIFIER
-- PRODUCT:ITEM.VARIANT-TYPE;0(DATE) 
--
-- File : linkops.h
--     linkops host module 
--
-- Copyright 1990 INMOS Limited
--
-- Date    : 3/26/91
-- Version : 1.2
--
-- 7/06/90 dave edwards
--   originated
--
------------------------------------------------------------------------------*/


enum ErrorModes {ERRORdetect, ERRORignore};
enum CommsModes {COMMSclosed, COMMSasynchronous, COMMSsynchronous};

/* use to obtain error message */
extern char lnkops_errorstring[1024];

/* link communications mode */
extern enum CommsModes OPScommsmode;

#define TSERIES_PROCESSORID 0L

#define ONESECOND 1000000L

#define SPMAXPACKET 4096
#define SPMAXHEADERVALUE (SPMAXPACKET - 2)

/* prototypes */
extern unsigned char OPS_Open ();
extern unsigned char OPS_Close ();
extern unsigned char OPS_Restart ();
extern unsigned char OPS_CommsAsynchronous ();
extern unsigned char OPS_CommsSynchronous ();
extern unsigned char OPS_ErrorDetect ();
extern unsigned char OPS_ErrorIgnore ();
extern unsigned char OPS_BootWrite ();
extern unsigned char OPS_Reset ();
extern unsigned char OPS_Analyse ();
extern unsigned char OPS_Poke16 ();
extern unsigned char OPS_Poke32 ();
extern unsigned char OPS_Peek16 ();
extern unsigned char OPS_Peek32 ();
extern unsigned char OPS_GetRequest ();
extern unsigned char OPS_SendReply ();


