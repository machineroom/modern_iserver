/* Copyright INMOS Limited 1988,1990,1991 */
/* CMSIDENTIFIER */
/* PRODUCT:ITEM.VARIANT-TYPE;0(DATE) */

#ifndef _IMS_BCMD_H_

#define _IMS_BCMD_H_

#ifdef MSDOS
struct IMS_IO {
   short   io_val;
   short   io_op;
};
#else
#include <sys/ioccom.h>
/*
 * I/O controls
 */
struct IMS_SETF {
   unsigned int    op:16;
   unsigned int    val:16;
};

struct IMS_READF {
   unsigned int    reserved:28;
   unsigned int    read_f:1;
   unsigned int    write_f:1;
   unsigned int    timeout_f:1; /* not used */
   unsigned int    error_f:1;
};

union IMS_IO {
   struct IMS_SETF set;
   struct IMS_READF status;
};

#define RESET              (1)    /* Reset the transputer   */
#define ANALYSE            (2)    /* Analyse the transputer */
#define SETTIMEOUT         (3)    /* Set the timeout        */
#define SETERRORSIGNAL     (4)    /* Set a signal for Error */
#define RESETERRORSIGNAL   (5)    /* Disable Error signals  */
#define SETREADDMA         (6)    /* Use DMA for reads      */
#define SETWRITEDMA        (7)    /* Use DMA for writes     */
#define RESETDMA           (8)    /* Turn off DMA           */
#define SENSEREAD          (9)    /* Sense Error on read    */
#define SENSEWRITE         (10)   /* Sense Error on write   */
#define IGNREAD            (11)   /* Ignore Error on read   */
#define IGNWRITE           (12)   /* Ignore Error on write  */
/*
 * _IOR and _IOW encode the read/write instructions to the kernel within the
 * ioctl command code.
 */

#ifdef __STDC__

#define READFLAGS       _IOR('k', 0, union IMS_IO)
#define SETFLAGS        _IOW('k', 1, union IMS_IO)

#else

#define READFLAGS       _IOR(k, 0, union IMS_IO)
#define SETFLAGS        _IOW(k, 1, union IMS_IO)

#endif

/* Some early inmos device drivers used different names */

/* S308A needs these */
#define B008_SETF  IMS_SETF
#define B008_READF IMS_READF
#define B008_IO    IMS_IO

/* S514A and S514B need these */
#define B014_SETF  IMS_SETF
#define B014_READF IMS_READF
#define B014_IO    IMS_IO
#endif /* MSDOS */

#endif /* _IMS_BCMD_H_ */
