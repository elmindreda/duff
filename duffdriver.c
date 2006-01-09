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

#if HAVE_DIRENT_H
#include <dirent.h>
#endif

#include "duffstring.h"
#include "duff.h"

/* These flags are defined and documented in duff.c.
 */
extern int follow_links_mode;
extern int all_files_flag;
extern int recursive_flag;
extern int ignore_empty_flag;
extern int quiet_flag;
extern int physical_flag;
extern int excess_flag;
extern const char* header_format;

/* List head for collected entries.
 */
static struct Entry* file_entries = NULL;
/* List head for traversed directories.
 */
static struct Directory* directories = NULL;

/* These functions are documented below, where they are defined.
 */
static int stat_file(const char* path, struct stat* sb, int depth);
static int has_recursed_directory(dev_t device, ino_t inode);
static void recurse_directory(const char* path,
                              const struct stat* sb,
			      int depth);
static void report_cluster(struct Entry* duplicates,
                           unsigned int number,
			   unsigned int count);

/* Stat:s a file according to the specified options.
 */
static int stat_file(const char* path, struct stat* sb, int depth)
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
    if (follow_links_mode == ALL_SYMLINKS ||
        depth == 0 && follow_links_mode == ARG_SYMLINKS)
    {
      if (stat(path, sb) != 0)
      {
	if (!quiet_flag)
	  warning("%s: %s", path, strerror(errno));

	return -1;
      }

      if ((sb->st_mode & S_IFMT) != S_IFDIR)
	return -1;
    }
    else
      return -1;
  }

  return 0;
}

/* Returns true if the directory has already been recursed into.
 * NOTE: This implementation is hideous.
 */
static int has_recursed_directory(dev_t device, ino_t inode)
{
  struct Directory* dir;

  /* TODO: Implement a more efficient data structure */

  for (dir = directories;  dir;  dir = dir->next)
  {
    if (dir->device == device && dir->inode == inode)
      return 1;
  }

  return 0;
}

/* Records the specified directory as recursed.
 * NOTE: This implementation is hideous.
 */
static void record_directory(dev_t device, ino_t inode)
{
  struct Directory* dir;

  dir = (struct Directory*) malloc(sizeof(struct Directory));
  dir->device = device;
  dir->inode = inode;
  dir->next = directories;
  directories = dir;
}

/* Recurses into a directory, collecting all or all non-hidden files,
 * according to the specified options.
 */
static void recurse_directory(const char* path,
                              const struct stat* sb,
			      int depth)
{
  DIR* dir;
  struct dirent* dir_entry;
  char* child_path;
  const char* name;

  if (has_recursed_directory(sb->st_dev, sb->st_ino))
    return;

  record_directory(sb->st_dev, sb->st_ino);

  dir = opendir(path);
  if (!dir)
  {
    if (!quiet_flag)
      warning("%s: %s", path, strerror(errno));

    return;
  }

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
    process_path(child_path, depth);
    free(child_path);
  }
  
  closedir(dir);
}

/* Processes a path name, whether from the command line or from
 * directory recursion.
 */
void process_path(const char* path, int depth)
{
  mode_t mode;
  struct stat sb;
  struct Entry* entry;

  if (stat_file(path, &sb, depth) != 0)
    return;

  mode = sb.st_mode & S_IFMT;
  switch (mode)
  {
    case S_IFREG:
    {
      if (sb.st_size == 0)
      {
	if (ignore_empty_flag)
	  return;
      }
      else
      {
	if (access(path, R_OK) != 0)
	{
	  if (!quiet_flag)
	    warning("%s: %s", path, strerror(errno));

	  return;
	}
      }

      /* NOTE: Check for duplicate arguments? */

      if (physical_flag)
      {
	/* TODO: Make this less suboptimal */

	for (entry = file_entries;  entry;  entry = entry->next)
	{
	  if (entry->device == sb.st_dev && entry->inode == sb.st_ino)
	    return;
	}
      }

      if ((entry = make_entry(path, &sb)) != NULL)
	link_entry(&file_entries, entry);

      break;
    }

    case S_IFDIR:
    {
      if (recursive_flag)
      {
	recurse_directory(path, &sb, depth + 1);
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
    for (entry = duplicates->next;  entry;  entry = entry->next)
    {
      printf("%s\n", entry->path);
      entry->status = REPORTED;
    }
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
    {
      printf("%s\n", entry->path);
      entry->status = REPORTED;
    }
  }
}

/* Finds and reports all duplicate clusters among the collected entries.
 */
void report_clusters(void)
{
  int number = 1, count = 0;
  struct Entry* base;
  struct Entry* entry;
  struct Entry* entry_next;
  struct Entry* duplicates = NULL;

  /* TODO: Implement a more efficient data structure */

  while ((base = file_entries) != NULL)
  {
    if (base->status == INVALID)
      continue;

    count = 0;
    
    for (entry = base->next;  entry;  entry = entry_next)
    {
      entry_next = entry->next;

      if (compare_entries(base, entry) == 0)
      {
	if (duplicates == NULL)
	{
	  unlink_entry(&file_entries, base);
	  link_entry(&duplicates, base);
	  
	  base->status = DUPLICATE;
	  count++;
	}
	
	unlink_entry(&file_entries, entry);
	link_entry(&duplicates, entry);
	
	entry->status = DUPLICATE;
	count++;
      }
    }
     
    if (duplicates)
    {
      report_cluster(duplicates, number, count);
      free_entry_list(&duplicates);
      number++;
    }
    else
    {
      unlink_entry(&file_entries, base);
      free_entry(base);
    }
  }
}

