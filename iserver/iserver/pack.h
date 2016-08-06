/* Copyright INMOS Limited 1988,1990,1991 */

/* CMSIDENTIFIER */
/* PRODUCT:ITEM.VARIANT-TYPE;0(DATE) */

/* Useful macros */

/* External variables */

extern unsigned char *InBuf;
extern unsigned char *OutBuf;
extern int OutCount;

/* Functions */

extern void put_8(unsigned char val);
extern void put_16(unsigned int val);
extern void put_32(unsigned long val);
extern void put_slice(unsigned int len, unsigned char *buff);
extern void put_count(unsigned int len);

extern unsigned char get_8(void);
extern unsigned int get_16(void);
extern unsigned long get_32(void);
extern unsigned int get_slice(unsigned char *buff);

extern char *pack32(unsigned long int val, char *buff);

extern unsigned long unpack32(char *buff);

