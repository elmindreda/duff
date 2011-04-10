/*
 * duff - Duplicate file finder
 * Copyright (c) 2005 Camilla Berglund <elmindreda@elmindreda.org>
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

#if HAVE_STRING_H
#include <string.h>
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
extern int null_terminate_flag;
extern enum Function digest_function;

/* Reads a path name from stdin according to the specified flags.
 */
int read_path(FILE* stream, char* path, size_t size)
{
  int c, i = 0;
  size_t length;

  if (null_terminate_flag)
  {
    while (i < size)
    {
      if ((c = fgetc(stream)) == EOF)
	return -1;

      path[i++] = (char) c;
      if (c == '\0')
	break;
    }
  }
  else
  {
    if (!fgets(path, size, stream))
      return -1;

    /* Kill newline terminator, if present */
    length = strlen(path);
    if ((length > 0) && (path[length - 1] == '\n'))
      path[length - 1] = '\0';
  }

  return 0;
}

/* Kills trailing slashes in the specified path (except if it's /).
 */
void kill_trailing_slashes(char* path)
{
  char* temp;

  while ((temp = strrchr(path, '/')))
  {
    if (temp == path || *(temp + 1) != '\0')
      break;
    *temp = '\0';
  }
}

/*! Returns the size, in bytes, of the specified digest type.
 */
size_t get_digest_size(void)
{
  switch (digest_function)
  {
    case SHA_1:
      return SHA1_HASH_SIZE;
    case SHA_256:
      return SHA256_HASH_SIZE;
    case SHA_384:
      return SHA384_HASH_SIZE;
    case SHA_512:
      return SHA512_HASH_SIZE;
    default:
      error(_("This cannot happen"));
  }
}

/* Prints a formatted message to stderr and exist with non-zero status.
 */
void error(const char* format, ...)
{
  char* message;
  int result;
  va_list vl;

  va_start(vl, format);
  result = vasprintf(&message, format, vl);
  va_end(vl);

  if (result > 0)
  {
    fprintf(stderr, "%s\n", message);
    free(message);
  }

  exit(1);
}

/* Prints a formatted message to stderr.
 */
void warning(const char* format, ...)
{
  char* message;
  int result;
  va_list vl;

  va_start(vl, format);
  result = vasprintf(&message, format, vl);
  va_end(vl);

  if (result > 0)
  {
    fprintf(stderr, "%s\n", message);
    free(message);
  }
}

/* Prints a duplicate cluster header to stdout.  Various escape
 * sequences in the format string are replaced with the provided values.
 * Note that this function does not terminate the output with any
 * special character (i.e. newline or null).
 */
void print_cluster_header(const char* format,
                          unsigned int count,
			  unsigned int index,
			  off_t size,
			  const uint8_t* digest)
{
  int i, digest_size;
  const char* c;

  for (c = format;  *c != '\0';  c++)
  {
    if (*c == '%')
    {
      c++;
      switch (*c)
      {
	case 's':
	  printf("%" PRIi64, size);
	  break;
	case 'i':
	  printf("%u", index);
	  break;
	case 'n':
	  printf("%u", count);
	  break;
	case 'c':
	case 'd':
	  digest_size = get_digest_size();
	  for (i = 0;  i < digest_size;  i++)
	    printf("%02x", digest[i]);
	  break;
	case '%':
	  putchar('%');
	  break;
	case '\0':
	  putchar('\n');
	  return;
	default:
	  /* If the character following the '%' looks normal then we figure it
	   * might be a good idea to silently prepend a '%' and pretend like we
	   * didn't notice the broken format string.
	   */
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

