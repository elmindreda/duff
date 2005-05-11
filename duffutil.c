/*
 * duff - Duplicate file finder
 * Copyright (c) 2004 Camilla Berglund <elmindreda@users.sourceforge.net>
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

#if HAVE_STDARG_H
#include <stdarg.h>
#endif

#if HAVE_CTYPE_H
#include <ctype.h>
#endif

#include "duffstring.h"
#include "duff.h"

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

const char* get_mode_name(int mode)
{
  switch (mode)
  {
    case S_IFREG:
      return "file";
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
      return "WTF?";
  }
}

void version(void)
{
  fprintf(stderr, "%s\n", PACKAGE_STRING);
  fprintf(stderr, "Copyright (C) 2004 Camilla Berglund\n");
}

void usage(void)
{
  fprintf(stderr, "usage: %s -h\n", PACKAGE_NAME);
  fprintf(stderr, "       %s -v\n", PACKAGE_NAME);
  fprintf(stderr, "       %s [-aeqrt] file ...\n", PACKAGE_NAME);
  fprintf(stderr, "options:\n");
  fprintf(stderr, "  -a  all files; include hidden files when searching recursively\n");
  fprintf(stderr, "  -e  excess files mode, print excess files\n");
  fprintf(stderr, "  -h  show this help\n");
  fprintf(stderr, "  -q  quiet; suppress warnings and error messages\n");
  fprintf(stderr, "  -r  recursive; search in specified directories\n");
  fprintf(stderr, "  -t  throrough; compare files byte by byte\n");
  fprintf(stderr, "  -v  show version information\n");
}

void bugs(void)
{
  fprintf(stderr, "Report bugs to <%s>\n", PACKAGE_BUGREPORT);
}

void print_cluster_header(const char* format,
                          unsigned int count,
			  unsigned int number,
			  size_t size,
			  unsigned int checksum)
{
  const char* c;

  for (c = format;  *c != '\0';  c++)
  {
    if (*c == '%')
    {
      c++;
      switch (*c)
      {
	case 's':
	  printf("%zu", size);
	  break;
	case 'n':
	  printf("%u", number);
	  break;
	case 'c':
	  printf("%u", count);
	  break;
	case 'k':
	  printf("0x%08lx", checksum);
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
  
  putchar('\n');
}

