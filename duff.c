/*
 * duff - Duplicate file finder
 * Copyright (c) 2006 Camilla Berglund <elmindreda@users.sourceforge.net>
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

#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#if HAVE_INTTYPES_H
#include <inttypes.h>
#else
#if HAVE_STDINT_H
#include <stdint.h>
#endif
#endif

#if HAVE_ERRNO_H
#include <errno.h>
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_STDIO_H
#include <stdio.h>
#endif

#if HAVE_STRING_H
#include <string.h>
#endif

#if HAVE_STDARG_H
#include <stdarg.h>
#endif

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#if HAVE_LIMITS_H
#include <limits.h>
#endif

#include "duffstring.h"
#include "duff.h"

/* The 'follow links' flag. Makes the program follow symbolic links.
 */
int follow_links_mode = NO_SYMLINKS;
/* The 'all files' flag. Includes dotfiles when searching recursively.
 */
int all_files_flag = 0;
/* The 'verbose' flag. Makes the program verbose.
 */
int verbose_flag = 0;
/* The 'recursive' flag. Recurses into all specified directories.
 */
int recursive_flag = 0;
/* The 'shut up' flag. Makes the program not complain about skipped
 * non-files.
 */
int quiet_flag = 0;
/* The 'physical mode' flag. Makes the program consider entries being
 * physical files instead of hard links.
 */
int physical_flag = 0;
/* The 'excess' flag. For each duplicate cluster, reports all but one.
 * Useful for `xargs rm'.
 */
int excess_flag = 0;
/* The 'paranoid' flag. Makes the program distrust checksums, forcing
 * byte-by-byte comparisons.
 */
int thorough_flag = 0;
/* The ignore empty files' flag. Makes the program not report empty
 * files as duplicates.
 */
int ignore_empty_flag = 0;
/* The 'header format' value. Specifies the look of the cluster header.
 * If set to the empty string, no headers are printed.
 */
const char* header_format = DEFAULT_HEADER_FORMAT;
/* The 'sample limit' value. Specifies the minimal size of files to be
 * compared with the sampling method.
 */
off_t sample_limit = DEFAULT_SIZE_LIMIT;

/* These functions are documented below, where they are defined.
 */
static void version(void);
static void usage(void);
static void bugs(void);

/* Prints version information to stderr.
 */
static void version(void)
{
  fprintf(stderr, "%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
  fprintf(stderr, "Copyright (c) 2005 Camilla Berglund <elmindreda@users.sourceforge.net>\n");
  fprintf(stderr, "%s contains sha1-asaddi\n", PACKAGE_NAME);
  fprintf(stderr, "Copyright (c) 2001-2003 Allan Saddi <allan@saddi.com>\n");
}

/* Prints brief help information to stderr.
 * It is a good idea to keep this synchronised with the actual code.
 * It is also a good idea it keep it synchronised with the manpage.
 */
static void usage(void)
{
  fprintf(stderr, "usage: %s [-HLPaepqrt] [-f format] [-l size] [file ...]\n", PACKAGE_NAME);
  fprintf(stderr, "       %s -h\n", PACKAGE_NAME);
  fprintf(stderr, "       %s -v\n", PACKAGE_NAME);
  fprintf(stderr, "options:\n");
  fprintf(stderr, "  -H  follow symbolic links on the command line\n");
  fprintf(stderr, "  -L  follow all symbolic links\n");
  fprintf(stderr, "  -P  do not follow any symbolic links\n");
  fprintf(stderr, "  -a  all files; include hidden files when searching recursively\n");
  fprintf(stderr, "  -e  excess files mode, print excess files\n");
  fprintf(stderr, "  -f  header format; set format for cluster headers\n");
  fprintf(stderr, "  -h  show this help\n");
  fprintf(stderr, "  -l  size limit; the minimal size that activates sampling\n");
  fprintf(stderr, "  -q  quiet; suppress warnings and error messages\n");
  fprintf(stderr, "  -p  physical mode; do not report multiple links\n");
  fprintf(stderr, "  -r  recursive; search in specified directories\n");
  fprintf(stderr, "  -t  thorough; force byte-by-byte comparison of files\n");
  fprintf(stderr, "  -v  show version information\n");
  fprintf(stderr, "  -z  do not report empty files\n");
}

/* Prints bug report address to stderr.
 */
static void bugs(void)
{
  fprintf(stderr, "Report bugs to <%s>\n", PACKAGE_BUGREPORT);
}

/* I don't know what this function does.
 * I just put it in because it looks cool.
 */
int main(int argc, char** argv)
{
  int i, ch;
  char* temp;
  off_t limit;
  char path[PATH_MAX];
  
  while ((ch = getopt(argc, argv, "HLPaef:hl:pqrtvz")) != -1)
  {
    switch (ch)
    {
      case 'H':
	follow_links_mode = ARG_SYMLINKS;
	break;
      case 'L':
	follow_links_mode = ALL_SYMLINKS;
	break;
      case 'P':
	follow_links_mode = NO_SYMLINKS;
	break;
      case 'a':
        all_files_flag = 1;
        break;
      case 'e':
        excess_flag = 1;
	break;
      case 'f':
        header_format = optarg;
        break;
      case 'h':
        usage();
        bugs();
        exit(0);
      case 'l':
        limit = (off_t) strtoull(optarg, &temp, 10);
	if (temp == optarg || errno == ERANGE || errno == EINVAL)
	  warning("malformed size limit %s; ignoring", optarg);
	else
	{
	  if (limit < SAMPLE_COUNT)
	    warning("sample limit must be at least %u; ignoring", SAMPLE_COUNT);
	  else
	    sample_limit = limit;
	}
	break;
      case 'p':
	physical_flag = 1;
	break;
      case 'q':
        quiet_flag = 1;
        break;
      case 'r':
        recursive_flag = 1;
        break;
      case 't':
        thorough_flag = 1;
        break;
      case 'v':
        version();
        exit(0);
      case 'z':
	ignore_empty_flag = 1;
	break;
      default:
        usage();
        bugs();
        exit(1);
    }
  }
  
  argc -= optind;
  argv += optind;

  if (argc)
  {
    for (i = 0;  i < argc;  i++)
    {
#if !LSTAT_FOLLOWS_SLASHED_SYMLINK
      /* Kill trailing slashes (except in "/") */
      while ((temp = strrchr(argv[i], '/')))
      {
	if (temp == argv[i] || *(temp + 1) != '\0')
	  break;
	*temp = '\0';
      }
#endif

      process_path(argv[i], 0);
    }
  }
  else
  {
    /* Read file names from stdin */
    while (fgets(path, sizeof(path), stdin))
    {
      if ((temp = strchr(path, '\n')))
	*temp = '\0';

      process_path(path, 0);

      if (feof(stdin) || ferror(stdin))
	break;
    }
  }
  
  report_clusters();
  
  exit(0);
}

