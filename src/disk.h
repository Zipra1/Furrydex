#ifndef DISK_H
#define DISK_H

#include <stdbool.h>

static int mount_sd_card(void);
static int lsdir(const char *path);
extern bool msc_enabled;

#endif