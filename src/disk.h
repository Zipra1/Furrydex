#ifndef DISK_H
#define DISK_H

#include <stdbool.h>
#include <stddef.h>
#include <zephyr/fs/fs.h>

typedef struct {
    char   name[MAX_FILE_NAME + 1]; /**< Null-terminated entry name.          */
    bool   is_dir;                  /**< True if directory, false if file.    */
    size_t size;                    /**< File size in bytes (files only).     */
} lsdir_entry_t;

typedef struct {
    lsdir_entry_t *entries; /**< Heap-allocated array of entries.            */
    int            count;   /**< Number of valid entries in the array.       */
} lsdir_result_t;

int mount_sd_card(void);
int lsdir(const char *path, lsdir_result_t *result);
void lsdir_free(lsdir_result_t *result);

#endif