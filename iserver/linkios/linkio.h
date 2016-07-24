/* Copyright INMOS Limited 1988,1990,1991 */
/* CMSIDENTIFIER */
/* PRODUCT:ITEM.VARIANT-TYPE;0(DATE) */

#ifndef _LINKIO_H_

#define _LINKIO_H_

#define NULL_LINK -1

#define SUCCEEDED        0
#define ER_LINK_BAD     -1
#define ER_LINK_CANT    -2
#define ER_LINK_SOFT    -3
#define ER_LINK_NODATA  -4
#define ER_LINK_NOSYNC  -5
#define ER_LINK_BUSY    -6
#define ER_NO_LINK      -7
#define ER_LINK_SYNTAX  -8

/* Now all the external declarations */

#ifdef LNKb004
#ifdef PROTOTYPES
extern int B004OpenLink(const char *Name);
extern int B004CloseLink(int LinkId);
extern int B004ReadLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int B004WriteLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int B004ResetLink(int LinkId);
extern int B004AnalyseLink(int LinkId);
extern int B004TestError(int LinkId);
extern int B004TestRead(int LinkId);
extern int B004TestWrite(int LinkId);
#else
extern int B004OpenLink();
extern int B004CloseLink();
extern int B004ReadLink();
extern int B004WriteLink();
extern int B004ResetLink();
extern int B004AnalyseLink();
extern int B004TestError();
extern int B004TestRead();
extern int B004TestWrite();
#endif /* PROTOTYPES */
#endif /* B004 */

#ifdef LNKb008
#ifdef PROTOTYPES
extern int B008OpenLink(const char *Name);
extern int B008CloseLink(int LinkId);
extern int B008ReadLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int B008WriteLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int B008ResetLink(int LinkId);
extern int B008AnalyseLink(int LinkId);
extern int B008TestError(int LinkId);
extern int B008TestRead(int LinkId);
extern int B008TestWrite(int LinkId);
#else
extern int B008OpenLink();
extern int B008CloseLink();
extern int B008ReadLink();
extern int B008WriteLink();
extern int B008ResetLink();
extern int B008AnalyseLink();
extern int B008TestError();
extern int B008TestRead();
extern int B008TestWrite();
#endif /* PROTOTYPES */
#endif /* B008 */

#ifdef LNKb011
#ifdef PROTOTYPES
extern int B011OpenLink(const char *Name);
extern int B011CloseLink(int LinkId);
extern int B011ReadLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int B011WriteLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int B011ResetLink(int LinkId);
extern int B011AnalyseLink(int LinkId);
extern int B011TestError(int LinkId);
extern int B011TestRead(int LinkId);
extern int B011TestWrite(int LinkId);
#else
extern int B011OpenLink();
extern int B011CloseLink();
extern int B011ReadLink();
extern int B011WriteLink();
extern int B011ResetLink();
extern int B011AnalyseLink();
extern int B011TestError();
extern int B011TestRead();
extern int B011TestWrite();
#endif /* PROTOTYPES */
#endif /* B011 */

#ifdef LNKb014
#ifdef PROTOTYPES
extern int B014OpenLink(const char *Name);
extern int B014CloseLink(int LinkId);
extern int B014ReadLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int B014WriteLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int B014ResetLink(int LinkId);
extern int B014AnalyseLink(int LinkId);
extern int B014TestError(int LinkId);
extern int B014TestRead(int LinkId);
extern int B014TestWrite(int LinkId);
#else
extern int B014OpenLink();
extern int B014CloseLink();
extern int B014ReadLink();
extern int B014WriteLink();
extern int B014ResetLink();
extern int B014AnalyseLink();
extern int B014TestError();
extern int B014TestRead();
extern int B014TestWrite();
#endif /* PROTOTYPES */
#endif /* B014 */

#ifdef LNKb016
#ifdef PROTOTYPES
extern int B016OpenLink(const char *Name);
extern int B016CloseLink(int LinkId);
extern int B016ReadLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int B016WriteLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int B016ResetLink(int LinkId);
extern int B016AnalyseLink(int LinkId);
extern int B016TestError(int LinkId);
extern int B016TestRead(int LinkId);
extern int B016TestWrite(int LinkId);
#else
extern int B016OpenLink();
extern int B016CloseLink();
extern int B016ReadLink();
extern int B016WriteLink();
extern int B016ResetLink();
extern int B016AnalyseLink();
extern int B016TestError();
extern int B016TestRead();
extern int B016TestWrite();
#endif /* PROTOTYPES */
#endif /* B016 */

#ifdef LNKtsp
#ifdef PROTOTYPES
extern int TSPOpenLink(const char *Name);
extern int TSPCloseLink(int LinkId);
extern int TSPReadLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int TSPWriteLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int TSPResetLink(int LinkId);
extern int TSPAnalyseLink(int LinkId);
extern int TSPTestError(int LinkId);
extern int TSPTestRead(int LinkId);
extern int TSPTestWrite(int LinkId);
#else
extern int TSPOpenLink();
extern int TSPCloseLink();
extern int TSPReadLink();
extern int TSPWriteLink();
extern int TSPResetLink();
extern int TSPAnalyseLink();
extern int TSPTestError();
extern int TSPTestRead();
extern int TSPTestWrite();
#endif /* PROTOTYPES */
#endif /* TSP */

#ifdef LNKqt0
#ifdef PROTOTYPES
extern int QT0OpenLink(const char *Name);
extern int QT0CloseLink(int LinkId);
extern int QT0ReadLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int QT0WriteLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int QT0ResetLink(int LinkId);
extern int QT0AnalyseLink(int LinkId);
extern int QT0TestError(int LinkId);
extern int QT0TestRead(int LinkId);
extern int QT0TestWrite(int LinkId);
#else
extern int QT0OpenLink();
extern int QT0CloseLink();
extern int QT0ReadLink();
extern int QT0WriteLink();
extern int QT0ResetLink();
extern int QT0AnalyseLink();
extern int QT0TestError();
extern int QT0TestRead();
extern int QT0TestWrite();
#endif /* PROTOTYPES */
#endif /* QT0 */

#ifdef S386
#ifdef PROTOTYPES
extern int S386OpenLink(const char *Name);
extern int S386CloseLink(int LinkId);
extern int S386ReadLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int S386WriteLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int S386ResetLink(int LinkId);
extern int S386AnalyseLink(int LinkId);
extern int S386TestError(int LinkId);
extern int S386TestRead(int LinkId);
extern int S386TestWrite(int LinkId);
#else
extern int S386OpenLink();
extern int S386CloseLink();
extern int S386ReadLink();
extern int S386WriteLink();
extern int S386ResetLink();
extern int S386AnalyseLink();
extern int S386TestError();
extern int S386TestRead();
extern int S386TestWrite();
#endif /* PROTOTYPES */
#endif /* S386 */

#ifdef LNKtd
#ifdef PROTOTYPES
extern int TDOpenLink(const char *Name);
extern int TDCloseLink(int LinkId);
extern int TDReadLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int TDWriteLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int TDResetLink(int LinkId);
extern int TDAnalyseLink(int LinkId);
extern int TDTestError(int LinkId);
#else
extern int TDOpenLink();
extern int TDCloseLink();
extern int TDReadLink();
extern int TDWriteLink();
extern int TDResetLink();
extern int TDAnalyseLink();
extern int TDTestError();
#endif /* PROTOTYPES */
#endif /* TD */

#endif /* _LINKIO_H_ */
