#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/fs/fs.h>
#include <ff.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

LOG_MODULE_REGISTER(fdex_disk, CONFIG_LOG_DEFAULT_LEVEL);

static FATFS fat_fs;
/* mounting info */
struct fs_mount_t mp = {
    .type = FS_FATFS,
    .fs_data = &fat_fs,
};

const char *disk_mount_pt = "/SD:";

int lsdir(const char *path)
{
    int res;
    struct fs_dir_t dirp;
    static struct fs_dirent entry;

    fs_dir_t_init(&dirp);

    /* Verify fs_opendir() */
    res = fs_opendir(&dirp, path);
    if (res)
    {
        printk("Error opening dir %s [%d]\n", path, res);
        return res;
    }

    printk("\nListing dir %s ...\n", path);
    for (;;)
    {
        /* Verify fs_readdir() */
        res = fs_readdir(&dirp, &entry);

        /* entry.name[0] == 0 means end-of-dir */
        if (res || entry.name[0] == 0)
        {
            break;
        }

        if (entry.type == FS_DIR_ENTRY_DIR)
        {
            printk("[DIR ] %s\n", entry.name);
        }
        else
        {
            printk("[FILE] %s (size = %zu)\n",
                   entry.name, entry.size);
        }
    }

    /* Verify fs_closedir() */
    fs_closedir(&dirp);

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
        lsdir(disk_mount_pt);
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

bool msc_enabled = false;

// static int cmd_msc_toggle(const struct shell *sh, size_t argc, char **argv)
// {
//     int ret;

//     if (msc_enabled)
//     {
//         ret = fs_mount(&mp);
//         if (ret)
//         {
//             shell_error(sh, "Failed to mount (%d)", ret);
//             return ret;
//         }
//         // bool force = true;
//         // ret = disk_access_ioctl(DISK_NAME, DISK_IOCTL_CTRL_DEINIT, &force);
//         // if (ret)
//         // {
//         //     shell_error(sh, "Failed to deinit disk (%d)", ret);
//         //     return ret;
//         // }
//         msc_enabled = false;
//         shell_print(sh, "Disk mounted to Zephyr, hidden from host");
//     }
//     else
//     {
//         // ret = disk_access_ioctl(DISK_NAME, DISK_IOCTL_CTRL_INIT, NULL);
//         // if (ret)
//         // {
//         //     shell_error(sh, "Failed to init disk (%d)", ret);
//         //     return ret;
//         // }
//         ret = fs_unmount(&mp);
//         if (ret)
//         {
//             shell_error(sh, "Failed to unmount (%d)", ret);
//             return ret;
//         }
//         // ret = disk_access_ioctl(DISK_NAME, DISK_IOCTL_CTRL_INIT, NULL);
//         // if (ret)
//         // {
//         //     shell_error(sh, "Failed to init disk for host (%d)", ret);
//         //     return ret;
//         // }
//         msc_enabled = true;
//         shell_print(sh, "Disk unmounted from Zephyr, visible to host");
//     }
//     return 0;
// }

// static int cmd_msc_status(const struct shell *sh, size_t argc, char **argv)
// {
//     shell_print(sh, "MSC is %s", msc_enabled ? "enabled (host has disk)" : "disabled (zephyr has disk)");
//     return 0;
// }

// SHELL_STATIC_SUBCMD_SET_CREATE(msc_cmds,
//                                SHELL_CMD(toggle, NULL, "Toggle USB mass storage", cmd_msc_toggle),
//                                SHELL_CMD(status, NULL, "Show MSC status", cmd_msc_status),
//                                SHELL_SUBCMD_SET_END);

// SHELL_CMD_REGISTER(msc, &msc_cmds, "USB Mass Storage commands", NULL);

// Thread settings