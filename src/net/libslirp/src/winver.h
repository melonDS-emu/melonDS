/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef WINVER_H
#define WINVER_H

#if defined(TARGET_WINVER)
/* TARGET_WINVER defined on the compiler command line? */

#  undef WINVER
#  undef _WIN32_WINNT
#  define WINVER TARGET_WINVER
#  define _WIN32_WINNT TARGET_WINVER

#elif !defined(WINVER)
/* Default WINVER to Windows 7 API, same as glib. */
#  undef _WIN32_WINNT

#  define WINVER 0x0601
#  define _WIN32_WINNT WINVER

#endif

/* Ensure that _WIN32_WINNT matches WINVER */
#if defined(WINVER) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT WINVER
#endif

#endif
