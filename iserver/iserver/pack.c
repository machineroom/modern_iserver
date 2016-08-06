/* Copyright INMOS Limited 1990 */

/* CMSIDENTIFIER */
static char *CMS_Id = "PRODUCT:ITEM.VARIANT-TYPE;0(DATE)";

#include "server.h"
#include "iserver.h"
#include "pack.h"

unsigned char *InBuf;
unsigned char *OutBuf;
int OutCount;

void put_8(val)
unsigned char val;
{
   *OutBuf++ = (char) val;
   OutCount++;
}

/* Pack the lower 16-bits of val */
void put_16(val)
unsigned int val;
{
   register unsigned char *cp;
   
#ifdef BIG_ENDIAN
   cp = (unsigned char *)&val+sizeof(int)-1;

   *OutBuf++ = *cp--;
   *OutBuf++ = *cp;
#else
   cp = (unsigned char *)&val;
   
   *OutBuf++ = *cp++;
   *OutBuf++ = *cp;
#endif /* BIG_ENDIAN */

   OutCount += 2;
}

/* Pack all of val */
void put_32(val)
unsigned long val;
{
   register unsigned char *cp;
   
#ifdef BIG_ENDIAN
   cp = (unsigned char *)&val+sizeof(long)-1;

   *OutBuf++ = *cp--;
   *OutBuf++ = *cp--;
   *OutBuf++ = *cp--;
   *OutBuf++ = *cp;
#else
   cp = (unsigned char *)&val;
   
   *OutBuf++ = *cp++;
   *OutBuf++ = *cp++;
   *OutBuf++ = *cp++;
   *OutBuf++ = *cp;
#endif /* BIG_ENDIAN */

   OutCount += 4;
}

/* Set the length field of the output packet */
void put_count(len)
unsigned int len;
{
   register unsigned char *cp;
   
   if (len & 1)
      len++;      /* Make length even no of bytes */
   
#ifdef BIG_ENDIAN
   cp = (unsigned char *)&len+sizeof(int)-1;
   
   Tbuf[0] = *cp--;
   Tbuf[1] = *cp;
#else
   cp = (unsigned char *)&len;
   
   Tbuf[0] = *cp++;
   Tbuf[1] = *cp;
#endif /* BIG_ENDIAN */
}

/* Pack bottom 16-bits of len and then len bytes of buff */
void put_slice(len, buff)
unsigned int len;
unsigned char *buff;
{
   register unsigned char *cp;
   int i;
   
#ifdef BIG_ENDIAN
   cp = (unsigned char *)&len+sizeof(int)-1;
   
   *OutBuf++ = *cp--;
   *OutBuf++ = *cp--;
#else
   cp = (unsigned char *)&len;
   
   *OutBuf++ = *cp++;
   *OutBuf++ = *cp++;
#endif /* BIG_ENDIAN */

   for (i=0; i<len; i++)
      *OutBuf++ = *buff++;
      
   OutCount += len+2;
}

unsigned char get_8(void)
{
   return *InBuf++;
}

unsigned int get_16(void)
{
   int                    res;
   register unsigned char *cp;
   
#ifdef BIG_ENDIAN
   res = *InBuf++;
   res += 256*(*InBuf++);
#else
   res = 0;
   cp = (unsigned char *)&res;
   
   *cp++ = *InBuf++;
   *cp++ = *InBuf++;
#endif /* BIG_ENDIAN */

   return res;
}

unsigned long get_32(void)
{
   long                   res=0L;
   register unsigned char *cp;
   
#ifdef BIG_ENDIAN
   cp = (unsigned char *)&res+sizeof(long)-1;
   
   *cp-- = *InBuf++;
   *cp-- = *InBuf++;
   *cp-- = *InBuf++;
   *cp-- = *InBuf++;
#else
   cp = (unsigned char *)&res;
   
   *cp++ = *InBuf++;
   *cp++ = *InBuf++;
   *cp++ = *InBuf++;
   *cp++ = *InBuf++;
#endif /* BIG_ENDIAN */

   return res;
}

unsigned int get_slice(buff)
unsigned char *buff;
{
   int                    len;
   register unsigned char *cp;
   int                    i;
   
#ifdef BIG_ENDIAN
   len = *InBuf++;
   len += 256*(*InBuf++);
#else
   len = 0;
   cp = (unsigned char *)&len;
   
   *cp++ = *InBuf++;
   *cp++ = *InBuf++;
#endif /* BIG_ENDIAN */

   for (i=0; i<len; i++)
   *buff++ = *InBuf++;

   return len;
}

char *pack32(val, buff)
unsigned long int val;
char *buff;
{
   register char *cp;
   
#ifdef BIG_ENDIAN
   cp = (char *)&val+sizeof(long)-1;

   *buff++ = *cp--;
   *buff++ = *cp--;
   *buff++ = *cp--;
   *buff++ = *cp;
#else
   cp = (char *)&val;
   
   *buff++ = *cp++;
   *buff++ = *cp++;
   *buff++ = *cp++;
   *buff++ = *cp;
#endif /* BIG_ENDIAN */

   return buff;
}

unsigned long unpack32(buff)
char *buff;
{
   long                   res=0L;
   register unsigned char *cp;
   
#ifdef BIG_ENDIAN
   cp = (unsigned char *)&res+sizeof(long)-1;
   
   *cp-- = *buff++;
   *cp-- = *buff++;
   *cp-- = *buff++;
   *cp-- = *buff++;
#else
   cp = (unsigned char *)&res;
   
   *cp++ = *buff++;
   *cp++ = *buff++;
   *cp++ = *buff++;
   *cp++ = *buff++;
#endif /* BIG_ENDIAN */

   return res;
}
