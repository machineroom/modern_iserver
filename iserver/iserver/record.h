/* Copyright INMOS Limited 1988,1990,1991 */

/* CMSIDENTIFIER */
/* PRODUCT:ITEM.VARIANT-TYPE;0(DATE) */

/* Macros */

#define MAX_CHUNK_SIZE     16384

/* External variables */

/* Functions */

#ifdef PROTOTYPES
#else
extern void SpOpenRec();
extern int form_seq_seek();
extern int unform_seq_seek();
#endif /* PROTOTYPES */
