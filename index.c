// index.c — Commit 5 (matches final logic)

#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "pes.h"

int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);

// PROVIDED

IndexEntry* index_find(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0)
            return &index->entries[i];
    }
    return NULL;
}

int index_remove(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0) {
            int remaining = index->count - i - 1;
            if (remaining > 0)
                memmove(&index->entries[i], &index->entries[i + 1],
                        remaining * sizeof(IndexEntry));
            index->count--;
            return index_save(index);
        }
    }
    return -1;
}

int index_status(const Index *index) {
    printf("Staged changes:\n");
    if (index->count == 0) printf("  (nothing to show)\n");
    for (int i = 0; i < index->count; i++)
        printf("  staged:     %s\n", index->entries[i].path);
    printf("\n");
    return 0;
}

// LOAD

int index_load(Index *index) {
    index->count = 0;

    FILE *fp = fopen(".pes/index", "r");
    if (!fp) return 0;

    while (index->count < MAX_INDEX_ENTRIES) {
        IndexEntry *e = &index->entries[index->count];
        char hex[HASH_HEX_SIZE + 1];

        if (fscanf(fp, "%o %64s %ld %u %255[^\n]\n",
                   &e->mode, hex, &e->mtime_sec, &e->size, e->path) != 5)
            break;

        hex_to_hash(hex, &e->hash);
        index->count++;
    }

    fclose(fp);
    return 0;
}

// SORT HELPER

static int cmp_entries(const void *a, const void *b) {
    const IndexEntry *ea = a;
    const IndexEntry *eb = b;
    return strcmp(ea->path, eb->path);
}

// SAVE (sorted + atomic)

int index_save(const Index *index) {
    Index temp = *index;

    qsort(temp.entries, temp.count,
          sizeof(IndexEntry), cmp_entries);

    FILE *fp = fopen(".pes/index.tmp", "w");
    if (!fp) return -1;

    for (int i = 0; i < temp.count; i++) {
        char hex[HASH_HEX_SIZE + 1];
        hash_to_hex(&temp.entries[i].hash, hex);

        fprintf(fp, "%o %s %ld %u %s\n",
                temp.entries[i].mode,
                hex,
                temp.entries[i].mtime_sec,
                temp.entries[i].size,
                temp.entries[i].path);
    }

    fflush(fp);
    fsync(fileno(fp));
    fclose(fp);

    rename(".pes/index.tmp", ".pes/index");

    return 0;
}

// ADD

int index_add(Index *index, const char *path) {
    struct stat st;
    if (stat(path, &st) != 0)
        return -1;

    FILE *fp = fopen(path, "rb");
    if (!fp)
        return -1;

    uint8_t *buf = malloc(st.st_size ? st.st_size : 1);
    if (!buf) {
        fclose(fp);
        return -1;
    }

    if (fread(buf, 1, st.st_size, fp) != (size_t)st.st_size) {
        fclose(fp);
        free(buf);
        return -1;
    }
    fclose(fp);

    ObjectID id;
    if (object_write(OBJ_BLOB, buf, st.st_size, &id) != 0) {
        free(buf);
        return -1;
    }
    free(buf);

    IndexEntry *e = index_find(index, path);

    if (e == NULL) {
        if (index->count >= MAX_INDEX_ENTRIES)
            return -1;
        e = &index->entries[index->count++];
    }

    strcpy(e->path, path);
    e->mode = (st.st_mode & S_IXUSR) ? 0100755 : 0100644;
    e->hash = id;
    e->mtime_sec = st.st_mtime;
    e->size = (uint32_t)st.st_size;

    return index_save(index);
}
