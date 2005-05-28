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

#if HAVE_DIRENT_H
#include <dirent.h>
#endif

#include "duffstring.h"
#include "duff.h"

int all_flag = 0;
int verbose_flag = 0;
int recursive_flag = 0;
int quiet_flag = 0;
int excess_flag = 0;
int thorough_flag = 0;
const char* format_flag = "%c files in cluster %n (%s bytes, checksum %k)";

static struct Entry* file_entries = NULL;
static struct Cluster* file_clusters = NULL;
static unsigned long file_count = 0;

void process_path(const char* path)
{
  mode_t mode;
  struct stat sb;
  struct Entry* file_entry;
  DIR* dir;
  struct dirent* dir_entry;
  char* entry_path;
  const char* name;

  name = strrchr(path, '/');
  if (name)
    name++;
  else
    name = path;

  if (strcmp(name, ".") == 0)
    return;

  if (strcmp(name, "..") == 0)
    return;

  if (stat(path, &sb) != 0)
  {
    if (!quiet_flag)
      warning("%s: %s", path, strerror(errno));

    return;
  }

  mode = sb.st_mode & S_IFMT;
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
	dir = opendir(path);
	if (!dir)
	  return;

	while (dir_entry = readdir(dir))
	{
	  if (!all_flag)
	  {
	    if (dir_entry->d_name[0] == '.')
	      continue;
	  }

	  asprintf(&entry_path, "%s/%s", path, dir_entry->d_name);
	  process_path(entry_path);
	  free(entry_path);
	}
	
	closedir(dir);
	dir = NULL;
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

int main(int argc, char** argv)
{
  int i, ch, number, count = 0;
  struct Entry* base;
  struct Entry* entry;
  struct Entry* copy;
  struct Entry* duplicates = NULL;
  
  while ((ch = getopt(argc, argv, "avrqhef:")) != -1)
  {
    switch (ch)
    {
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
	break;
      case 't':
	thorough_flag = 1;
	break;
      case 'f':
	format_flag = optarg;
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
    process_path(argv[i]);
  
  number = 1;
  
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
    
    if (duplicates)
    {
      if (*format_flag != '\0')
	print_cluster_header(format_flag,
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

