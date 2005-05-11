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

#include "duff.h"

extern int quiet_flag;
extern int thorough_flag;

struct Entry* make_entry(const char* path, size_t size)
{
  struct Entry* entry;

  entry = (struct Entry*) malloc(sizeof(struct Entry));
  entry->next = NULL;
  entry->path = strdup(path);
  entry->size = size;
  entry->checksum = 0;
  entry->status = FRESH;
  
  return entry;
}

struct Entry* copy_entry(struct Entry* entry)
{
  struct Entry* copy = (struct Entry*) malloc(sizeof(struct Entry));
  
  copy->next = NULL;
  copy->path = strdup(entry->path);
  copy->size = entry->size;
  copy->checksum = entry->checksum;
  copy->status = entry->status;
  
  return copy;
}

void free_entry(struct Entry* entry)
{
  free(entry->path);
  free(entry);
}

void free_entry_list(struct Entry** entries)
{
  struct Entry* entry;
  
  while (*entries)
  {
    entry = *entries;
    *entries = entry->next;
    free_entry(entry);
  }
}

int get_entry_checksum(struct Entry* entry)
{
  int c;
  long checksum = 0;
  FILE* file;
  
  if (entry->status != FRESH)
    return 0;
  
  file = fopen(entry->path, "rb");
  if (!file)
  {
    if (!quiet_flag)
      warning("%s: %s", entry->path, strerror(errno));
    
    entry->status = INVALID;
    return -1;
  }
  
  while ((c = fgetc(file)) != EOF)
  {
    if (ferror(file))
    {
      fclose(file);
  
      if (!quiet_flag)
        warning("%s: %s", entry->path, strerror(errno));
	
      entry->status = INVALID;
      return -1;
    }
    
    /* TODO: Insert algorithm here. */
    checksum += c;
  }
  
  fclose(file);
  
  entry->checksum = checksum;
  entry->status = CHECKED;
  return 0;
}

int compare_entries(struct Entry* first, struct Entry* second)
{
  if (first->size != second->size)
    return -1;
    
  if (get_entry_checksum(first) != 0)
    return -1;

  if (get_entry_checksum(second) != 0)
    return -1;

  if (first->checksum != second->checksum)
    return -1;

  if (thorough_flag)
  {
    if (compare_entry_contents(first, second))
      return -1;
  }

  return 0;
}

int compare_entry_contents(struct Entry* first, struct Entry* second)
{
  int fc, sc, result = 0;
  FILE* first_stream = fopen(first->path, "rb");
  FILE* second_stream = fopen(second->path, "rb");

  if (!first_stream || second_stream)
  {
    if (first_stream)
      fclose(first_stream);
    if (second_stream)
      fclose(second_stream);
    return -1;
  }

  do
  {
    fc = fgetc(first_stream);
    sc = fgetc(second_stream);
    if (fc != sc)
    {
      result = -1;
      break;
    }
  }
  while (fc != EOF);

  fclose(first_stream);
  fclose(second_stream);
  return result;
}

