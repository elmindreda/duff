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

#if HAVE_STDIO_H
#include <stdio.h>
#endif

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#if HAVE_STDARG_H
#include <stdarg.h>
#endif

#if HAVE_INTTYPES_H
#include <inttypes.h>
#elif HAVE_STDINT_H
#include <stdint.h>
#endif

#if HAVE_STRING_H
#include <string.h>
#endif

#if HAVE_CTYPE_H
#include <ctype.h>
#endif

#include "sha1.h"
#include "sha256.h"
#include "sha384.h"
#include "sha512.h"

#include "duffstring.h"
#include "duff.h"

/* These flags are defined and documented in duff.c.
 */
extern int null_terminate_flag;
extern int human_readable_flag;

/* Message digest functions.
 */
enum Function
{
  SHA_1,
  SHA_256,
  SHA_384,
  SHA_512,
};

typedef enum Function Function;

/* The message digest function to use.
 */
static Function digest_function = SHA_1;

/* Represents a name of a digest function.
 */
struct FunctionName
{
  const char* name;
  int id;
};

typedef struct FunctionName FunctionName;

/* Supported digest function names.
 */
static FunctionName functions[] =
{
  { "sha1", SHA_1 },
  { "sha256", SHA_256 },
  { "sha384", SHA_384 },
  { "sha512", SHA_512 },
  { "sha-1", SHA_1 },
  { "sha-256", SHA_256 },
  { "sha-384", SHA_384 },
  { "sha-512", SHA_512 }
};

/* Union of all used SHA family contexts.
 */
union Context
{
  SHA1Context sha1;
  SHA256Context sha256;
  SHA384Context sha384;
  SHA512Context sha512;
};

/* The context(s) used by the digest helper functions.
 */
static union Context context;

/* Initializes a list for use.
 */
void init_file_list(FileList* list)
{
  memset(list, 0, sizeof(FileList));
}

/* Allocates and returns a single file within the specified list, resizing the
 * list as necessary.
 */
File* alloc_file(FileList* list)
{
  if (list->allocated == list->available)
  {
    size_t count;

    if (list->available)
      count = list->available * 2;
    else
      count = 128;

    list->files = realloc(list->files, count * sizeof(File));
    if (list->files == NULL)
      error(_("Out of memory"));

    list->available = count;
  }

  File* file = list->files + list->allocated;
  list->allocated++;
  return file;
}

/* Empties the list without freeing its allocated memory.
 */
void empty_file_list(FileList* list)
{
  list->allocated = 0;
}

/* Frees the memory allocated by the list and reinitializes it.
 */
void free_file_list(FileList* list)
{
  free(list->files);
  init_file_list(list);
}

/* Reads a path name from the specified stream according to the specified flags.
 */
char* read_path(FILE* stream)
{
  size_t capacity = 0, size = 0;
  char* path = NULL;
  char terminator = get_field_terminator();

  for (;;)
  {
    const int c = fgetc(stream);
    if (c == EOF && size == 0)
      return NULL;

    if (size == capacity)
    {
      path = realloc(path, capacity + PATH_SIZE_STEP);
      if (!path)
        error(_("Out of memory"));

      capacity += PATH_SIZE_STEP;
    }

    if (c == EOF || c == terminator)
      break;

    path[size++] = (char) c;
  }

  path[size] = '\0';
  return path;
}

/* Kills trailing slashes in the specified path (except if it's /).
 */
void kill_trailing_slashes(char* path)
{
  char* temp;

  while ((temp = strrchr(path, '/')))
  {
    if (temp == path || *(temp + 1) != '\0')
      break;
    *temp = '\0';
  }
}

/* Returns the current field terminator used for stdin and stdout.
 */
size_t get_field_terminator(void)
{
  if (null_terminate_flag)
    return '\0';
  else
    return '\n';
}

/* Sets the SHA family function to be used by the digest helpers.
 */
int set_digest_function(const char* name)
{
  int i;

  for (i = 0;  i < sizeof(functions) / sizeof(functions[0]);  i++)
  {
    if (strcasecmp(functions[i].name, name) == 0)
    {
      digest_function = functions[i].id;
      return 0;
    }
  }

  return -1;
}

/*! Returns the size, in bytes, of the specified digest type.
 */
size_t get_digest_size(void)
{
  switch (digest_function)
  {
    case SHA_1:
      return SHA1_HASH_SIZE;
    case SHA_256:
      return SHA256_HASH_SIZE;
    case SHA_384:
      return SHA384_HASH_SIZE;
    case SHA_512:
      return SHA512_HASH_SIZE;
  }

  error(_("This cannot happen"));
}

/* Initializes the context for the current function.
 */
void init_digest(void)
{
  switch (digest_function)
  {
    case SHA_1:
      SHA1Init(&context.sha1);
      return;
    case SHA_256:
      SHA256Init(&context.sha256);
      return;
    case SHA_384:
      SHA384Init(&context.sha384);
      return;
    case SHA_512:
      SHA512Init(&context.sha512);
      return;
  }

  error(_("This cannot happen"));
}

/* Updates the context for the current function.
 */
