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

#if HAVE_DIRENT_H
#include <dirent.h>
#endif

#include "sha1.h"
#include "duffstring.h"
#include "duff.h"

/* The 'follow links' flag. Makes the program follow symbolic links.
 */
int follow_flag = 0;
/* The 'all files' flag. Includes dotfiles when searching recursively.
 */
int all_flag = 0;
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
/* The 'excess' flag. For each duplicate cluster, reports all but one.
 * Useful for `xargs rm'.
 */
int excess_flag = 0;
/* The 'paranoid' flag. Makes the program distrust checksums, forcing
 * byte-by-byte comparisons.
 */
int thorough_flag = 0;
/* The 'header format' value. Specifies the look of the cluster header.
 * If set to the empty string, no headers are printed.
 */
const char* header_format = DEFAULT_HEADER_FORMAT;
/* The 'sample limit' value. Specifies the minimal size of files to be
 * compared with the sampling method.
 */
off_t sample_limit = DEFAULT_SIZE_LIMIT;

static struct Entry* file_entries = NULL;
static struct Cluster* file_clusters = NULL;
static unsigned long file_count = 0;

static void version(void);
static void usage(void);
static void bugs(void);

void process_directory(const char* path);
void process_path(const char* path);

void process_directory(const char* path)
{
  DIR* dir;
  struct dirent* dir_entry;
  char* child_path;
  const char* name;

  dir = opendir(path);
  if (!dir)
    return;

  while (dir_entry = readdir(dir))
  {
    name = dir_entry->d_name;
    if (name[0] == '.')
    {
      if (!all_flag)
	continue;

      if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
	continue;
    }

    asprintf(&child_path, "%s/%s", path, name);
    process_path(child_path);
    free(child_path);
  }
  
  closedir(dir);
  dir = NULL;
}

/* Processes a path.
 */
void process_path(const char* path)
{
  mode_t mode;
  struct stat sb;
  struct Entry* file_entry;

  /* TODO: Check whether the path is a symbolic link. */

  if (lstat(path, &sb) != 0)
  {
    if (!quiet_flag)
      warning("%s: %s", path, strerror(errno));

    return;
  }

  mode = sb.st_mode & S_IFMT;
  if (mode == S_IFLNK)
  {
    if (!follow_flag)
      return;

    if (stat(path, &sb) != 0)
    {
      if (!quiet_flag)
	warning("%s: %s", path, strerror(errno));

      return;
    }

    mode = sb.st_mode & S_IFMT;
  }

  switch (mode)
  {
    case S_IFREG:
    {
      if (file_entry = make_entry(path, sb.st_size))
      {
        file_entry->next = file_entries;
        file_entries = file_entry;
      }

      break;
    }

    case S_IFDIR:
    {
      if (recursive_flag)
      {
	process_directory(path);
        break;
      }

      /* FALLTHROUGH */
    }

    default:
    {
      if (!quiet_flag)
        warning("%s: %s, skipping", path, get_mode_name(mode));
      break;
    }
  }
}

void find_clusters(void)
{
  /*
  struct Entry* base;
  struct Entry* entry;
  int count, number = 1;
  
  for (base = file_entries;  base;  base = base->next)
  {
    if (base->status == INVALID || base->status == REPORTED)
      continue;

    count = 1;
    
    for (entry = base->next;  entry;  entry = entry->next)
    {
      if (base->size == entry->size)
      {
        if (get_entry_checksum(base) != 0)
          break;
        
        if (get_entry_checksum(entry) != 0)
          continue;
        
        if (base->checksum == entry->checksum)
        {
          if (base->status != REPORTED)
          {
            copy = copy_entry(base);
            copy->next = duplicates;
            duplicates = copy;
            
            base->status = REPORTED;
          }
          
          copy = copy_entry(entry);
          copy->next = duplicates;
          duplicates = copy;
          
          entry->status = REPORTED;
          count++;
        }
      }
    }
  }
  */
}

