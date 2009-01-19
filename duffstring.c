/*
 * duff - Duplicate file finder
 * Copyright (c) 2005 Camilla Berglund <elmindreda@users.sourceforge.net>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 *  1. The origin of this software must not be misrepresented; you
 *     must not claim that you wrote the original software. If you use
 *     this software in a product, an acknowledgment in the product
 *     documentation would be appreciated but is not required.
 *
 *  2. Altered source versions must be plainly marked as such, and
 *     must not be misrepresented as being the original software.
 *
 *  3. This notice may not be removed or altered from any source
 *     distribution.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

/* Macros to get 64-bit off_t
 */
#define _GNU_SOURCE 1
#define _FILE_OFFSET_BITS 64

#if HAVE_STDARG_H
#include <stdarg.h>
#endif

#if HAVE_STRING_H
#include <string.h>
#endif

#if !HAVE_VASPRINTF

int vasprintf(char** result, const char* format, va_list vl)
{
  char buffer[8192];

  if (vsnprintf(buffer, sizeof(buffer), format, vl) < 0)
    buffer[sizeof(buffer) - 1] = '\0';

  size_t length = strlen(buffer);
  *result = (char*) malloc(length + 1);
  strcpy(*result, buffer);

  return length;
}

#endif /*HAVE_VASPRINTF*/

#if !HAVE_ASPRINTF

int asprintf(char** result, const char* format, ...)
{
  va_list vl;
  int length;
  
  va_start(vl, format);
  length = vasprintf(result, format, vl);
  va_end(vl);

  return length;
}

#endif /*HAVE_ASPRINTF*/

