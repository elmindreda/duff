/*
 * duff - Duplicate file finder
 * Copyright (c) 2005 Camilla Berglund <elmindreda@elmindreda.org>
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

#if HAVE_INTTYPES_H
#include <inttypes.h>
#elif HAVE_STDINT_H
#include <stdint.h>
#endif

#if HAVE_ASSERT_H
#include <assert.h>
#endif

#include "duff.h"

/* These flags are defined and documented in duff.c.
 */
extern int quiet_flag;
extern int physical_flag;
extern int thorough_flag;
extern off_t sample_limit;

/* These functions are documented below, where they are defined.
 */
static int get_entry_sample(Entry* entry);
static int get_entry_digest(Entry* entry);
static int compare_entry_digests(Entry* first, Entry* second);
static int compare_entry_samples(Entry* first, Entry* second);
static int compare_entry_contents(Entry* first, Entry* second);

/* Initialises the specified entry.
 */
void fill_entry(Entry* entry, const char* path, const struct stat* sb)
{
  entry->path = strdup(path);
  entry->size = sb->st_size;
  entry->device = sb->st_dev;
  entry->inode = sb->st_ino;
  entry->status = UNTOUCHED;
  entry->digest = NULL;
  entry->sample = NULL;
}

/* Frees any memory allocated for the specified entry.
 */
void free_entry(Entry* entry)
{
  free(entry->digest);
  free(entry->sample);
  free(entry->path);
}

/* Retrieves sample from a file, if needed.
 */
static int get_entry_sample(Entry* entry)
{
  FILE* file;
  size_t size;
  uint8_t* sample;

  if (entry->status == INVALID)
    return -1;
  if (entry->sample)
    return 0;

  file = fopen(entry->path, "rb");
  if (!file)
  {
    if (!quiet_flag)
      warning("%s: %s", entry->path, strerror(errno));

    entry->status = INVALID;
    return -1;
  }

  size = SAMPLE_SIZE;
  if (size > entry->size)
    size = entry->size;

  sample = (uint8_t*) malloc(size);

  if (fread(sample, size, 1, file) < 1)
  {
    if (!quiet_flag)
      warning("%s: %s", entry->path, strerror(errno));

    free(sample);
    fclose(file);
    return -1;
  }

  entry->sample = sample;

  fclose(file);
  return 0;
}

/* Calculates the digest of a file, if needed.
 */
static int get_entry_digest(Entry* entry)
{
  FILE* file;
  size_t size;
  char buffer[8192];

  if (entry->status == INVALID)
    return -1;
  if (entry->digest)
    return 0;

  digest_init();

  if (entry->sample && entry->size <= SAMPLE_SIZE)
    digest_update(entry->sample, entry->size);
  else if (entry->size > 0)
  {
    file = fopen(entry->path, "rb");
    if (!file)
    {
      if (!quiet_flag)
        warning("%s: %s", entry->path, strerror(errno));

      entry->status = INVALID;
      return -1;
    }

    for (;;)
    {
      size = fread(buffer, 1, sizeof(buffer), file);
      if (ferror(file))
      {
        if (!quiet_flag)
          warning("%s: %s", entry->path, strerror(errno));

        fclose(file);

        entry->status = INVALID;
        return -1;
      }

      if (size == 0)
        break;

      digest_update(buffer, size);
    }

    fclose(file);
  }

  entry->digest = (uint8_t*) malloc(get_digest_size());
  digest_finish(entry->digest);

  return 0;
}

/* This function defines the high-level comparison algorithm, using
 * lower level primitives.  This is the place to change or add
 * calls to comparison modes.  The general idea is to find proof of
 * equality or un-equality as early and as quickly as possible.
 */
int compare_entries(Entry* first, Entry* second)
{
  if (first->size != second->size)
    return -1;

  if (first->size == 0)
    return 0;

  if (!physical_flag)
  {
    if (first->device == second->device && first->inode == second->inode)
      return 0;
  }

  if (first->size >= sample_limit)
  {
    if (compare_entry_samples(first, second) != 0)
      return -1;

    if (first->size <= SAMPLE_SIZE)
      return 0;
  }

  if (thorough_flag)
  {
    if (compare_entry_contents(first, second) != 0)
      return -1;
  }
  else
  {
    /* NOTE: Skip calculating digests if potential cluster only has two entries?
     * NOTE: Requires knowledge from higher level */
    if (compare_entry_digests(first, second) != 0)
      return -1;
  }

  return 0;
}

/* Generates the digest for the specified entry if it's not already present.
 */
void generate_entry_digest(Entry* entry)
{
  get_entry_digest(entry);
}

/* Compares the digests of two files, calculating them if neccessary.
 */
static int compare_entry_digests(Entry* first, Entry* second)
{
  int i, digest_size;

  if (get_entry_digest(first) != 0)
    return -1;

  if (get_entry_digest(second) != 0)
    return -1;

  digest_size = get_digest_size();
  for (i = 0;  i < digest_size;  i++)
    if (first->digest[i] != second->digest[i])
      return -1;

  return 0;
}

/* Compares the samples of two files, retrieving them if neccessary.
 */
static int compare_entry_samples(Entry* first, Entry* second)
{
  if (get_entry_sample(first) != 0)
    return -1;

  if (get_entry_sample(second) != 0)
    return -1;

  size_t size = SAMPLE_SIZE;
  if (size > first->size)
    size = first->size;

  if (memcmp(first->sample, second->sample, size) != 0)
    return -1;

  return 0;
}

/* Performs byte-by-byte comparison of the contents of two files.
 * This is the action we most want to avoid ever having to do.
 * It is also completely un-optmimised.  Enjoy.
 * NOTE: This function assumes that the files are of equal size, as
 * there's little point in calling it otherwise.
 * TODO: Use a read buffer.
 */
static int compare_entry_contents(Entry* first, Entry* second)
{
  int fc, sc, result = 0;
  FILE* first_stream;
  FILE* second_stream;

  if (first->size == 0)
    return 0;

  first_stream = fopen(first->path, "rb");
  second_stream = fopen(second->path, "rb");

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

