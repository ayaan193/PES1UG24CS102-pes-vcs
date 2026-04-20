// tree.c — Tree object serialization and construction

#include "tree.h"
#include "index.h"
#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define MODE_FILE 0100644
#define MODE_EXEC 0100755
#define MODE_DIR  0040000

// ─── PROVIDED ───────────────────────────────────────────────────────────────

uint32_t get_file_mode(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return 0;

    if (S_ISDIR(st.st_mode))  return MODE_DIR;
    if (st.st_mode & S_IXUSR) return MODE_EXEC;
    return MODE_FILE;
}

// tree_parse and tree_serialize remain unchanged from skeleton

// ─── HELPER: Recursive builder ─────────────────────────────────────────────

static int build_tree_level(Index *idx, const char *prefix, ObjectID *out_id) {

    Tree tree;
    tree.count = 0;

    size_t prefix_len = strlen(prefix);

    for (size_t i = 0; i < idx->count; i++) {

        IndexEntry *e = &idx->entries[i];

        if (strncmp(e->path, prefix, prefix_len) != 0) continue;

        const char *rel_path = e->path + prefix_len;

        if (rel_path[0] == '\0') continue;

        const char *slash = strchr(rel_path, '/');

        if (!slash) {
            // File entry
            if (tree.count < MAX_TREE_ENTRIES) {
                TreeEntry *entry = &tree.entries[tree.count++];

                entry->mode = e->mode;
                strncpy(entry->name, rel_path, sizeof(entry->name));
                entry->hash = e->hash;
            }

        } else {
            // Directory entry

            size_t dir_len = slash - rel_path;

            char dirname[256];
            strncpy(dirname, rel_path, dir_len);
            dirname[dir_len] = '\0';

            // Avoid duplicates
            int exists = 0;
            for (int j = 0; j < tree.count; j++) {
                if (strcmp(tree.entries[j].name, dirname) == 0) {
                    exists = 1;
                    break;
                }
            }

            if (!exists && tree.count < MAX_TREE_ENTRIES) {

                char new_prefix[512];
                snprintf(new_prefix, sizeof(new_prefix), "%s%s/", prefix, dirname);

                ObjectID sub_id;
                if (build_tree_level(idx, new_prefix, &sub_id) < 0) {
                    return -1;
                }

                TreeEntry *entry = &tree.entries[tree.count++];
                entry->mode = MODE_DIR;
                strncpy(entry->name, dirname, sizeof(entry->name));
                entry->hash = sub_id;
            }
        }
    }

    // ─── FINAL STEP (Commit 5) ───

    void *data = NULL;
    size_t len = 0;

    if (tree_serialize(&tree, &data, &len) < 0) {
        return -1;
    }

    if (object_write(OBJ_TREE, data, len, out_id) < 0) {
        free(data);
        return -1;
    }

    free(data);
    return 0;
}

// ─── MAIN ENTRY ────────────────────────────────────────────────────────────

int tree_from_index(ObjectID *id_out) {

    Index idx;
    if (index_load(&idx) < 0) {
        return -1;
    }

    return build_tree_level(&idx, "", id_out);
}