static void version(void)
{
  fprintf(stderr, "%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
  fprintf(stderr, "Copyright (c) 2004 Camilla Berglund <elmindreda@users.sourceforge.net>\n");
  fprintf(stderr, "%s contains sha1-asaddi\n", PACKAGE_NAME);
  fprintf(stderr, "Copyright (c) 2001-2003 Allan Saddi <allan@saddi.com>\n");
}

/* It is a good idea to keep this synchronised with the actual code.
 */
static void usage(void)
{
  fprintf(stderr, "usage: %s -h\n", PACKAGE_NAME);
  fprintf(stderr, "       %s -v\n", PACKAGE_NAME);
  fprintf(stderr, "       %s [-LPaeqrt] [-f format] [-l size] file ...\n", PACKAGE_NAME);
  fprintf(stderr, "options:\n");
  fprintf(stderr, "  -L  follow all symbolic links\n");
  fprintf(stderr, "  -P  don't follow any symbolic links\n");
  fprintf(stderr, "  -a  all files; include hidden files when searching recursively\n");
  fprintf(stderr, "  -e  excess files mode, print excess files\n");
  fprintf(stderr, "  -f  header format; set format for cluster headers\n");
  fprintf(stderr, "  -h  show this help\n");
  fprintf(stderr, "  -l  size limit; the minimal size that activates sampling\n");
  fprintf(stderr, "  -q  quiet; suppress warnings and error messages\n");
  fprintf(stderr, "  -r  recursive; search in specified directories\n");
  fprintf(stderr, "  -t  throrough; compare files byte by byte\n");
  fprintf(stderr, "  -v  show version information\n");
}

static void bugs(void)
{
  fprintf(stderr, "Report bugs to <%s>\n", PACKAGE_BUGREPORT);
}

int main(int argc, char** argv)
{
  int i, ch, number, count = 0;
  char* temp;
  struct Entry* base;
  struct Entry* entry;
  struct Entry* copy;
  struct Entry* duplicates = NULL;
  
  while ((ch = getopt(argc, argv, "LPavrqhef:l:")) != -1)
  {
    switch (ch)
    {
      case 'L':
	follow_flag = 1;
	break;
      case 'P':
	follow_flag = 0;
	break;
      case 'a':
        all_flag = 1;
        break;
      case 'v':
        version();
        exit(0);
      case 'r':
        recursive_flag = 1;
        break;
      case 'q':
        quiet_flag = 1;
        break;
      case 'e':
        excess_flag = 1;
	error("option -e not yet implemented");
      case 't':
        thorough_flag = 1;
        break;
      case 'f':
        header_format = optarg;
        break;
      case 'l':
        sample_limit = (off_t) strtoull(optarg, &temp, 10);
	if (temp == optarg || errno == ERANGE)
	  warning("ignoring malformed size limit %s", optarg);
	break;
      case 'h':
      default:
        usage();
        bugs();
        exit(0);
    }
  }
  
  argc -= optind;
  argv += optind;

  if (!argc)
  {
    usage();
    bugs();
    exit(0);
  }
  
  for (i = 0;  i < argc;  i++)
  {
    if (*argv[i] == '\0')
      continue;

    process_path(argv[i]);
  }
  
  number = 1;
  
  for (base = file_entries;  base;  base = base->next)
  {
    if (base->status == INVALID || base->status == REPORTED)
      continue;

    count = 1;
    
    for (entry = base->next;  entry;  entry = entry->next)
    {
      if (compare_entries(base, entry) == 0)
      {
	if (base->status != REPORTED)
	{
	  copy = copy_entry(base);
	  copy->next = duplicates;
	  duplicates = copy;
	  
	  base->status = REPORTED;
	}
	
	copy = copy_entry(entry);
	copy->next = duplicates;
	duplicates = copy;
	
	entry->status = REPORTED;
	count++;
      }
    }
    
    if (duplicates)
    {
      if (*header_format != '\0')
        print_cluster_header(header_format,
                             count,
                             number,
                             duplicates->size,
                             duplicates->checksum);

      for (entry = duplicates;  entry;  entry = entry->next)
        printf("%s\n", entry->path);
      
      free_entry_list(&duplicates);
      number++;
    }
  }
  
  free_entry_list(&file_entries);
}

