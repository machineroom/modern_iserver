/* Copyright INMOS Limited 1988,1990,1991 */

/* CMSIDENTIFIER */
/* PRODUCT:ITEM.VARIANT-TYPE;0(DATE) */

/* Useful macros */

/* External variables */

extern unsigned char *InBuf;
extern unsigned char *OutBuf;
extern int OutCount;

/* Functions */

#ifdef PROTOTYPES
#else
extern void put_8();
extern void put_16();
extern void put_32();
extern void put_slice();
extern void put_count();

extern unsigned char get_8();
extern unsigned int get_16();
extern unsigned long get_32();
extern unsigned int get_slice();

extern char *pack32();

extern unsigned long unpack32();
#endif /* PROTOTYPES */
