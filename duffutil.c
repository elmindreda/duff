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

#define _GNU_SOURCE

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#if HAVE_STDIO_H
#include <stdio.h>
#endif

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#if HAVE_STDARG_H
#include <stdarg.h>
#endif

#if HAVE_CTYPE_H
#include <ctype.h>
#endif

#include "sha1.h"
#include "sha256.h"
#include "sha384.h"
#include "sha512.h"

#include "duffstring.h"
#include "duff.h"

/* These flags are defined and documented in duff.c.
 */
extern enum Function digest_function;

static void print_message_digest(const uint8_t* digest)
{
  int i, size;

  switch (digest_function)
  {
    case SHA_1:
      size = SHA1_HASH_SIZE;
      break;
    case SHA_256:
      size = SHA256_HASH_SIZE;
      break;
    case SHA_384:
      size = SHA384_HASH_SIZE;
      break;
    case SHA_512:
      size = SHA512_HASH_SIZE;
      break;
    default:
      error("This cannot happen");
  }

  for (i = 0;  i < size;  i++)
    printf("%02x", digest[i]);
}

/* Prints a formatted message to stderr and exist with non-zero status.
 */
void error(const char* format, ...)
{
  char* message;
  
  va_list vl;
  
  va_start(vl, format);
  vasprintf(&message, format, vl);
  va_end(vl);
  
  fprintf(stderr, "%s\n", message);
  
  free(message);
  exit(1);
}

/* Prints a formatted message to stderr.
 */
void warning(const char* format, ...)
{
  char* message;
  
  va_list vl;
  
  va_start(vl, format);
  vasprintf(&message, format, vl);
  va_end(vl);
  
  fprintf(stderr, "%s\n", message);
  
  free(message);
}

/* Returns a string representation of a fstat(2) file mode.
 */
const char* get_mode_name(int mode)
{
  switch (mode)
  {
    case S_IFREG:
      return "file";
    case S_IFLNK:
      return "symbolic link";
    case S_IFIFO:
      return "named pipe";
    case S_IFCHR:
      return "character device";
    case S_IFDIR:
      return "directory";
    case S_IFBLK:
      return "block device";
    case S_IFSOCK:
      return "socket";
    default:
      return "unknown";
  }
}

/* Prints a duplicate cluster header to stdout.  Various escape
 * sequences in the format string are replaced with the provided values.
 * Note that this function does not terminate the output with any
 * special character.
 */
void print_cluster_header(const char* format,
                          unsigned int count,
			  unsigned int index,
			  off_t size,
			  const uint8_t* digest)
{
  int i;
  const char* c;

  for (c = format;  *c != '\0';  c++)
  {
    if (*c == '%')
    {
      c++;
      switch (*c)
      {
	case 's':
	  printf("%llu", (unsigned long long int) size);
	  break;
	case 'i':
	  printf("%u", index);
	  break;
	case 'n':
	  printf("%u", count);
	  break;
	case 'c':
	  print_message_digest(digest);
	  break;
	case '%':
	  putchar('%');
	  break;
	case '\0':
	  putchar('\n');
	  return;
	default:
	  if (isgraph(*c) || isspace(*c))
	  {
	    putchar('%');
	    putchar(*c);
	  }
      }
    }
    else
      putchar(*c);
  }
}

