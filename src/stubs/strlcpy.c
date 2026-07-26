#include <stddef.h>
#include <string.h>

static inline size_t strlcpy(char *dst, const char *src, size_t siz)
{
    size_t src_len = strlen(src);

    if (siz > 0) {
        size_t copy_len = (src_len >= siz) ? siz - 1 : src_len;
        memcpy(dst, src, copy_len);
        dst[copy_len] = '\0';
    }

    return src_len;
}