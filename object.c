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

// ─── TODO: Implement these ──────────────────────────────────────────────────

int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out) {

    // Step 1: Convert enum → string
    const char *type_str;
    if (type == OBJ_BLOB) type_str = "blob";
    else if (type == OBJ_TREE) type_str = "tree";
    else type_str = "commit";

    // Step 2: Build header
    char header[64];
    int header_len = snprintf(header, sizeof(header), "%s %zu", type_str, len);
    int full_header_len = header_len + 1;

    // Step 3: Allocate buffer
    size_t total_size = full_header_len + len;
    unsigned char *store = malloc(total_size);
    if (!store) return -1;

    // Step 4: Copy header + data
    memcpy(store, header, header_len);
    store[header_len] = '\0';
    memcpy(store + full_header_len, data, len);

    // Step 5: Compute hash
    ObjectID id;
    compute_hash(store, total_size, &id);

    // Step 6: Deduplication
    if (object_exists(&id)) {
        *id_out = id;
        free(store);
        return 0;
    }

    // ─── COMMIT 4 ADDITION ───

    // Step 7: Build final path
    char path[512];
    object_path(&id, path, sizeof(path));

    // Extract directory (.pes/objects/XX)
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/%.2x%.2x",
             OBJECTS_DIR, id.hash[0], id.hash[1]);

    // Ensure directories exist
    mkdir(OBJECTS_DIR, 0755);
    mkdir(dir, 0755);

    // Step 8: Temp file path
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

    // Flush file to disk
    fsync(fd);
    close(fd);

    // Step 9: Atomic rename
    if (rename(temp_path, path) < 0) {
        free(store);
        return -1;
    }

    // Step 10: fsync directory (persist rename)
    int dir_fd = open(dir, O_RDONLY);
    if (dir_fd >= 0) {
        fsync(dir_fd);
        close(dir_fd);
    }

    *id_out = id;
    free(store);
    return 0;
}

int object_read(const ObjectID *id, ObjectType *type_out, void **data_out, size_t *len_out) {
    // Not implemented yet
    (void)id;
    (void)type_out;
    (void)data_out;
    (void)len_out;
    return -1;
}
