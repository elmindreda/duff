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

#include "duff.h"

/* These flags are defined and documented in duff.c.
 */
extern int same_device_flag;
extern int quiet_flag;
extern int thorough_flag;
extern off_t sample_limit;

/* These functions are documented below, where they are defined.
 */
static int get_file_sample(File* file);
static int get_file_digest(File* file);
static int compare_file_digests(File* first, File* second);
static int compare_file_samples(File* first, File* second);
static int compare_file_contents(File* first, File* second);

/* Initialises the specified file.
 */
void init_file(File* file, const char* path, const struct stat* sb)
{
  file->path = strdup(path);
  file->size = sb->st_size;
  file->device = sb->st_dev;
  file->inode = sb->st_ino;
  file->status = UNTOUCHED;
  file->digest = NULL;
  file->sample = NULL;
}

/* Frees any memory allocated for the specified file.
 */
void free_file(File* file)
{
  free(file->digest);
  free(file->sample);
  free(file->path);
}

/* This function defines the high-level comparison algorithm, using
 * lower level primitives.  This is the place to change or add
 * calls to comparison modes.  The general idea is to find proof of
 * equality or un-equality as early and as quickly as possible.
 */
int compare_files(File* first, File* second)
{
  if (first->size != second->size)
    return -1;

  if (first->size == 0)
    return 0;

  if (first->device == second->device && first->inode == second->inode)
    return 0;

  if (same_device_flag)
  {
    if (first->device != second->device)
      return -1;
  }

  if (first->size >= sample_limit)
  {
    if (compare_file_samples(first, second) != 0)
      return -1;

    if (first->size <= SAMPLE_SIZE)
      return 0;
  }

  if (thorough_flag)
  {
    if (compare_file_contents(first, second) != 0)
      return -1;
  }
  else
  {
    /* NOTE: Skip calculating digests if potential cluster only has two files?
     * NOTE: Requires knowledge from higher level */
    if (compare_file_digests(first, second) != 0)
      return -1;
  }

  return 0;
}

/* Generates the digest for the specified file if it's not already present.
 */
void generate_file_digest(File* file)
{
  get_file_digest(file);
}

/* Retrieves sample from a file, if needed.
 */
static int get_file_sample(File* file)
{
  FILE* stream;
  size_t size;
  uint8_t* sample;

  if (file->status == SAMPLED || file->status == HASHED)
    return 0;

  stream = fopen(file->path, "rb");
  if (!stream)
  {
    if (!quiet_flag)
      warning("%s: %s", file->path, strerror(errno));

    file->status = INVALID;
    return -1;
  }

  size = SAMPLE_SIZE;
  if (size > file->size)
    size = file->size;

  sample = (uint8_t*) malloc(size);

  if (fread(sample, size, 1, stream) < 1)
  {
    if (!quiet_flag)
      warning("%s: %s", file->path, strerror(errno));

    free(sample);
    fclose(stream);

    file->status = INVALID;
    return -1;
  }

  fclose(stream);

  file->sample = sample;
  file->status = SAMPLED;
  return 0;
}

/* Calculates the digest of a file, if needed.
 */
static int get_file_digest(File* file)
{
  FILE* stream;
  size_t size;
  char buffer[BUFFER_SIZE];

  if (file->status == HASHED)
    return 0;

  digest_init();

  if (file->status == SAMPLED && file->size <= SAMPLE_SIZE)
    digest_update(file->sample, file->size);
  else if (file->size > 0)
  {
    stream = fopen(file->path, "rb");
    if (!stream)
    {
      if (!quiet_flag)
        warning("%s: %s", file->path, strerror(errno));

      file->status = INVALID;
      return -1;
    }

    for (;;)
    {
      size = fread(buffer, 1, sizeof(buffer), stream);
      if (ferror(stream))
      {
        if (!quiet_flag)
          warning("%s: %s", file->path, strerror(errno));

        fclose(stream);

        file->status = INVALID;
        return -1;
      }

      if (size == 0)
        break;

      digest_update(buffer, size);
    }

    fclose(stream);
  }

  file->digest = (uint8_t*) malloc(get_digest_size());
  digest_finish(file->digest);

  file->status = HASHED;
  return 0;
}

/* Compares the digests of two files, calculating them if neccessary.
 */
static int compare_file_digests(File* first, File* second)
{
  if (get_file_digest(first) != 0)
    return -1;

  if (get_file_digest(second) != 0)
    return -1;

  if (memcmp(first->digest, second->digest, get_digest_size()) != 0)
    return -1;

  return 0;
}

/* Compares the samples of two files, retrieving them if neccessary.
 */
static int compare_file_samples(File* first, File* second)
{
  if (get_file_sample(first) != 0)
    return -1;

  if (get_file_sample(second) != 0)
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
static int compare_file_contents(File* first, File* second)
{
  int fc, sc;
  off_t count = 0;
  FILE* first_stream;
  FILE* second_stream;

  first_stream = fopen(first->path, "rb");
  if (!first_stream)
  {
    if (!quiet_flag)
      warning("%s: %s", first->path, strerror(errno));

    first->status = INVALID;
    return -1;
  }

  second_stream = fopen(second->path, "rb");
  if (!second_stream)
  {
    if (!quiet_flag)
      warning("%s: %s", second->path, strerror(errno));

    fclose(first_stream);

    second->status = INVALID;
    return -1;
  }

  for (;;)
  {
    fc = fgetc(first_stream);
    sc = fgetc(second_stream);

    if (fc != sc || fc == EOF)
      break;

    count++;
  }

  if (ferror(first_stream))
  {
    if (!quiet_flag)
      warning("%s: %s", first->path, strerror(errno));

    first->status = INVALID;
  }

  if (ferror(second_stream))
  {
    if (!quiet_flag)
      warning("%s: %s", second->path, strerror(errno));

    second->status = INVALID;
  }

  fclose(first_stream);
  fclose(second_stream);

  if (count != first->size)
    return -1;

  return 0;
}

