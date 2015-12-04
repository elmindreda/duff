/*
 * duff - Duplicate file finder
 * Copyright (c) 2005 Camilla Berglund <dreda@dreda.org>
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

#if HAVE_INTTYPES_H
#include <inttypes.h>
#elif HAVE_STDINT_H
#include <stdint.h>
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
  #define NAMLEN(dirent) strlen((dirent)->d_name)
#else
  #define dirent direct
  #define NAMLEN(dirent) (dirent)->d_namlen
#if HAVE_SYS_NDIR_H
  #include <sys/ndir.h>
  #endif
    #if HAVE_SYS_DIR_H
      #include <sys/dir.h>
    #endif
  #if HAVE_NDIR_H
    #include <ndir.h>
  #endif
#endif

#include "duffstring.h"
#include "duff.h"

/* The number of buckets to use for files.
 */
#define BUCKET_COUNT (1 << HASH_BITS)

/* Calculates the bucket index corresponding to the specified file size.
 */
#define BUCKET_INDEX(size) ((size) & (BUCKET_COUNT - 1))

/* These flags are defined and documented in duff.c.
 */
extern SymlinkMode follow_links_mode;
extern int all_files_flag;
extern int unique_files_flag;
extern int null_terminate_flag;
extern int recursive_flag;
extern int ignore_empty_flag;
extern int quiet_flag;
extern int physical_flag;
extern int physical_cluster_flag;
extern int excess_flag;
extern const char* header_format;
extern int header_uses_digest;

/* Represents a single physical directory.
 */
struct Dir
{
  dev_t device;
  ino_t inode;
};

typedef struct Dir Dir;

/* Represents a list of physical directories.
 */
struct DirList
{
  Dir* dirs;
  size_t allocated;
  size_t available;
};

typedef struct DirList DirList;

/* List of traversed physical directories, used to avoid loops.
 */
static DirList recorded_dirs;

/* Buckets of lists of collected files.
 */
static FileList buckets[BUCKET_COUNT];

/* These functions are documented below, where they are defined.
 */
static int stat_path(const char* path, struct stat* sb, int depth);
static int has_recorded_directory(dev_t device, ino_t inode);
static void record_directory(dev_t device, ino_t inode);
static void process_directory(const char* path,
                              const struct stat* sb,
                              int depth);
static void process_file(const char* path, struct stat* sb);
static void process_path(const char* path, int depth);
static void report_cluster(const FileList* cluster, unsigned int index);
static void process_clusters(void);
static void process_uniques(void);

/* Initializes the driver, processes the specified arguments and reports the
 * clusters found.
 */
void process_args(int argc, char** argv)
{
  size_t i;

  memset(&recorded_dirs, 0, sizeof(DirList));

  for (i = 0;  i < BUCKET_COUNT;  i++)
    init_file_list(&buckets[i]);

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
    char* path;

    /* Read file names from stdin */
    while ((path = read_path(stdin)))
    {
      kill_trailing_slashes(path);
      process_path(path, 0);
      free(path);
    }
  }

  if (unique_files_flag)
    process_uniques();
  else
    process_clusters();

  for (i = 0;  i < BUCKET_COUNT;  i++)
    free_file_list(&buckets[i]);

  free(recorded_dirs.dirs);
  memset(&recorded_dirs, 0, sizeof(DirList));
}

/* Stat:s a file according to the specified options.
 */
static int stat_path(const char* path, struct stat* sb, int depth)
{
  if (*path == '\0')
    return -1;

  if (lstat(path, sb) != 0)
  {
    if (!quiet_flag)
      warning("%s: %s", path, strerror(errno));

    return -1;
  }

  if (S_ISLNK(sb->st_mode))
  {
    if (follow_links_mode == ALL_SYMLINKS ||
        (depth == 0 && follow_links_mode == ARG_SYMLINKS))
    {
      if (stat(path, sb) != 0)
      {
        if (!quiet_flag)
          warning("%s: %s", path, strerror(errno));

        return -1;
      }

      if (S_ISDIR(sb->st_mode))
        return -1;
    }
    else
      return -1;
  }

  return 0;
}

