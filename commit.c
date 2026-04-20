// commit.c — Commit creation and history traversal

#include "commit.h"
#include "index.h"
#include "tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);
int object_read(const ObjectID *id, ObjectType *type_out, void **data_out, size_t *len_out);

// ─── PROVIDED (UNCHANGED) ─────────────────────────────────────────────
// (keep all same)

// ─── IMPLEMENTATION ───────────────────────────────────────────────────

int commit_create(const char *message, ObjectID *commit_id_out) {

    Index index;
    if (index_load(&index) != 0)
        return -1;

    ObjectID tree_id;
    if (tree_from_index(&index, &tree_id) != 0)
        return -1;

    Commit c;
    memset(&c, 0, sizeof(Commit));

    c.tree = tree_id;

    // 🔥 NEW: parent handling
    ObjectID parent_id;
    if (head_read(&parent_id) == 0) {
        c.parent = parent_id;
        c.has_parent = 1;
    } else {
        c.has_parent = 0;
    }

    snprintf(c.author, sizeof(c.author), "%s", pes_author());
    c.timestamp = (uint64_t)time(NULL);
    snprintf(c.message, sizeof(c.message), "%s", message);

    void *data;
    size_t len;
    if (commit_serialize(&c, &data, &len) != 0)
        return -1;

    if (object_write(OBJ_COMMIT, data, len, commit_id_out) != 0) {
        free(data);
        return -1;
    }
    free(data);

    if (head_update(commit_id_out) != 0)
        return -1;

    return 0;
}
