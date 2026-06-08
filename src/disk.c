#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/fs/fs.h>
#include <ff.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include "disk.h"

LOG_MODULE_REGISTER(fdex_disk, CONFIG_LOG_DEFAULT_LEVEL);

static FATFS fat_fs;
/* mounting info */
struct fs_mount_t mp = {
    .type = FS_FATFS,
    .fs_data = &fat_fs,
};

const char *disk_mount_pt = "/SD:";

/**
 * @brief Release memory allocated by lsdir().
 */
void lsdir_free(lsdir_result_t *result)
{
    if (!result) {
        return;
    }
    free(result->entries);
    result->entries = NULL;
    result->count   = 0;
}

/**
 * @brief List a directory and return its contents.
 *
 * On success the caller owns result->entries and must call lsdir_free()
 * when finished.  On failure result is left in a valid, empty state that
 * is safe (but unnecessary) to pass to lsdir_free().
 * lsdir is written by generative AI, requires review
 * this function is likely unsafe and causing crashes when the heap fragments
 *
 * @param path   Directory to list.
 * @param result Output; populated with one entry per item found.
 * @return 0 on success, negative errno on failure.
 */
int lsdir(const char *path, lsdir_result_t *result)
{
    int res;
    struct fs_dir_t  dirp;
    static struct fs_dirent entry;      /* static: avoids large stack frame */

    /* Initialise the output to a safe empty state. */
    result->entries = NULL;
    result->count   = 0;

    fs_dir_t_init(&dirp);

    res = fs_opendir(&dirp, path);
    if (res) {
        printk("Error opening dir %s [%d]\n", path, res);
        return res;
    }

    /* Start with room for 16 entries; double as needed. */
    int capacity = 16;
    lsdir_entry_t *entries = malloc(capacity * sizeof(lsdir_entry_t));
    if (!entries) {
        fs_closedir(&dirp);
        return -ENOMEM;
    }

    int count = 0;

    for (;;) {
        res = fs_readdir(&dirp, &entry);

        /* entry.name[0] == 0 signals end-of-directory (not an error). */
        if (res || entry.name[0] == 0) {
            if (entry.name[0] == 0) {
                res = 0;
            }
            break;
        }

        /* Grow the array when full. */
        if (count == capacity) {
            capacity *= 2;
            lsdir_entry_t *tmp = realloc(entries, capacity * sizeof(lsdir_entry_t));
            if (!tmp) {
                free(entries);
                fs_closedir(&dirp);
                return -ENOMEM;
            }
            entries = tmp;
        }

        strncpy(entries[count].name, entry.name, MAX_FILE_NAME);
        entries[count].name[MAX_FILE_NAME] = '\0';
        entries[count].is_dir = (entry.type == FS_DIR_ENTRY_DIR);
        entries[count].size   = entry.size;
        count++;
    }

    fs_closedir(&dirp);

    if (res == 0) {
        result->entries = entries;
        result->count   = count;
    } else {
        free(entries);      /* discard partial list on read error */
    }

    return res;
}

int mount_sd_card(void)
{
    /* raw disk i/o */
    static const char *disk_pdrv = "SD";
    uint64_t memory_size_mb;
    uint32_t block_count;
    uint32_t block_size;

    // if (disk_access_init(disk_pdrv) != 0)
    // {
    //     LOG_ERR("Storage init ERROR!");
    //     return -1;
    // }

    if (disk_access_ioctl(disk_pdrv,
                          DISK_IOCTL_GET_SECTOR_COUNT, &block_count))
    {
        LOG_ERR("Unable to get sector count");
        return -1;
    }
    LOG_INF("Block count %u", block_count);

    if (disk_access_ioctl(disk_pdrv,
                          DISK_IOCTL_GET_SECTOR_SIZE, &block_size))
    {
        LOG_ERR("Unable to get sector size");
        return -1;
    }
    printk("Sector size %u\n", block_size);

    memory_size_mb = (uint64_t)block_count * block_size;
    printk("Memory Size(MB) %u\n", (uint32_t)(memory_size_mb >> 20));

    mp.mnt_point = disk_mount_pt;

    int res = fs_mount(&mp);

    if (res == FR_OK)
    {
        printk("Disk mounted.\n");
    }
    else
    {
        printk("Failed to mount disk - trying one more time\n");
        res = fs_mount(&mp);
        if (res != FR_OK)
        {
            printk("Error mounting disk.\n");
            return -1;
        }
    }

    return 0;
}