/* Returns true if the directory has already been recorded.
 * TODO: Implement a more efficient data structure.
 */
static int has_recorded_directory(dev_t device, ino_t inode)
{
  size_t i;
  const Dir* dirs = recorded_dirs.dirs;

  for (i = 0;  i < recorded_dirs.allocated;  i++)
  {
    if (dirs[i].device == device && dirs[i].inode == inode)
      return 1;
  }

  return 0;
}

/* Records the specified directory.
 * TODO: Implement a more efficient data structure.
 */
static void record_directory(dev_t device, ino_t inode)
{
  if (recorded_dirs.allocated == recorded_dirs.available)
  {
    size_t count;

    if (recorded_dirs.available)
      count = recorded_dirs.available * 2;
    else
      count = 1024;

    recorded_dirs.dirs = realloc(recorded_dirs.dirs, count * sizeof(Dir));
    if (recorded_dirs.dirs == NULL)
      error(_("Out of memory"));

    recorded_dirs.available = count;
  }

  recorded_dirs.dirs[recorded_dirs.allocated].device = device;
  recorded_dirs.dirs[recorded_dirs.allocated].inode = inode;
  recorded_dirs.allocated++;
}

/* Recurses into a directory, collecting all or all non-hidden files,
 * according to the specified options.
 */
static void process_directory(const char* path,
                              const struct stat* sb,
                              int depth)
{
  DIR* dir;
  struct dirent* dir_entry;
  char* child_path;
  const char* name;

  if (has_recorded_directory(sb->st_dev, sb->st_ino))
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

    if (asprintf(&child_path, "%s/%s", path, name) < 0)
        error(_("Out of memory"));

    process_path(child_path, depth);
    free(child_path);
  }

  closedir(dir);
}

/* Processes a single file.
 */
static void process_file(const char* path, struct stat* sb)
{
  if (sb->st_size == 0)
  {
    if (ignore_empty_flag)
      return;
  }

  /* NOTE: Check for duplicate arguments? */

  if (physical_flag)
  {
    /* TODO: Make this less pessimal */

    size_t i, bucket = BUCKET_INDEX(sb->st_size);

    for (i = 0;  i < buckets[bucket].allocated;  i++)
    {
      if (buckets[bucket].files[i].device == sb->st_dev &&
          buckets[bucket].files[i].inode == sb->st_ino)
      {
        return;
      }
    }
  }

  init_file(alloc_file(&buckets[BUCKET_INDEX(sb->st_size)]), path, sb);
}

/* Processes a path name according to its type, whether from the command line or
 * from directory recursion.
 *
 * This function calls process_file and process_directory as needed.
 */
static void process_path(const char* path, int depth)
{
  mode_t mode;
  struct stat sb;

  if (stat_path(path, &sb, depth) != 0)
    return;

  mode = sb.st_mode & S_IFMT;
  switch (mode)
  {
    case S_IFREG:
    {
      process_file(path, &sb);
      break;
    }

    case S_IFDIR:
    {
      if (recursive_flag)
      {
        process_directory(path, &sb, depth + 1);
        break;
      }

      /* FALLTHROUGH */
    }

    default:
    {
      if (!quiet_flag)
      {
        switch (mode)
        {
          case S_IFLNK:
            warning(_("%s is a symbolic link; skipping"), path);
            break;
          case S_IFIFO:
            warning(_("%s is a named pipe; skipping"), path);
            break;
          case S_IFBLK:
            warning(_("%s is a block device; skipping"), path);
            break;
          case S_IFCHR:
            warning(_("%s is a character device; skipping"), path);
            break;
          case S_IFDIR:
            warning(_("%s is a directory; skipping"), path);
            break;
          case S_IFSOCK:
            warning(_("%s is a socket; skipping"), path);
            break;
          default:
            error(_("This cannot happen"));
        }
      }
    }
  }
}

