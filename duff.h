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

enum
{
  UNTOUCHED,
  INVALID,
  DUPLICATE,
  REPORTED,
};

enum
{
  ALL_SYMLINKS,
  NO_SYMLINKS,
  ARG_SYMLINKS,
};

struct Directory
{
  struct Directory* next;
  dev_t device;
  ino_t inode;
};

struct Entry
{
  struct Entry* prev;
  struct Entry* next;
  char* path;
  off_t size;
  dev_t device;
  ino_t inode;
  int status;
  uint8_t* checksum;
  uint8_t* samples;
};

/* These live in duffentry.c */
struct Entry* make_entry(const char* path, const struct stat* sb);
struct Entry* copy_entry(struct Entry* entry);
void link_entry(struct Entry** head, struct Entry* entry);
void unlink_entry(struct Entry** head, struct Entry* entry);
void free_entry(struct Entry* entry);
void free_entry_list(struct Entry** entries);
int compare_entries(struct Entry* first, struct Entry* second);

/* These live in duffutil.c */
void error(const char* format, ...);
void warning(const char* format, ...);
const char* get_mode_name(int mode);
void print_cluster_header(const char* format,
                          unsigned int count,
			  unsigned int index,
			  off_t size,
			  const uint8_t* checksum);

/* These live in duffdriver.c */
void process_path(const char* path, int depth);
void report_clusters(void);

