/* Copyright INMOS Limited 1988,1990 */

/* CMSIDENTIFIER */
/* PRODUCT:ITEM.VARIANT-TYPE;0(DATE) */

/* primary server operation tags  */
#ifndef _SP_H

#define _SP_H

#define SP_ZERO            0

#define SP_OPEN            10        /* filec.c  */
#define SP_CLOSE           11
#define SP_READ            12
#define SP_WRITE           13
#define SP_GETS            14
#define SP_PUTS            15
#define SP_FLUSH           16
#define SP_SEEK            17
#define SP_TELL            18
#define SP_EOF             19
#define SP_FERROR          20
#define SP_REMOVE          21
#define SP_RENAME          22
#define SP_GETBLOCK        23
#define SP_PUTBLOCK        24
#define SP_ISATTY          25
#define SP_OPENREC         26
#define SP_GETREC          27
#define SP_PUTREC          28
#define SP_PUTEOF          29
#define SP_GETKEY          30        /* hostc.c  */
#define SP_POLLKEY         31
#define SP_GETENV          32
#define SP_TIME            33
#define SP_SYSTEM          34
#define SP_EXIT            35
#define SP_COMMAND         40        /* serverc.c  */
#define SP_CORE            41
#define SP_ID              42
#define SP_GETINFO         43
#define SP_MSDOS           50        /* msdos.c  */

#define SP_PARMACS         65       /* RCW Parmacs addition */ 

#define SP_FILEEXISTS      80
#define SP_TRANSLATE       81
#define SP_FERRSTAT        82
#define SP_COMMANDARG      83
#define SP_ALSYS           100       /* Not used by inmos iserver */
#define SP_KPAR            101       /* Not used by inmos iserver */
#define SP_BEAM            102       /* Not used by inmos iserver */

/* INMOS reserves all numbers up to 127  */

/* Operation results */
#define ER_SUCCESS         0
#define ER_UNIMPLEMENTED   1
#define ER_ERROR           129
#define ER_NOPRIV          131
#define ER_NORESOURCE      132
#define ER_NOFILE          133
#define ER_TRUNCATED       134
#define ER_BADID           135
#define ER_NOPOSN          136
#define ER_NOTAVAILABLE    137
#define ER_EOF             138
#define ER_AKEYREPLY       139
#define ER_BADPARAS        141
#define ER_NOTERM          142
#define ER_RECTOOBIG       143

#define ZERO               0
#define NONZERO            1

#endif /* _SP_H */
