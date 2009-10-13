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

/* Macros to get 64-bit off_t
 */
#define _GNU_SOURCE 1
#define _FILE_OFFSET_BITS 64

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
#include "sha256.h"
#include "sha384.h"
#include "sha512.h"

#include "duff.h"

/* These flags are defined and documented in duff.c.
 */
extern int quiet_flag;
extern int thorough_flag;
extern off_t sample_limit;
extern enum Function digest_function;

/* These functions are documented below, where they are defined.
 */
static int get_entry_samples(Entry* entry);
static int get_entry_digest(Entry* entry);
static int get_entry_digest_sha1(uint8_t** result, FILE* file);
static int get_entry_digest_sha256(uint8_t** result, FILE* file);
static int get_entry_digest_sha384(uint8_t** result, FILE* file);
static int get_entry_digest_sha512(uint8_t** result, FILE* file);
static int compare_entry_digests(Entry* first, Entry* second);
static int compare_entry_samples(Entry* first, Entry* second);
static int compare_entry_contents(Entry* first, Entry* second);

/* Allocates and initialises an entry.
 */
Entry* make_entry(const char* path, const struct stat* sb)
{
  Entry* entry;

  entry = (Entry*) malloc(sizeof(Entry));
  entry->prev = NULL;
  entry->next = NULL;
  entry->path = strdup(path);
  entry->size = sb->st_size;
  entry->device = sb->st_dev;
  entry->inode = sb->st_ino;
  entry->status = UNTOUCHED;
  entry->digest = NULL;
  entry->samples = NULL;

  return entry;
}

/* Inserts an entry as the first item in a list.
 * Note that the entry must be detached from any previous list.
 */
void link_entry(Entry** head, Entry* entry)
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
void unlink_entry(Entry** head, Entry* entry)
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
void free_entry(Entry* entry)
{
  assert(entry->prev == NULL);
  assert(entry->next == NULL);

  free(entry->samples);
  free(entry->digest);
  free(entry->path);
  free(entry);
}

/* Frees a list of entries.
 * On exit, the specified head is set to NULL.
 */
void free_entry_list(Entry** entries)
{
  Entry* entry;

  while (*entries)
  {
    entry = *entries;
    unlink_entry(entries, entry);
    free_entry(entry);
  }
}

/* Retrieves samples from a file, if needed.
 */
static int get_entry_samples(Entry* entry)
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

    if (fread(samples + i, 1, 1, file) < 1)
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

/* Calculates the digest of a file, if needed.
 */
static int get_entry_digest(Entry* entry)
{
  FILE* file;
  int result;

  if (entry->status == INVALID)
    return -1;
  if (entry->digest)
    return 0;

  file = fopen(entry->path, "rb");
  if (!file)
  {
    if (!quiet_flag)
      warning("%s: %s", entry->path, strerror(errno));

    entry->status = INVALID;
    return -1;
  }

  switch (digest_function)
  {
    case SHA_1:
      result = get_entry_digest_sha1(&(entry->digest), file);
      break;

    case SHA_256:
      result = get_entry_digest_sha256(&(entry->digest), file);
      break;

    case SHA_384:
      result = get_entry_digest_sha384(&(entry->digest), file);
      break;

    case SHA_512:
      result = get_entry_digest_sha512(&(entry->digest), file);
      break;

    default:
      error(gettext("This cannot happen"));
  }

  fclose(file);

  if (result)
  {
    if (!quiet_flag)
      warning("%s: %s", entry->path, strerror(errno));

    entry->status = INVALID;
    return -1;
  }

  return 0;
}

/* Calculates and returns the SHA-1 digest of the specified file.
 */
static int get_entry_digest_sha1(uint8_t** result, FILE* file)
{
  size_t size;
  char buffer[8192];
  SHA1Context context;

  SHA1Init(&context);

  for (;;)
  {
    size = fread(buffer, 1, sizeof(buffer), file);
    if (ferror(file))
      return -1;

    if (size == 0)
      break;

    SHA1Update(&context, buffer, size);
  }

  *result = (uint8_t*) malloc(SHA1_HASH_SIZE);
  SHA1Final(&context, *result);
  return 0;
}

/* Calculates and returns the SHA-256 digest of the specified file.
 */
int get_entry_digest_sha256(uint8_t** result, FILE* file)
{
  size_t size;
  char buffer[8192];
  SHA256Context context;

  SHA256Init(&context);

  for (;;)
  {
    size = fread(buffer, 1, sizeof(buffer), file);
    if (ferror(file))
      return -1;

    if (size == 0)
      break;

    SHA256Update(&context, buffer, size);
  }

  *result = (uint8_t*) malloc(SHA256_HASH_SIZE);
  SHA256Final(&context, *result);
  return 0;
}

/* Calculates and returns the SHA-384 digest of the specified file.
 */
int get_entry_digest_sha384(uint8_t** result, FILE* file)
{
  size_t size;
  char buffer[8192];
  SHA384Context context;

  SHA384Init(&context);

  for (;;)
  {
    size = fread(buffer, 1, sizeof(buffer), file);
    if (ferror(file))
      return -1;

    if (size == 0)
      break;

    SHA384Update(&context, buffer, size);
  }

  *result = (uint8_t*) malloc(SHA384_HASH_SIZE);
  SHA384Final(&context, *result);
  return 0;
}

/* Calculates and returns the SHA-512 digest of the specified file.
 */
int get_entry_digest_sha512(uint8_t** result, FILE* file)
{
  size_t size;
  char buffer[8192];
  SHA512Context context;

  SHA512Init(&context);

  for (;;)
  {
    size = fread(buffer, 1, sizeof(buffer), file);
    if (ferror(file))
      return -1;

    if (size == 0)
      break;

    SHA512Update(&context, buffer, size);
  }

  *result = (uint8_t*) malloc(SHA512_HASH_SIZE);
  SHA512Final(&context, *result);
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

  if (first->size >= sample_limit)
  {
    if (compare_entry_samples(first, second) != 0)
      return -1;
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

