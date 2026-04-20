#include "index.h"
#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    Index idx;
    idx.count = 1;

    strcpy(idx.entries[0].path, "a.txt");
    idx.entries[0].mode = 0100644;

    FILE *f = fopen("a.txt", "rb");
    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    rewind(f);

    void *buf = malloc(len);
    fread(buf, 1, len, f);
    fclose(f);

    ObjectID blob_id;
    object_write(OBJ_BLOB, buf, len, &blob_id);
    free(buf);

    idx.entries[0].hash = blob_id;

    return index_save(&idx);
}
