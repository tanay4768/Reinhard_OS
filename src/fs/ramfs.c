/* In-memory hierarchical filesystem (RAM disk).
 *
 * Every file/directory is an fs_node_t. Children are kept in a singly linked
 * list in creation order. File contents are heap blocks that are replaced on
 * write; this keeps the code small and is plenty for a teaching kernel.
 */

#include "fs.h"

#include "heap.h"
#include "kstring.h"

static fs_node_t *root;
static fs_node_t *cwd;

/* ---------- node helpers ---------- */

static fs_node_t *node_new(const char *name, fs_type_t type)
{
    fs_node_t *n = kzalloc(sizeof(*n));
    if (!n)
        return NULL;
    strlcpy(n->name, name, sizeof(n->name));
    n->type = type;
    return n;
}

static void attach(fs_node_t *parent, fs_node_t *child)
{
    child->parent = parent;
    child->next_sibling = NULL;

    if (!parent->first_child) {
        parent->first_child = child;
        return;
    }
    fs_node_t *last = parent->first_child;
    while (last->next_sibling)
        last = last->next_sibling;
    last->next_sibling = child;
}

static void detach(fs_node_t *node)
{
    fs_node_t **link = &node->parent->first_child;
    while (*link && *link != node)
        link = &(*link)->next_sibling;
    if (*link)
        *link = node->next_sibling;
}

static fs_node_t *find_child(const fs_node_t *dir, const char *name, size_t len)
{
    for (fs_node_t *c = dir->first_child; c; c = c->next_sibling)
        if (strlen(c->name) == len && memcmp(c->name, name, len) == 0)
            return c;
    return NULL;
}

/* ---------- path resolution ---------- */

/* Walks `path` from the root (absolute) or cwd (relative).
 *  - stop_at_parent == false: returns the node the path names.
 *  - stop_at_parent == true : returns the directory that would contain the
 *    last component and copies that component into `leaf`.
 * On failure returns NULL and sets *err. */
static fs_node_t *resolve(const char *path, bool stop_at_parent, char *leaf, fs_err_t *err)
{
    fs_node_t  *node = (path[0] == '/') ? root : cwd;
    const char *p = path;

    while (*p == '/')
        p++;

    while (*p) {
        const char *start = p;
        size_t len = 0;
        while (p[len] && p[len] != '/')
            len++;
        p += len;
        while (*p == '/')
            p++;
        bool last = (*p == '\0');

        if (len >= FS_NAME_MAX) {
            *err = FS_ERR_BAD_NAME;
            return NULL;
        }

        bool is_dot    = (len == 1 && start[0] == '.');
        bool is_dotdot = (len == 2 && start[0] == '.' && start[1] == '.');

        if (last && stop_at_parent) {
            if (is_dot || is_dotdot) {
                *err = FS_ERR_BAD_NAME;
                return NULL;
            }
            if (node->type != FS_DIR) {
                *err = FS_ERR_NOT_DIR;
                return NULL;
            }
            memcpy(leaf, start, len);
            leaf[len] = '\0';
            return node;
        }

        if (node->type != FS_DIR) {
            *err = FS_ERR_NOT_DIR;
            return NULL;
        }
        if (is_dot)
            continue;
        if (is_dotdot) {
            if (node->parent)
                node = node->parent;
            continue;
        }

        fs_node_t *child = find_child(node, start, len);
        if (!child) {
            *err = FS_ERR_NOT_FOUND;
            return NULL;
        }
        node = child;
    }

    if (stop_at_parent) {                 /* empty path or "/" has no leaf */
        *err = FS_ERR_BAD_NAME;
        return NULL;
    }
    return node;
}

/* ---------- public API ---------- */

void fs_init(void)
{
    root = node_new("", FS_DIR);
    cwd = root;

    fs_node_t *readme = fs_create("/README.txt", FS_FILE, NULL);
    static const char text[] =
        "Welcome to Akira OS!\n"
        "\n"
        "This is a RAM-backed filesystem. Try:\n"
        "  ls, cd, mkdir, touch, cat, rm\n"
        "  notepad notes.txt     (Ctrl+S saves, Ctrl+Q quits)\n"
        "  echo hello > hi.txt\n"
        "  meminfo, vmtest, pageinfo\n";
    if (readme)
        fs_write(readme, text, sizeof(text) - 1);

    fs_create("/home", FS_DIR, NULL);
    fs_create("/tmp", FS_DIR, NULL);
}

