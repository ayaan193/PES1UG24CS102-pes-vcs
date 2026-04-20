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
// (Keep EVERYTHING exactly same as your original file above this point)

// ... [ALL PROVIDED FUNCTIONS UNCHANGED] ...

// ─── IMPLEMENTATION ───────────────────────────────────────────────────

int commit_create(const char *message, ObjectID *commit_id_out) {

    // 1. Load index
    Index index;
    if (index_load(&index) != 0)
        return -1;

    // 2. Build tree
    ObjectID tree_id;
    if (tree_from_index(&index, &tree_id) != 0)
        return -1;

    // 3. Fill commit struct
    Commit c;
    memset(&c, 0, sizeof(Commit));

    c.tree = tree_id;
    c.has_parent = 0; // no parent yet

    snprintf(c.author, sizeof(c.author), "%s", pes_author());
    c.timestamp = (uint64_t)time(NULL);
    snprintf(c.message, sizeof(c.message), "%s", message);

    // 4. Serialize
    void *data;
    size_t len;
    if (commit_serialize(&c, &data, &len) != 0)
        return -1;

    // 5. Write object
    if (object_write(OBJ_COMMIT, data, len, commit_id_out) != 0) {
        free(data);
        return -1;
    }
    free(data);

    // 6. Update HEAD
    if (head_update(commit_id_out) != 0)
        return -1;

    return 0;
}
