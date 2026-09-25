#ifndef DISK_H
#define DISK_H

#include <stdbool.h>
#include <stddef.h>
#include <zephyr/fs/fs.h>

#define LSDIR_MAX_NAME_LEN 32

typedef struct {
    char   name[LSDIR_MAX_NAME_LEN]; /**< Compact, null-terminated entry name. */
    bool   is_dir;                    /**< True if directory, false if file.    */
    size_t size;                      /**< File size in bytes (files only).     */
} lsdir_entry_t;

typedef struct {
    lsdir_entry_t *entries; /**< Heap-allocated array of entries.            */
    int            count;   /**< Number of valid entries in the array.       */
} lsdir_result_t;

int mount_sd_card(void);
int lsdir(const char *path, lsdir_result_t *result);
void lsdir_free(lsdir_result_t *result);
const char *get_file_extension(const char *filename);
void ini_key_to_value(const char *input, const char *target, char *out, size_t out_size);
void openfile_fdl(const char *input);

#endif