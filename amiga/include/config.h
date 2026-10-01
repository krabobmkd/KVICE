/* config.h for the AmigaOS 3.x m68k (bebbo gcc 6.5 + libnix) cross build.
 *
 * Hand written: there is no autoconf run for this target. It was derived
 * from a native Linux "--enable-headlessui --enable-x64 --without-resid"
 * config.h, keeping only what libnix / the NDK really provide.
 */

#ifndef VICE_AMIGA_CONFIG_H
#define VICE_AMIGA_CONFIG_H

/* - - - target platform - - - */
#define AMIGA_COMPILE /**/
#define AMIGA_M68K /**/
#define USE_GCC /**/

/* first milestone: reuse the headless UI code paths of the core */
#define USE_HEADLESSUI /**/

/* - - - features - - - */
#define HAVE_FASTSID /**/
#define HAVE_ZLIB /**/
#define HAVE_MOUSE /**/
#define HAVE_LIGHTPEN /**/
#define HAVE_NEW_8580_FILTER /**/
/* note: no FEATURE_CPUMEMHISTORY, too much memory and CPU for a 68030 */
/* integer FastSID filter and sound clock: no FPU on a 68030 (soft-float) */
#define FIXPOINT_ARITHMETIC /**/

/* - - - headers - - - */
#define STDC_HEADERS 1
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_LIMITS_H 1
#define HAVE_MATH_H 1
#define HAVE_SIGNAL_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1
#define HAVE_DIRENT_H 1
#define HAVE_LIBGEN_H 1

/* - - - functions - - - */
#define HAVE_ATEXIT 1
#define HAVE_DIRNAME 1
#define HAVE_GETCWD 1
#define HAVE_GETTIMEOFDAY 1
#define HAVE_MEMMOVE 1
#define HAVE_SNPRINTF 1
#define HAVE_VSNPRINTF 1
#define HAVE_STRCASECMP 1
#define HAVE_STRNCASECMP 1
#define HAVE_STRDUP 1
#define HAVE_STRERROR 1
#define HAVE_STRTOK 1
#define HAVE_STRTOUL 1
#define HAVE_LIBM 1

/* - - - types - - - */
#define HAVE_OFF_T 1
#define HAVE_OFF_T_IN_SYS_TYPES /**/
#define HAVE_TIME_T_IN_TIME_H /**/
#define HAVE_TIME_T_IN_TYPES_H /**/
#define HAVE_U_SHORT 1
#define RETSIGTYPE void
#define SIZEOF_TIME_T 4
#define SIZEOF_UNSIGNED_INT 4
#define SIZEOF_UNSIGNED_LONG 4
#define SIZEOF_UNSIGNED_SHORT 2
#define WORDS_BIGENDIAN 1

/* - - - package - - - */
#define CONFIGURE_FLAGS "amiga cmake cross build"
#define PACKAGE "vice"
#define PACKAGE_BUGREPORT ""
#define PACKAGE_NAME "vice"
#define PACKAGE_STRING "vice 3.10"
#define PACKAGE_TARNAME "vice"
#define PACKAGE_URL ""
#define PACKAGE_VERSION "3.10"
#define PREFIX "PROGDIR:"
#define VERSION "3.10"
#define YYTEXT_POINTER 1

#endif
