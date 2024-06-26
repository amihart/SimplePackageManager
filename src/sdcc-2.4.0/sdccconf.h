/* sdccconf.h.  Generated by configure.  */
/*
 */

#ifndef SDCCCONF_HEADER
#define SDCCCONF_HEADER


#define SDCC_VERSION_HI 2
#define SDCC_VERSION_LO 4
#define SDCC_VERSION_P 0
#define SDCC_VERSION_STR "2.4.0"

#define DIR_SEPARATOR_STRING "/"
#define DIR_SEPARATOR_CHAR '/'

#define PREFIX "/usr"
#define EXEC_PREFIX "/usr"
#define BINDIR "/usr/bin"
#define DATADIR "/usr/share"

#define INCLUDE_DIR_SUFFIX DIR_SEPARATOR_STRING "sdcc/include"
#define LIB_DIR_SUFFIX DIR_SEPARATOR_STRING "sdcc/lib"

#define BIN2DATA_DIR DIR_SEPARATOR_STRING "../share"
#define PREFIX2BIN_DIR DIR_SEPARATOR_STRING "bin"
#define PREFIX2DATA_DIR DIR_SEPARATOR_STRING "share"

/* environment variables */
#define SDCC_DIR_NAME "SDCC_HOME"
#define SDCC_INCLUDE_NAME "SDCC_INCLUDE"
#define SDCC_LIB_NAME "SDCC_LIB"

/* standard libraries */
#define STD_LIB "libsdcc"
#define STD_INT_LIB "libint"
#define STD_LONG_LIB "liblong"
#define STD_FP_LIB "libfloat"
#define STD_DS390_LIB "libds390"
#define STD_DS400_LIB "libds400"
#define STD_XA51_LIB "libxa51"

#define HAVE_SYS_SOCKET_H 1

#define HAVE_STRERROR 1
#define HAVE_VSNPRINTF 1
#define HAVE_SNPRINTF 1
#define HAVE_VSPRINTF 1
#define HAVE_MKSTEMP 1

#define RETSIGTYPE void

#define TYPE_BYTE char
#define TYPE_WORD short
#define TYPE_DWORD int
#define TYPE_UBYTE unsigned TYPE_BYTE
#define TYPE_UWORD unsigned TYPE_WORD
#define TYPE_UDWORD unsigned TYPE_DWORD

/* #undef WORDS_BIGENDIAN */

#define OPT_DISABLE_MCS51 1
#define OPT_DISABLE_GBZ80 1
#define OPT_DISABLE_Z80 0
#define OPT_DISABLE_AVR 1
#define OPT_DISABLE_DS390 1
#define OPT_DISABLE_DS400 1
#define OPT_DISABLE_TININative 1
#define OPT_DISABLE_PIC 1
#define OPT_DISABLE_PIC16 1
#define OPT_DISABLE_XA51 1
#define OPT_DISABLE_HC08 1

#define OPT_ENABLE_LIBGC 0

#endif

/* End of config.h */
