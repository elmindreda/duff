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

#define _GNU_SOURCE 1
#define _FILE_OFFSET_BITS 64

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

/* The 'symlink dereference' mode.  Controls the handling of symlinks to
 * directories.  The different modes are defined in duff.h.
 */
int follow_links_mode = NO_SYMLINKS;
/* The 'all files' flag.  Includes dotfiles when searching recursively.
 */
int all_files_flag = 0;
/* The 'verbose' flag.  Makes the program verbose.
 */
int verbose_flag = 0;
/* The 'recursive' flag.  Recurses into all specified directories.
 */
int recursive_flag = 0;
/*! The 'null termination' flag.  Reads and writes null-terminated file
 *  names.
 */
int null_terminate_flag = 0;
/* The 'shut up' flag.  Makes the program not complain about skipped
 * non-files.
 */
int quiet_flag = 0;
/* The 'physical mode' flag.  Makes the program consider entries being
 * physical files instead of hard links.
 */
int physical_flag = 0;
/* The 'excess' flag.  For each duplicate cluster, reports all but one.
 * Useful for `xargs rm'.
 */
int excess_flag = 0;
/* The 'paranoid' flag.  Makes the program distrust message digests, forcing
 * byte-by-byte comparisons.
 */
int thorough_flag = 0;
/* The ignore empty files' flag.  Makes the program not report empty
 * files as duplicates.
 */
int ignore_empty_flag = 0;
/* The 'header format' value.  Specifies the look of the cluster header.
 * If set to the empty string, no headers are printed.
 */
const char* header_format = DEFAULT_HEADER_FORMAT;
/* The 'sample limit' value.  Specifies the minimal size of files to be
 * compared with the sampling method.
 */
off_t sample_limit = DEFAULT_SIZE_LIMIT;
/* The message digest function to use.
 */
enum Function digest_function = SHA_1;

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
 * Note that it is a good idea to keep this synchronised with the actual code.
 * Note that it is also a good idea it keep it synchronised with the manpage.
 */
static void usage(void)
{
  fprintf(stderr, "usage: %s [-0HLPaepqrtz] [-c function] [-f format] [-l size] [file ...]\n", PACKAGE_NAME);
  fprintf(stderr, "       %s -h\n", PACKAGE_NAME);
  fprintf(stderr, "       %s -v\n", PACKAGE_NAME);
  fprintf(stderr, "options:\n"
                  "  -0  read and write file names terminated by a null character\n"
                  "  -H  follow symbolic links on the command line\n"
                  "  -L  follow all symbolic links\n"
                  "  -P  do not follow any symbolic links (default)\n"
                  "  -a  all files; include hidden files when searching recursively\n"
		  "  -c  the hash function to use\n"
                  "  -e  excess files mode, print excess files\n"
                  "  -f  header format; set format for cluster headers\n"
                  "  -h  show this help\n"
                  "  -l  size limit; the minimal size that activates sampling\n"
                  "  -q  quiet; suppress warnings and error messages\n"
                  "  -p  physical mode; do not report multiple links\n"
                  "  -r  recursive; search in specified directories\n"
                  "  -t  thorough; force byte-by-byte comparison of files\n"
                  "  -v  show version information\n"
                  "  -z  do not report empty files\n");
}

/* Prints bug report address to stderr.
 */
static void bugs(void)
{
  fprintf(stderr, "Report bugs to <%s>\n", PACKAGE_BUGREPORT);
}

/* Reads a path name from stdin, according to the specified flags.
 */
static int read_path(char* path, size_t size)
{
  int c, i = 0;
  size_t length;

  if (null_terminate_flag)
  {
    while (i < size)
    {
      if ((c = fgetc(stdin)) == EOF)
	return -1;

      path[i++] = (char) c;
      if (c == '\0')
	break;
    }
  }
  else
  {
    if (!fgets(path, size, stdin))
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
static void kill_trailing_slashes(char* path)
{
  char* temp;

  while ((temp = strrchr(path, '/')))
  {
    if (temp == path || *(temp + 1) != '\0')
      break;
    *temp = '\0';
  }
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

  while ((ch = getopt(argc, argv, "0HLPac:ef:hl:pqrtvz")) != -1)
  {
    switch (ch)
    {
      case '0':
	null_terminate_flag = 1;
	break;
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
      case 'c':
	if (strcmp(optarg, "sha1") == 0)
	  digest_function = SHA_1;
	else if (strcmp(optarg, "sha256") == 0)
	  digest_function = SHA_256;
	else if (strcmp(optarg, "sha384") == 0)
	  digest_function = SHA_384;
	else if (strcmp(optarg, "sha512") == 0)
	  digest_function = SHA_512;
	else
	  error("Unknown hash function %s", optarg);
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
    /* Read file names from command line */
    for (i = 0;  i < argc;  i++)
    {
      kill_trailing_slashes(argv[i]);
      process_path(argv[i], 0);
    }
  }
  else
  {
    /* Read file names from stdin */
    while (read_path(path, sizeof(path)) == 0)
    {
      kill_trailing_slashes(path);
      process_path(path, 0);
    }
  }
  
  report_clusters();
  
  exit(0);
}

