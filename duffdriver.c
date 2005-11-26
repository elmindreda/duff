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

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#if HAVE_DIRENT_H
#include <dirent.h>
#endif

#include "duffstring.h"
#include "duff.h"

/* These flags are defined and documented in duff.c.
 */
extern int follow_links_flag;
extern int all_files_flag;
extern int recursive_flag;
extern int quiet_flag;
extern int excess_flag;
extern int ignore_empty_flag;
extern const char* header_format;

/* List head for collected entries.
 */
static struct Entry* file_entries = NULL;

/* These functions are documented below, where they are defined.
 */
static int stat_file(const char* path, struct stat* sb);
static void recurse_directory(const char* path);
static void report_cluster(struct Entry* duplicates,
                           unsigned int number,
			   unsigned int count);

/* Stat:s a file according to the specified options.
 */
static int stat_file(const char* path, struct stat* sb)
{
#if HAVE_LSTAT_EMPTY_STRING_BUG || HAVE_STAT_EMPTY_STRING_BUG
  if (*path == '\0')
    return -1;
#endif

  if (lstat(path, sb) != 0)
  {
    if (!quiet_flag)
      warning("%s: %s", path, strerror(errno));

    return -1;
  }

  if ((sb->st_mode & S_IFMT) == S_IFLNK)
  {
    if (!follow_links_flag)
      return -1;

    if (stat(path, sb) != 0)
    {
      if (!quiet_flag)
	warning("%s: %s", path, strerror(errno));

      return -1;
    }
  }

  return 0;
}

/* Recurses into a directory, collecting all or all non-hidden files,
 * according to the specified options.
 */
static void recurse_directory(const char* path)
{
  DIR* dir;
  struct dirent* dir_entry;
  char* child_path;
  const char* name;

  dir = opendir(path);
  if (!dir)
    return;

  while ((dir_entry = readdir(dir)))
  {
    name = dir_entry->d_name;
    if (name[0] == '.')
    {
      if (!all_files_flag)
	continue;

      if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
	continue;
    }

    asprintf(&child_path, "%s/%s", path, name);
    process_path(child_path);
    free(child_path);
  }
  
  closedir(dir);
}

/* Processes a path name, whether from the command line or from
 * directory recursion.
 */
void process_path(const char* path)
{
  mode_t mode;
  struct stat sb;
  struct Entry* file_entry;

  if (stat_file(path, &sb) != 0)
    return;

  mode = sb.st_mode & S_IFMT;
  switch (mode)
  {
    case S_IFREG:
    {
      if ((file_entry = make_entry(path, &sb)))
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
	recurse_directory(path);
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

/* Reports a cluster to stdout, according to the specified options.
 */
static void report_cluster(struct Entry* duplicates,
                           unsigned int number,
			   unsigned int count)
{
  struct Entry* entry;

  if (excess_flag)
  {
    /* Report all but the first entry in the cluster */
    /* TODO: Prefer symlinks over actual files when -L is in force */
    for (entry = duplicates->next;  entry;  entry = entry->next)
      printf("%s\n", entry->path);
  }
  else
  {
    /* Print header and report all entries in the cluster */

    if (*header_format != '\0')
      print_cluster_header(header_format,
			   count,
			   number,
			   duplicates->size,
			   duplicates->checksum);

    for (entry = duplicates;  entry;  entry = entry->next)
      printf("%s\n", entry->path);
  }
}

/* Finds and reports all duplicate clusters among the collected entries.
 */
void report_clusters(void)
{
  int number = 1, count = 0;
  struct Entry* base;
  struct Entry* entry;
  struct Entry* copy;
  struct Entry* duplicates = NULL;

  /* TODO: Remove reported and invalidated entries. */

  for (base = file_entries;  base;  base = base->next)
  {
    if (base->status == INVALID || base->status == REPORTED)
      continue;

    if (ignore_empty_flag)
    {
      if (base->size == 0)
	continue;
    }

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
      report_cluster(duplicates, number, count);
      free_entry_list(&duplicates);
      number++;
    }
  }

  /* TODO: Put this somewhere sensible */
  free_entry_list(&file_entries);
}

