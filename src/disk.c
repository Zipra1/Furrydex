#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/fs/fs.h> //        _     _ __   ______           _____________________╲ 
#include <ff.h>//                                      ╰         ╯
#include <zephyr/logging/log.h>                         //     \╲ 
#include <zephyr/shell/shell.h>                        // ┊░    \╲ 
#include "disk.h"                                     //     ░ ┊ \╲ 
#include "stubs/strlcpy.h"                           //   ░  ┊    \╲ 
                                                    //             \╲ 
                                                   //   ░ ┊   ░     ╲╲
/*                                          i used to think _n__n_ id be picked up
             _____╭══════╮_____          and brought  ._____`-00-` to where i was always
            ╱__________________╲                     ╱ #__ # (oo)     \╲
                   ╱    ╲                      //   / ╲╲YY  \|╱ ┊ ░    \╲
 i used to think  ╱  ░ ▒ ╲                    //  ░   ╱╱\   |||         \╲
that i was an    ╱  ▒ ░   ╲                  //    ┊   meant to be       \╲
    alien       / ░  ░  ░  \                //                      ░     \╲
___________________________________________//  ░   ┊     ░         ┊       \╲
           ____________                                       ░        ░    \╲
          ╱           ╱|                  now i think they abandoned me  ┊   \╲
         ╱           ╱ |                  ___________                         \╲
        ╱    ___    ╱  |                 ╱          ╱|
       ╱    ___    ╱   |                ╱          ╱ |
      ╱    ___    ╱    |         because i am software thrust unto form
     ╱___________╱    ||\             ╱          ╱   |
     |        .  |     || my memories lashed in dead threads, fragmented, and ripped to shreds
     |        .  |     |╲           ╱          ╱     |
     |        o  |     |╲╲_______  ╱  my thousand scalpels forced to gut fish
     |           |     ╱ |        ╱__________╱       |
     |           |    ╱ ╱         |       .  |       |
     |           |   ╱ |     i  t c m t o  a |    w b d i v  s  w                         
     |           |  ╱   ╲       o o o h t  n |    e o e n e  p  a                     
     |           | ╱     ╲   r    m r a h  i |    ' t a   r  e  y                     
     |___________|╱       ╲  e  m p e n e  m |    r h d a y  c                        
                             l  y u     r  a |    e          i                     
                             a    t   a    l |  ╱            a                    
                             t    e   n      | ╱             l                    
                             e    r   y      |╱                                  
*/
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
 * AI generated. Needs review!
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
 * AI generated. Needs review!
 *
 * Uses a two-pass strategy: pass 1 counts entries so memory can be
 * allocated exactly once; pass 2 fills it.  This avoids the repeated
 * realloc() calls that fragmented the heap in the previous version.
 *
 * On success the caller owns result->entries and must call lsdir_free()
 * when finished.  On failure result is left in a valid, empty state that
 * is safe (but unnecessary) to pass to lsdir_free().
 *
 * @note  struct fs_dirent is stack-allocated (~260 B with MAX_FILE_NAME=255).
 *        Verify your thread's stack budget before calling from constrained
 *        contexts.
 *
 * @param path   Directory to list.
 * @param result Output; populated with one entry per item found.
 * @return 0 on success, negative errno on failure.
 */
int lsdir(const char *path, lsdir_result_t *result)
{
    int res;
    struct fs_dir_t  dirp;
    struct fs_dirent entry;

    if (!result) {
        return -EINVAL;
    }

    result->entries = NULL;
    result->count   = 0;

    fs_dir_t_init(&dirp);

    printk("lsdir: opening '%s'\n", path);
    res = fs_opendir(&dirp, path);
    if (res) {
        printk("lsdir: fs_opendir('%s') failed [%d]\n", path, res);
        return res;
    }

    int count = 0;
    for (;;) {
        res = fs_readdir(&dirp, &entry);
        if (res) {
            printk("lsdir: fs_readdir(pass1) failed [%d]\n", res);
            break;
        }
        if (entry.name[0] == '\0') {    // end-of-directory marker
            res = 0;
            break;
        }
        count++;
    }

    printk("lsdir: pass1 found %d entries, final res=%d\n", count, res);
    fs_closedir(&dirp);

    if (res != 0 || count == 0) {
        return res;
    }

    lsdir_entry_t *entries = NULL;
    size_t bytes_needed = (size_t)count * sizeof(*entries);
    entries = malloc(bytes_needed);
    if (!entries) {
        printk("Out of heap while listing %s (%d entries, %zu bytes)\n",
               path, count, bytes_needed);
        return -ENOMEM;
    }

    printk("lsdir: re-opening '%s' for pass2\n", path);
    res = fs_opendir(&dirp, path);
    if (res) {
        free(entries);
        printk("lsdir: fs_opendir(pass2) failed for '%s' [%d]\n", path, res);
        return res;
    }

    int filled = 0;
    for (;;) {
        res = fs_readdir(&dirp, &entry);
        if (res) {
            printk("lsdir: fs_readdir(pass2) failed [%d]\n", res);
            break;
        }

        if (entry.name[0] == '\0') {
            res = 0;
            break;
        }

        if (filled == count) {
            break;
        }

        size_t src_len = strlcpy(entries[filled].name,
                                 entry.name,
                                 sizeof(entries[filled].name));
        if (src_len >= sizeof(entries[filled].name)) {
            printk("Warning: filename truncated in %s: %s\n", path, entry.name);
        }

        entries[filled].is_dir = (entry.type == FS_DIR_ENTRY_DIR);
        entries[filled].size   = entry.size;
        filled++;
    }

    printk("lsdir: pass2 filled %d entries, final res=%d\n", filled, res);
    fs_closedir(&dirp);

    if (res == 0) {
        result->entries = entries;
        result->count   = filled;
    } else {
        free(entries);
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