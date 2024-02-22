/*
 * duff - Duplicate file finder
 * Copyright (c) 2005 Camilla Löwy <elmindreda@elmindreda.org>
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

/* The number of bytes to use as read buffer when reading files.
 * NOTE: This must be at least 1 and should likely be multiples of 4096.
 */
#define BUFFER_SIZE 8192

/* The number of bytes to sample from the beginning of potential duplicates.
 * NOTE: This must be at least 1 but likely not larger than 4096.
 */
#define SAMPLE_SIZE 4096

/* The number of bytes to increase the path read buffer with when it fills up.
 * NOTE: This must be at least 1.
 */
#define PATH_SIZE_STEP 256

/* The number of bits of file size to use as bucket index.
 * NOTE: This must be at least 1.
 */
#define HASH_BITS 10

/* Status modes for files.
 */
enum Status
{
    /* The file has been stat:d but its data has not been touched.
     */
    UNTOUCHED,
    /* The beginning of the file has been hashed.
     */
    SAMPLED,
    /* The entire file has been hashed.
     */
    HASHED,
    /* An error ocurred when reading from the file.
     */
    INVALID,
    /* The file has been found to be a duplicate.
     */
    DUPLICATE
};

typedef enum Status Status;

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
    ARG_SYMLINKS
};

typedef enum SymlinkMode SymlinkMode;

/* Represents a collected file and potential duplicate.
 */
struct File
{
    char* path;
    off_t size;
    dev_t device;
    ino_t inode;
    Status status;
    uint8_t* digest;
    uint8_t* sample;
};

typedef struct File File;

/* Represents a list of files.
 */
struct FileList
{
    File* files;
    size_t allocated;
    size_t available;
};

typedef struct FileList FileList;

/* These are defined and documented in dufffile.c */
void init_file(File* file, const char* path, const struct stat* sb);
void free_file(File* file);
int compare_files(File* first, File* second);
void generate_file_digest(File* file);

/* These are defined and documented in duffutil.c */
void init_file_list(FileList* list);
File* alloc_file(FileList* list);
void empty_file_list(FileList* list);
void free_file_list(FileList* list);
char* read_path(FILE* stream);
void kill_trailing_slashes(char* path);
size_t get_field_terminator(void);
int set_digest_function(const char* name);
size_t get_digest_size(void);
void init_digest(void);
void update_digest(const void* data, size_t size);
void finish_digest(uint8_t* digest);
void error(const char* format, ...) __attribute__((format(printf, 1, 2))) __attribute__((noreturn));
void warning(const char* format, ...) __attribute__((format(printf, 1, 2)));
int cluster_header_uses_digest(const char* format);
void print_cluster_header(const char* format,
                          unsigned int count,
                          unsigned int index,
                          off_t size,
                          const uint8_t* digest);
char* add_thousands_separator(char *a, char *out, size_t out_size);
char* add_thousands_separator_z(size_t sz, char *out, size_t out_size);
char* human_readable(size_t size, char *out, size_t out_size);

/* These are defined and documented in duffdriver.c */
void process_args(int argc, char** argv);
