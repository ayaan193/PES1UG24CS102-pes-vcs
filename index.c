#include "index.h"
#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INDEX_FILE ".pes/index"

// ─── LOAD INDEX FROM DISK ────────────────────────────────────────────────

int index_load(Index *idx) {
    if (!idx) return -1;

    FILE *f = fopen(INDEX_FILE, "rb");
    if (!f) {
        // If index doesn't exist → treat as empty
        idx->count = 0;
        return 0;
    }

    // Read count
    if (fread(&idx->count, sizeof(int), 1, f) != 1) {
        fclose(f);
        return -1;
    }

    if (idx->count < 0 || idx->count > MAX_INDEX_ENTRIES) {
        fclose(f);
        return -1;
    }

    // Read entries
    if (fread(idx->entries, sizeof(IndexEntry), idx->count, f) != (size_t)idx->count) {
        fclose(f);
        return -1;
    }

    fclose(f);
    return 0;
}

// ─── SAVE INDEX TO DISK ────────────────────────────────────────────────

int index_save(const Index *idx) {
    if (!idx) return -1;

    FILE *f = fopen(INDEX_FILE, "wb");
    if (!f) return -1;

    // Write count
    if (fwrite(&idx->count, sizeof(int), 1, f) != 1) {
        fclose(f);
        return -1;
    }

    // Write entries
    if (fwrite(idx->entries, sizeof(IndexEntry), idx->count, f) != (size_t)idx->count) {
        fclose(f);
        return -1;
    }

    fclose(f);
    return 0;
}
