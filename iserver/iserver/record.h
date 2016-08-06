/* Copyright INMOS Limited 1988,1990,1991 */

/* CMSIDENTIFIER */
/* PRODUCT:ITEM.VARIANT-TYPE;0(DATE) */

/* Macros */

#define MAX_CHUNK_SIZE     16384

/* External variables */

/* Functions */

extern void SpOpenRec(void);
extern void SpPutRec(void);
extern void SpGetRec(void);
extern void SpPutEOF(void);

extern int form_seq_seek(long fileid, long offset, long origin);
extern int unform_seq_seek(long fileid, long offset, int origin);

