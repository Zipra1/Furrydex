#ifndef DISK_H
#define DISK_H

#include <stdbool.h>
#include <stddef.h>
#include <zephyr/fs/fs.h>

struct shell;

#if defined(CONFIG_DISK_MODE_NVS)
#define FURRYDEX_DISK_NAME "NVS"
#define FURRYDEX_MOUNT_POINT "/NVS:"
#else
#define FURRYDEX_DISK_NAME "SD"
#define FURRYDEX_MOUNT_POINT "/SD:"
#endif

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

int mount_drive(void);
int lsdir(const char *path, lsdir_result_t *result);
void lsdir_free(lsdir_result_t *result);
const char *get_file_extension(const char *filename);
int ini_key_to_value(const char *path, const char *key, char *value, size_t value_size);
void openfile_fdl(const struct shell *shell, const char *input);

#endif