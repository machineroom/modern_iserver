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

//jawilso3 added 
extern int OpenLink(char *Name);
extern int CloseLink(int LinkId);
extern int ReadLink(int LinkId, char *Buffer, unsigned int Count, int Timeout);
extern int WriteLink(int LinkId, char *Buffer, unsigned int Count, int Timeout);
extern int ResetLink(int LinkId);
extern int AnalyseLink(int LinkId);
extern int TestError(int LinkId);

/* Now all the external declarations */

#ifdef LNKC011
extern int C011OpenLink(const char *Name);
extern int C011CloseLink(int LinkId);
extern int C011ReadLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int C011WriteLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int C011ResetLink(int LinkId);
extern int C011AnalyseLink(int LinkId);
extern int C011TestError(int LinkId);
extern int C011TestRead(int LinkId);
extern int C011TestWrite(int LinkId);
#endif /* LNKC011 */

#ifdef LNKb004
extern int B004OpenLink(const char *Name);
extern int B004CloseLink(int LinkId);
extern int B004ReadLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int B004WriteLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int B004ResetLink(int LinkId);
extern int B004AnalyseLink(int LinkId);
extern int B004TestError(int LinkId);
extern int B004TestRead(int LinkId);
extern int B004TestWrite(int LinkId);
#endif /* B004 */

#ifdef LNKb008
extern int B008OpenLink(const char *Name);
extern int B008CloseLink(int LinkId);
extern int B008ReadLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int B008WriteLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int B008ResetLink(int LinkId);
extern int B008AnalyseLink(int LinkId);
extern int B008TestError(int LinkId);
extern int B008TestRead(int LinkId);
extern int B008TestWrite(int LinkId);
#endif /* B008 */

#ifdef LNKb011
extern int B011OpenLink(const char *Name);
extern int B011CloseLink(int LinkId);
extern int B011ReadLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int B011WriteLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int B011ResetLink(int LinkId);
extern int B011AnalyseLink(int LinkId);
extern int B011TestError(int LinkId);
extern int B011TestRead(int LinkId);
extern int B011TestWrite(int LinkId);
#endif /* B011 */

#ifdef LNKb014
extern int B014OpenLink(const char *Name);
extern int B014CloseLink(int LinkId);
extern int B014ReadLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int B014WriteLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int B014ResetLink(int LinkId);
extern int B014AnalyseLink(int LinkId);
extern int B014TestError(int LinkId);
extern int B014TestRead(int LinkId);
extern int B014TestWrite(int LinkId);
#endif /* B014 */

#ifdef LNKb016
extern int B016OpenLink(const char *Name);
extern int B016CloseLink(int LinkId);
extern int B016ReadLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int B016WriteLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int B016ResetLink(int LinkId);
extern int B016AnalyseLink(int LinkId);
extern int B016TestError(int LinkId);
extern int B016TestRead(int LinkId);
extern int B016TestWrite(int LinkId);
#endif /* B016 */

#ifdef LNKtsp
extern int TSPOpenLink(const char *Name);
extern int TSPCloseLink(int LinkId);
extern int TSPReadLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int TSPWriteLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int TSPResetLink(int LinkId);
extern int TSPAnalyseLink(int LinkId);
extern int TSPTestError(int LinkId);
extern int TSPTestRead(int LinkId);
extern int TSPTestWrite(int LinkId);
#endif /* TSP */

#ifdef LNKqt0
extern int QT0OpenLink(const char *Name);
extern int QT0CloseLink(int LinkId);
extern int QT0ReadLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int QT0WriteLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int QT0ResetLink(int LinkId);
extern int QT0AnalyseLink(int LinkId);
extern int QT0TestError(int LinkId);
extern int QT0TestRead(int LinkId);
extern int QT0TestWrite(int LinkId);
#endif /* QT0 */

#ifdef S386
extern int S386OpenLink(const char *Name);
extern int S386CloseLink(int LinkId);
extern int S386ReadLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int S386WriteLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int S386ResetLink(int LinkId);
extern int S386AnalyseLink(int LinkId);
extern int S386TestError(int LinkId);
extern int S386TestRead(int LinkId);
extern int S386TestWrite(int LinkId);
#endif /* S386 */

#ifdef LNKtd
extern int TDOpenLink(const char *Name);
extern int TDCloseLink(int LinkId);
extern int TDReadLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int TDWriteLink(int LinkId, char *Buffer, int Count, int Timeout);
extern int TDResetLink(int LinkId);
extern int TDAnalyseLink(int LinkId);
extern int TDTestError(int LinkId);
#endif /* TD */

#endif /* _LINKIO_H_ */
