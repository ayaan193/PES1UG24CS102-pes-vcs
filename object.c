// object.c — Content-addressable object store

#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <openssl/evp.h>

// ─── PROVIDED ────────────────────────────────────────────────────────────────

void hash_to_hex(const ObjectID *id, char *hex_out) {
    for (int i = 0; i < HASH_SIZE; i++) {
        sprintf(hex_out + i * 2, "%02x", id->hash[i]);
    }
    hex_out[HASH_HEX_SIZE] = '\0';
}

int hex_to_hash(const char *hex, ObjectID *id_out) {
    if (strlen(hex) < HASH_HEX_SIZE) return -1;
    for (int i = 0; i < HASH_SIZE; i++) {
        unsigned int byte;
        if (sscanf(hex + i * 2, "%2x", &byte) != 1) return -1;
        id_out->hash[i] = (uint8_t)byte;
    }
    return 0;
}

void compute_hash(const void *data, size_t len, ObjectID *id_out) {
    unsigned int hash_len;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, data, len);
    EVP_DigestFinal_ex(ctx, id_out->hash, &hash_len);
    EVP_MD_CTX_free(ctx);
}

void object_path(const ObjectID *id, char *path_out, size_t path_size) {
    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(id, hex);
    snprintf(path_out, path_size, "%s/%.2s/%s", OBJECTS_DIR, hex, hex + 2);
}

int object_exists(const ObjectID *id) {
    char path[512];
    object_path(id, path, sizeof(path));
    return access(path, F_OK) == 0;
}

// ─── IMPLEMENTATION ──────────────────────────────────────────────────────────

int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out) {

    const char *type_str;
    if (type == OBJ_BLOB) type_str = "blob";
    else if (type == OBJ_TREE) type_str = "tree";
    else type_str = "commit";

    char header[64];
    int header_len = snprintf(header, sizeof(header), "%s %zu", type_str, len);
    int full_header_len = header_len + 1;

    size_t total_size = full_header_len + len;
    unsigned char *store = malloc(total_size);
    if (!store) return -1;

    memcpy(store, header, header_len);
    store[header_len] = '\0';
    memcpy(store + full_header_len, data, len);

    ObjectID id;
    compute_hash(store, total_size, &id);

    if (object_exists(&id)) {
        *id_out = id;
        free(store);
        return 0;
    }

    char path[512];
    object_path(&id, path, sizeof(path));

    char dir[512];
    snprintf(dir, sizeof(dir), "%s/%.2x%.2x",
             OBJECTS_DIR, id.hash[0], id.hash[1]);

    mkdir(OBJECTS_DIR, 0755);
    mkdir(dir, 0755);

    char temp_path[512];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", path);

    int fd = open(temp_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        free(store);
        return -1;
    }

    if (write(fd, store, total_size) != (ssize_t)total_size) {
        close(fd);
        free(store);
        return -1;
    }

    fsync(fd);
    close(fd);

    if (rename(temp_path, path) < 0) {
        free(store);
        return -1;
    }

    int dir_fd = open(dir, O_RDONLY);
    if (dir_fd >= 0) {
        fsync(dir_fd);
        close(dir_fd);
    }

    *id_out = id;
    free(store);
    return 0;
}

// ─── COMMIT 5: object_read ──────────────────────────────────────────────────

int object_read(const ObjectID *id, ObjectType *type_out, void **data_out, size_t *len_out) {

    char path[512];
    object_path(id, path, sizeof(path));

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    rewind(f);

    unsigned char *buffer = malloc(file_size);
    if (!buffer) {
        fclose(f);
        return -1;
    }

    fread(buffer, 1, file_size, f);
    fclose(f);

    // Verify integrity
    ObjectID computed;
    compute_hash(buffer, file_size, &computed);

    if (memcmp(computed.hash, id->hash, HASH_SIZE) != 0) {
        free(buffer);
        return -1;
    }

    // Parse header
    char *data_start = memchr(buffer, '\0', file_size);
    if (!data_start) {
        free(buffer);
        return -1;
    }

    *data_start = '\0';

    // Determine type
    if (strncmp((char *)buffer, "blob", 4) == 0)
        *type_out = OBJ_BLOB;
    else if (strncmp((char *)buffer, "tree", 4) == 0)
        *type_out = OBJ_TREE;
    else
        *type_out = OBJ_COMMIT;

    // Parse size
    size_t size;
    sscanf((char *)buffer, "%*s %zu", &size);

    // Extract data
    void *data = malloc(size);
    memcpy(data, data_start + 1, size);

    *data_out = data;
    *len_out = size;

    free(buffer);
    return 0;
}