void update_digest(const void* data, size_t size)
{
  switch (digest_function)
  {
    case SHA_1:
      SHA1Update(&context.sha1, data, size);
      return;
    case SHA_256:
      SHA256Update(&context.sha256, data, size);
      return;
    case SHA_384:
      SHA384Update(&context.sha384, data, size);
      return;
    case SHA_512:
      SHA512Update(&context.sha512, data, size);
      return;
  }

  error(_("This cannot happen"));
}

/* Finalizes the digest of the chosen function.
 */
void finish_digest(uint8_t* digest)
{
  switch (digest_function)
  {
    case SHA_1:
      SHA1Final(&context.sha1, digest);
      return;
    case SHA_256:
      SHA256Final(&context.sha256, digest);
      return;
    case SHA_384:
      SHA384Final(&context.sha384, digest);
      return;
    case SHA_512:
      SHA512Final(&context.sha512, digest);
      return;
  }

  error(_("This cannot happen"));
}

/* Prints a formatted message to stderr and exist with non-zero status.
 */
void error(const char* format, ...)
{
  char* message;
  int result;
  va_list vl;

  va_start(vl, format);
  result = vasprintf(&message, format, vl);
  va_end(vl);

  if (result > 0)
  {
    fprintf(stderr, "%s: %s\n", PACKAGE_NAME, message);
    free(message);
  }

  exit(EXIT_FAILURE);
}

/* Prints a formatted message to stderr.
 */
void warning(const char* format, ...)
{
  char* message;
  int result;
  va_list vl;

  va_start(vl, format);
  result = vasprintf(&message, format, vl);
  va_end(vl);

  if (result > 0)
  {
    fprintf(stderr, "%s: %s\n", PACKAGE_NAME, message);
    free(message);
  }
}

/* Checks whether the cluster header format string uses file digests.
 */
int cluster_header_uses_digest(const char* format)
{
  const char* c;

  for (c = format;  *c != '\0';  c++)
  {
    if (*c == '%')
    {
      c++;
      if (*c == 'c' || *c == 'd')
        return 1;
      if (*c == '\0')
        break;
    }
  }

  return 0;
}

/* Prints a duplicate cluster header to stdout.  Various escape
 * sequences in the format string are replaced with the provided values.
 * Note that this function does not terminate the output with any
 * special character (i.e. newline or null).
 */
void print_cluster_header(const char* format,
                          unsigned int count,
                          unsigned int index,
                          off_t size,
                          const uint8_t* digest)
{
  int i, digest_size;
  const char* c;

  for (c = format;  *c != '\0';  c++)
  {
    if (*c == '%')
    {
      c++;
      switch (*c)
      {
        case 's':
          if (human_readable_flag)
          {
            char buf[5];
            human_readable(size, buf, 5);
            fputs(buf, stdout); fputs("B", stdout);
          }
          else
          {
            printf("%" PRIi64, size);
          }
          break;
        case 'i':
          printf("%u", index);
          break;
        case 'n':
          if (human_readable_flag)
          {
            char buf[256]; /* probably overkil, would anybody have cluster of more than 999 duplicates? */
            add_thousands_separator_z((size_t)count, buf, 256);
            fputs(buf, stdout);
          }
          else
          {
            printf("%u", count);
          }
          break;
        case 'c':
        case 'd':
          digest_size = get_digest_size();
          for (i = 0;  i < digest_size;  i++)
            printf("%02x", digest[i]);
          break;
        case '%':
          putchar('%');
          break;
        case '\0':
          putchar('\n');
          return;
        default:
          /* If the character following the '%' looks normal then we figure it
           * might be a good idea to silently prepend a '%' and pretend like we
           * didn't notice the broken format string.
           */
          if (isgraph(*c) || isspace(*c))
          {
            putchar('%');
            putchar(*c);
          }
      }
    }
    else
      putchar(*c);
  }
}

/* shorten given number to 3 digits + qualifier
 * eg. "12345" -> "12K"
 * out buffer must be able to hold 3 digits + 1 letter + null
 */
char* human_readable(size_t size, char *out, size_t out_size)
{
  int i = 0;

  if (out_size < 5)
  {
    error(_("human_readable: output buffer too short"));
  }

  const char* units[] = {"", "K", "M", "G", "T", "P", "E"};
  while (size >= 1000)
  {
    size /= 1024;
    i++; /* don't care bout more than thousand exabytes */
  }

  if (size == 0) { size = 1; } /* corner case between 1000 and 1024 */

  snprintf(out, 5, "%d%s", size, units[i]);
  return out;
}

/* add thousand separator to integer (as a string)
 * eg. 1234567 -> 1,234,567
 *TODO: assert on out_size overflow
 */
char* add_thousands_separator(char *a, char *out, size_t out_size)
{
  char *comma = ",";
  char *o = out;
  int n = 0;
  int k = strlen(a);
  int step = k % 3;
  if (step == 0) step = 3;

  while (k > 3)
  {
    strncpy(o, a, step);
    o += step;
    strncpy(o, comma, 1);
    o++;
    a += step;
    k -= step;
    step = 3; /* only in first round it can be smaller */
  }
  strncpy(o, a, k);
  o += k;
  *o = '\0';
  return out;
}

char* add_thousands_separator_z(size_t sz, char *out, size_t out_size)
{
    char tmp[out_size];
    snprintf(tmp, out_size, "%zu", sz);
    return add_thousands_separator(tmp, out, out_size);
}

