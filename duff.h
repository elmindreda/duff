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

enum
{
  FRESH,
  INVALID,
  CHECKED,
  REPORTED,
};

struct Entry
{
  struct Entry* next;
  char* path;
  size_t size;
  long checksum;
  int status;
};

struct Cluster
{
  struct Entry* head;
  long count;
};

struct Entry* make_entry(const char* path, size_t size);
struct Entry* copy_entry(struct Entry* entry);
void free_entry(struct Entry* entry);
void free_entry_list(struct Entry** entries);
int get_entry_checksum(struct Entry* entry);
int compare_entries(struct Entry* first, struct Entry* second);
int compare_entry_contents(struct Entry* first, struct Entry* second);

void error(const char* format, ...);
void warning(const char* format, ...);
const char* get_mode_name(int mode);
void version(void);
void usage(void);
void bugs(void);
void print_cluster_header(const char* format,
                          unsigned int count,
			  unsigned int number,
			  size_t size,
			  unsigned int checksum);

