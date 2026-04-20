#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "pes.h"

int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);

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

// SAVE (no atomicity yet)
int index_save(const Index *index) {
    FILE *fp = fopen(".pes/index", "w");
    if (!fp) return -1;

    for (int i = 0; i < index->count; i++) {
        char hex[HASH_HEX_SIZE + 1];
        hash_to_hex(&index->entries[i].hash, hex);

        fprintf(fp, "%o %s %ld %u %s\n",
                index->entries[i].mode,
                hex,
                index->entries[i].mtime_sec,
                index->entries[i].size,
                index->entries[i].path);
    }

    fclose(fp);
    return 0;
}

// ADD (basic)
int index_add(Index *index, const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;

    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;

    uint8_t *buf = malloc(st.st_size ? st.st_size : 1);
    fread(buf, 1, st.st_size, fp);
    fclose(fp);

    ObjectID id;
    object_write(OBJ_BLOB, buf, st.st_size, &id);
    free(buf);

    IndexEntry *e = index_find(index, path);
    if (!e) {
        e = &index->entries[index->count++];
    }

    strcpy(e->path, path);
    e->mode = 0100644;
    e->hash = id;
    e->mtime_sec = st.st_mtime;
    e->size = st.st_size;

    return index_save(index);
}
