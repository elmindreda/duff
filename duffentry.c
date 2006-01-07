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

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#if HAVE_ASSERT_H
#include <assert.h>
#endif

#include "sha1.h"
#include "duff.h"

/* These flags are defined and documented in duff.c.
 */
extern int quiet_flag;
extern int thorough_flag;
extern off_t sample_limit;

/* These functions are documented below, where they are defined.
 */
static int get_entry_samples(struct Entry* entry);
static int get_entry_checksum(struct Entry* entry);
static int compare_entry_checksums(struct Entry* first, struct Entry* second);
static int compare_entry_samples(struct Entry* first, struct Entry* second);
static int compare_entry_contents(struct Entry* first, struct Entry* second);

/* Allocates and initialises an entry.
 */
struct Entry* make_entry(const char* path, const struct stat* sb)
{
  struct Entry* entry;

  entry = (struct Entry*) malloc(sizeof(struct Entry));
  entry->prev = NULL;
  entry->next = NULL;
  entry->path = strdup(path);
  entry->size = sb->st_size;
  entry->device = sb->st_dev;
  entry->inode = sb->st_ino;
  entry->status = UNTOUCHED;
  entry->checksum = NULL;
  entry->samples = NULL;
  
  return entry;
}

/* Makes a deep copy of an entry.
 */
struct Entry* copy_entry(struct Entry* entry)
{
  struct Entry* copy = (struct Entry*) malloc(sizeof(struct Entry));
  
  copy->prev = NULL;
  copy->next = NULL;
  copy->path = strdup(entry->path);
  copy->size = entry->size;
  copy->device = entry->device;
  copy->inode = entry->inode;
  copy->status = entry->status;

  if (entry->checksum)
  {
    copy->checksum = (uint8_t*) malloc(SHA1_HASH_SIZE);
    memcpy(copy->checksum, entry->checksum, SHA1_HASH_SIZE);
  }
  else
    copy->checksum = NULL;

  if (entry->samples)
  {
    copy->samples = (uint8_t*) malloc(SAMPLE_COUNT);
    memcpy(copy->samples, entry->samples, SAMPLE_COUNT);
  }
  else
    copy->samples = NULL;
  
  return copy;
}

/* Inserts an entry as the first item in a list.
 * Note that the entry must be detached from any previous list.
 */
void link_entry(struct Entry** head, struct Entry* entry)
{
  assert(entry->prev == NULL);
  assert(entry->next == NULL);

  entry->prev = NULL;
  entry->next = *head;

  if (*head != NULL)
    (*head)->prev = entry;

  *head = entry;
}

/* Removes an entry from a list.
 * Note that the entry must be a member of the list.
 */
void unlink_entry(struct Entry** head, struct Entry* entry)
{
  if (entry->prev != NULL)
    entry->prev->next = entry->next;
  else
    *head = entry->next;

  if (entry->next != NULL)
    entry->next->prev = entry->prev;

  entry->prev = entry->next = NULL;
}

/* Frees an entry and any dynamically allocated members.
 */
void free_entry(struct Entry* entry)
{
  assert(entry->prev == NULL);
  assert(entry->next == NULL);

  free(entry->samples);
  free(entry->checksum);
  free(entry->path);
  free(entry);
}

/* Frees a list of entries.
 * On exit, the specified head is set to NULL.
 */
void free_entry_list(struct Entry** entries)
{
  struct Entry* entry;
  
  while (*entries)
  {
    entry = *entries;
    unlink_entry(entries, entry);
    free_entry(entry);
  }
}

/* Retrieves samples from a file, if needed.
 */
