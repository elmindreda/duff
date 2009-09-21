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

/* Macros for pretend gettext.
 */
#define gettext(String) (String)
#define textdomain(Domain)
#define bindtextdomain(Package, Directory)

#define SAMPLE_COUNT 10

/* Status modes for entries.
 */
typedef enum
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
} Status;

/* Symlink dereferencing modes.
 */
typedef enum
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
} SymlinkMode;

/* Typedefs for structs below.
 */
typedef struct Entry_t Entry;
typedef struct Directory_t Directory;

/* Represents a traversed directory.
 */
struct Directory_t
{
  Directory* next;
  dev_t device;
  ino_t inode;
};

/* Represents a collected file and potential duplicate.
 */
struct Entry_t
{
  Entry* prev;
  Entry* next;
  char* path;
  off_t size;
  dev_t device;
  ino_t inode;
  Status status;
  uint8_t* digest;
  uint8_t* samples;
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

/* These are defined and documented in duffutil.c */
int read_path(FILE* stream, char* path, size_t size);
void kill_trailing_slashes(char* path);
size_t get_digest_size(void);
void error(const char* format, ...);
void warning(const char* format, ...);
void print_cluster_header(const char* format,
                          unsigned int count,
			  unsigned int index,
			  off_t size,
			  const uint8_t* digest);

/* These are defined and documented in duffdriver.c */
void process_path(const char* path, int depth);
void report_clusters(void);