/* Reports a cluster to stdout, according to the specified options.
 */
static void report_cluster(const FileList* cluster, unsigned int index)
{
  size_t i;
  File* files = cluster->files;

  if (excess_flag)
  {
    /* Report all but the first file in the cluster */
    for (i = 1;  i < cluster->allocated;  i++)
    {
      printf("%s", files[i].path);
      putchar(get_field_terminator());
    }
  }
  else
  {
    /* Print header and report all files in the cluster */

    if (*header_format != '\0')
    {
      if (header_uses_digest)
        generate_file_digest(files);

      print_cluster_header(header_format,
                           cluster->allocated,
                           index,
                           files->size,
                           files->digest);

      putchar(get_field_terminator());
    }

    for (i = 0;  i < cluster->allocated;  i++)
    {
      printf("%s", files[i].path);
      putchar(get_field_terminator());
    }
  }
}

/* Finds and reports all duplicate clusters in each bucket of collected files.
 */
static void process_clusters(void)
{
  size_t i, j, d, first, second, index = 1;
  FileList duplicates;

  init_file_list(&duplicates);

  for (i = 0;  i < BUCKET_COUNT;  i++)
  {
    File* files = buckets[i].files;

    /* quick skip for single piece bucket */
    if (buckets[i].allocated < 2) continue;

    for (first = 0;  first < buckets[i].allocated;  first++)
    {
      if (files[first].status == INVALID ||
          files[first].status == DUPLICATE)
      {
        continue;
      }

      for (second = first + 1;  second < buckets[i].allocated;  second++)
      {
        if (files[second].status == INVALID ||
            files[second].status == DUPLICATE)
        {
            continue;
        }

        if (compare_files(&files[first], &files[second]) == 0)
        {
          if (duplicates.allocated == 0)
          {
            *alloc_file(&duplicates) = files[first];
            files[first].status = DUPLICATE;
          }

          *alloc_file(&duplicates) = files[second];
          files[second].status = DUPLICATE;
        }
        else
        {
          if (files[first].status == INVALID)
            break;
        }
      }

      if (duplicates.allocated > 0)
      {
        if (physical_cluster_flag)
        {
          ino_t prev_inode = duplicates.files[0].inode;
          dev_t prev_dev = duplicates.files[0].device;
          for (d = 1;  d < duplicates.allocated;  d++) /* assume that duplicates count > 1 */
          {
            if (duplicates.files[d].inode != prev_inode || duplicates.files[d].device != prev_dev)
            {
              report_cluster(&duplicates, index);
              break;
            }

            prev_inode = duplicates.files[d].inode;
            prev_dev = duplicates.files[d].device;
          }
        }
        else
        {
          report_cluster(&duplicates, index);
        }

        empty_file_list(&duplicates);

        index++;
      }
    }

    for (j = 0;  j < buckets[i].allocated;  j++)
    {
      free_file(&files[j]);
    }
    free_file_list(&buckets[i]);
  }

  free_file_list(&duplicates);
}

/* Finds and reports all unique files in each bucket of collected files.
 */
static void process_uniques(void)
{
  size_t i, first, second;

  for (i = 0;  i < BUCKET_COUNT;  i++)
  {
    File* files = buckets[i].files;

    for (first = 0;  first < buckets[i].allocated;  first++)
    {
      if (files[first].status == INVALID ||
          files[first].status == DUPLICATE)
      {
        continue;
      }

      for (second = first + 1;  second < buckets[i].allocated;  second++)
      {
        if (files[second].status == INVALID ||
            files[second].status == DUPLICATE)
        {
            continue;
        }

        if (compare_files(&files[first], &files[second]) == 0)
        {
          files[first].status = DUPLICATE;
          files[second].status = DUPLICATE;
        }
      }

      if (files[first].status != INVALID &&
          files[first].status != DUPLICATE)
      {
        printf("%s", files[first].path);
        putchar(get_field_terminator());
      }
    }
  }
}