static int get_entry_samples(struct Entry* entry)
{
  int i;
  FILE* file;
  uint8_t* samples;
  
  if (entry->status == INVALID)
    return -1;
  if (entry->samples)
    return 0;

  file = fopen(entry->path, "rb");
  if (!file)
  {
    if (!quiet_flag)
      warning("%s: %s", entry->path, strerror(errno));
      
    entry->status = INVALID;
    return -1;
  }

  samples = (uint8_t*) malloc(SAMPLE_COUNT);
  
  for (i = 0;  i < SAMPLE_COUNT;  i++)
  {
    fseek(file, i * entry->size / SAMPLE_COUNT, SEEK_SET);
    fread(samples + i, 1, 1, file);

    if (ferror(file))
    {
      if (!quiet_flag)
        warning("%s: %s", entry->path, strerror(errno));

      free(samples);
      fclose(file);
      return -1;
    }
  }

  entry->samples = samples;

  fclose(file);
  return 0;
}

/* Calculates the checksum of a file, if needed.
 */
static int get_entry_checksum(struct Entry* entry)
{
  FILE* file;
  size_t size;
  char buffer[8192];
  SHA1Context context;
  
  if (entry->status == INVALID)
    return -1;
  if (entry->checksum)
    return 0;
  
  file = fopen(entry->path, "rb");
  if (!file)
  {
    if (!quiet_flag)
      warning("%s: %s", entry->path, strerror(errno));
    
    entry->status = INVALID;
    return -1;
  }

  SHA1Init(&context);

  for (;;)
  {
    size = fread(buffer, 1, sizeof(buffer), file);
    if (ferror(file))
    {
      fclose(file);
  
      if (!quiet_flag)
        warning("%s: %s", entry->path, strerror(errno));

      entry->status = INVALID;
      return -1;
    }

    if (size == 0)
      break;

    SHA1Update(&context, buffer, size);
  }

  fclose(file);

  entry->checksum = (uint8_t*) malloc(SHA1_HASH_SIZE);
  SHA1Final(&context, entry->checksum);
  return 0;
}

/* This function defines the high-level comparison algorithm, using
 * lower level primitives.  This is the place to change or add
 * calls to comparison modes.  The general idea is to find proof of
 * un-equality as soon and as quickly as possible.
 */
int compare_entries(struct Entry* first, struct Entry* second)
{
  if (first->size != second->size)
    return -1;

  if (first->size >= sample_limit)
  {
    if (compare_entry_samples(first, second) != 0)
      return -1;
  }

  /* TODO: Skip checksumming if potential cluster only has two entries 
   * NOTE: Requires knowledge from higher level */
  if (compare_entry_checksums(first, second) != 0)
    return -1;

  if (thorough_flag)
  {
    if (compare_entry_contents(first, second) != 0)
      return -1;
  }

  return 0;
}

/* Compares the checksums of two files, generating them if neccessary.
 */
static int compare_entry_checksums(struct Entry* first, struct Entry* second)
{
  int i;

  if (get_entry_checksum(first) != 0)
    return -1;

  if (get_entry_checksum(second) != 0)
    return -1;

  for (i = 0;  i < SHA1_HASH_SIZE;  i++)
    if (first->checksum[i] != second->checksum[i])
      return -1;

  return 0;
}

/* Compares the samples of two files, retrieving them if neccessary.
 */
static int compare_entry_samples(struct Entry* first, struct Entry* second)
{
  int i;

  if (get_entry_samples(first) != 0)
    return -1;

  if (get_entry_samples(second) != 0)
    return -1;

  for (i = 0;  i < SAMPLE_COUNT;  i++)
    if (first->samples[i] != second->samples[i])
      return -1;

  return 0;
}

/* Performs byte-by-byte comparison of the contents of two files.
 * This is the action we most want to avoid ever having to do.
 * It is also completely un-optmimised.  Enjoy.
 * NOTE: This function assumes that the files are of equal size, as
 * there's little point in calling it otherwise.
 */
static int compare_entry_contents(struct Entry* first, struct Entry* second)
{
  int fc, sc, result = 0;
  FILE* first_stream = fopen(first->path, "rb");
  FILE* second_stream = fopen(second->path, "rb");

  if (!first_stream || !second_stream)
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

