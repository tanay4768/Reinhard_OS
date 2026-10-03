#ifndef AKIRA_FS_H
#define AKIRA_FS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FS_NAME_MAX      32
#define FS_MAX_FILE_SIZE (64u * 1024u)

typedef enum { FS_FILE, FS_DIR } fs_type_t;

typedef enum {
    FS_OK = 0,
    FS_ERR_NOT_FOUND,
    FS_ERR_EXISTS,
    FS_ERR_NOT_DIR,
    FS_ERR_IS_DIR,
    FS_ERR_NOT_EMPTY,
    FS_ERR_BAD_NAME,
    FS_ERR_NO_SPACE,
    FS_ERR_BUSY
} fs_err_t;

typedef struct fs_node {
    char            name[FS_NAME_MAX];
    fs_type_t       type;
    struct fs_node *parent;
    struct fs_node *first_child;
    struct fs_node *next_sibling;
    uint8_t        *data;
    size_t          size;
} fs_node_t;

void        fs_init(void);
fs_node_t  *fs_root(void);
fs_node_t  *fs_cwd(void);
void        fs_set_cwd(fs_node_t *dir);

fs_node_t  *fs_lookup(const char *path);
fs_node_t  *fs_create(const char *path, fs_type_t type, fs_err_t *err);
fs_err_t    fs_remove(const char *path);

fs_err_t    fs_write(fs_node_t *file, const void *data, size_t len);   /* replace  */
fs_err_t    fs_append(fs_node_t *file, const void *data, size_t len);
size_t      fs_read(const fs_node_t *file, size_t offset, void *buf, size_t len);

void        fs_path(const fs_node_t *node, char *out, size_t cap);
const char *fs_strerror(fs_err_t err);

#endif