fs_node_t *fs_root(void) { return root; }
fs_node_t *fs_cwd(void)  { return cwd; }
void fs_set_cwd(fs_node_t *dir)
{
    if (dir && dir->type == FS_DIR)
        cwd = dir;
}

fs_node_t *fs_lookup(const char *path)
{
    fs_err_t err = FS_OK;
    return resolve(path, false, NULL, &err);
}

fs_node_t *fs_create(const char *path, fs_type_t type, fs_err_t *err_out)
{
    fs_err_t err = FS_OK;
    char leaf[FS_NAME_MAX];
    fs_node_t *node = NULL;

    fs_node_t *parent = resolve(path, true, leaf, &err);
    if (parent) {
        if (find_child(parent, leaf, strlen(leaf))) {
            err = FS_ERR_EXISTS;
        } else if (!(node = node_new(leaf, type))) {
            err = FS_ERR_NO_SPACE;
        } else {
            attach(parent, node);
        }
    }

    if (err_out)
        *err_out = err;
    return node;
}

fs_err_t fs_remove(const char *path)
{
    fs_err_t err = FS_OK;
    fs_node_t *node = resolve(path, false, NULL, &err);
    if (!node)
        return err;

    if (node == root)
        return FS_ERR_BUSY;
    for (fs_node_t *n = cwd; n; n = n->parent)       /* never delete our own cwd */
        if (n == node)
            return FS_ERR_BUSY;
    if (node->type == FS_DIR && node->first_child)
        return FS_ERR_NOT_EMPTY;

    detach(node);
    kfree(node->data);
    kfree(node);
    return FS_OK;
}

fs_err_t fs_write(fs_node_t *file, const void *data, size_t len)
{
    if (file->type != FS_FILE)
        return FS_ERR_IS_DIR;
    if (len > FS_MAX_FILE_SIZE)
        return FS_ERR_NO_SPACE;

    uint8_t *fresh = NULL;
    if (len) {
        fresh = kmalloc(len);
        if (!fresh)
            return FS_ERR_NO_SPACE;
        memcpy(fresh, data, len);
    }
    kfree(file->data);
    file->data = fresh;
    file->size = len;
    return FS_OK;
}

fs_err_t fs_append(fs_node_t *file, const void *data, size_t len)
{
    if (file->type != FS_FILE)
        return FS_ERR_IS_DIR;
    if (len == 0)
        return FS_OK;
    if (file->size + len > FS_MAX_FILE_SIZE)
        return FS_ERR_NO_SPACE;

    uint8_t *fresh = kmalloc(file->size + len);
    if (!fresh)
        return FS_ERR_NO_SPACE;
    if (file->size)
        memcpy(fresh, file->data, file->size);
    memcpy(fresh + file->size, data, len);

    kfree(file->data);
    file->data = fresh;
    file->size += len;
    return FS_OK;
}

size_t fs_read(const fs_node_t *file, size_t offset, void *buf, size_t len)
{
    if (file->type != FS_FILE || offset >= file->size)
        return 0;
    if (len > file->size - offset)
        len = file->size - offset;
    memcpy(buf, file->data + offset, len);
    return len;
}

void fs_path(const fs_node_t *node, char *out, size_t cap)
{
    if (cap == 0)
        return;
    if (node == root || !node) {
        strlcpy(out, "/", cap);
        return;
    }

    const fs_node_t *chain[32];
    int depth = 0;
    for (const fs_node_t *n = node; n && n != root && depth < 32; n = n->parent)
        chain[depth++] = n;

    size_t pos = 0;
    for (int i = depth - 1; i >= 0; i--) {
        if (pos + 1 < cap)
            out[pos++] = '/';
        for (const char *s = chain[i]->name; *s && pos + 1 < cap; s++)
            out[pos++] = *s;
    }
    out[pos] = '\0';
}

const char *fs_strerror(fs_err_t err)
{
    switch (err) {
    case FS_OK:            return "success";
    case FS_ERR_NOT_FOUND: return "No such file or directory";
    case FS_ERR_EXISTS:    return "File exists";
    case FS_ERR_NOT_DIR:   return "Not a directory";
    case FS_ERR_IS_DIR:    return "Is a directory";
    case FS_ERR_NOT_EMPTY: return "Directory not empty";
    case FS_ERR_BAD_NAME:  return "Invalid name";
    case FS_ERR_NO_SPACE:  return "Out of space";
    case FS_ERR_BUSY:      return "Resource busy";
    }
    return "Unknown error";
}
