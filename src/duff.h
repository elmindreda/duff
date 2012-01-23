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

#include "gettext.h"

/* Shorthand macro for gettext.
  */
#define _(String) gettext(String)

/* Only use __attribute__ on GCC and compatible compilers */
#ifndef __GNUC__
#define __attribute__(x)
#endif

#define SAMPLE_SIZE 512

/* Typedefs for structs and enums.
 */
typedef enum Status Status;
typedef enum SymlinkMode SymlinkMode;
typedef enum Function Function;
typedef struct Entry Entry;
typedef struct Directory Directory;

/* Status modes for entries.
 */
enum Status
{
  /* The file has been stat:d but its data has not been touched.
   */
  UNTOUCHED,
  /* An error ocurred when reading from the file.
   */
  INVALID,
  /* The file is a member of a cluster of duplicates.
   */
  DUPLICATE,
  /* The file has been reported as a duplicate.
   */
  REPORTED,
};

/* Symlink dereferencing modes.
 */
enum SymlinkMode
{
  /* Do not dereference any directory symlinks.
   */
  NO_SYMLINKS,
  /* Dereference all directory symlinks encountered.
   */
  ALL_SYMLINKS,
  /* Dereference only those directory symlinks listed on the command line.
   */
  ARG_SYMLINKS,
};

/* Represents a traversed directory.
 */
struct Directory
{
  Directory* next;
  dev_t device;
  ino_t inode;
};

/* Represents a collected file and potential duplicate.
 */
struct Entry
{
  Entry* prev;
  Entry* next;
  char* path;
  off_t size;
  dev_t device;
  ino_t inode;
  Status status;
  uint8_t* digest;
  uint8_t* sample;
};

/* Message digest functions.
 */
enum Function
{
  SHA_1,
  SHA_256,
  SHA_384,
  SHA_512,
};

/* These are defined and documented in duffentry.c */
Entry* make_entry(const char* path, const struct stat* sb);
void link_entry(Entry** head, Entry* entry);
void unlink_entry(Entry** head, Entry* entry);
void free_entry(Entry* entry);
void free_entry_list(Entry** entries);
int compare_entries(Entry* first, Entry* second);
void generate_entry_digest(Entry* entry);

/* These are defined and documented in duffutil.c */
int read_path(FILE* stream, char* path, size_t size);
void kill_trailing_slashes(char* path);
void set_digest_function(Function function);
size_t get_digest_size(void);
void digest_init(void);
void digest_update(const void* data, size_t size);
void digest_finish(uint8_t* digest);
void error(const char* format, ...) __attribute__((format(printf, 1, 2))) __attribute__((noreturn));
void warning(const char* format, ...) __attribute__((format(printf, 1, 2)));
int cluster_header_uses_digest(const char* format);
void print_cluster_header(const char* format,
                          unsigned int count,
			  unsigned int index,
			  off_t size,
			  const uint8_t* digest);

/* These are defined and documented in duffdriver.c */
void process_path(const char* path, int depth);
void report_clusters(void);

