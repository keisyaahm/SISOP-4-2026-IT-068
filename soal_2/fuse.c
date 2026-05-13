#define FUSE_USE_VERSION 28
#define _DEFAULT_SOURCE

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <limits.h>

#define XOR_KEY 0x76

static char enc_storage[PATH_MAX];

// XOR encrypt/decrypt — operasi sama untuk keduanya
static void xor_buffer(char *buf, size_t size) {
    for (size_t i = 0; i < size; i++) {
        buf[i] ^= XOR_KEY;
    }
}

// FUSE: getattr
static int moo_getattr(const char *path, struct stat *st) {
    memset(st, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        char real[PATH_MAX * 2];
        snprintf(real, sizeof(real), "%s", enc_storage);
        if (lstat(real, st) == -1) return -errno;
        return 0;
    }

    // Coba sebagai file dulu (.enc)
    char enc_path[PATH_MAX * 2];
    snprintf(enc_path, sizeof(enc_path), "%s%s.enc", enc_storage, path);
    struct stat tmp;
    if (lstat(enc_path, &tmp) == 0 && S_ISREG(tmp.st_mode)) {
        *st = tmp;
        return 0;
    }

    // Coba sebagai direktori
    char dir_path[PATH_MAX * 2];
    snprintf(dir_path, sizeof(dir_path), "%s%s", enc_storage, path);
    if (lstat(dir_path, st) == 0) return 0;

    return -ENOENT;
}

// FUSE: access
static int moo_access(const char *path, int mask) {
    if (strcmp(path, "/") == 0) {
        return access(enc_storage, mask) == -1 ? -errno : 0;
    }

    char enc_path[PATH_MAX * 2];
    snprintf(enc_path, sizeof(enc_path), "%s%s.enc", enc_storage, path);
    if (access(enc_path, mask) == 0) return 0;

    char dir_path[PATH_MAX * 2];
    snprintf(dir_path, sizeof(dir_path), "%s%s", enc_storage, path);
    if (access(dir_path, mask) == 0) return 0;

    return -ENOENT;
}

// FUSE: readdir — tampilkan nama tanpa .enc
static int moo_readdir(const char *path, void *buf,
                       fuse_fill_dir_t filler, off_t offset,
                       struct fuse_file_info *fi) {
    (void) offset;
    (void) fi;

    char real[PATH_MAX * 2];
    snprintf(real, sizeof(real), "%s%s", enc_storage, path);

    DIR *dp = opendir(real);
    if (!dp) return -errno;

    filler(buf, ".",  NULL, 0);
    filler(buf, "..", NULL, 0);

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        if (de->d_name[0] == '.') continue;

        char display_name[NAME_MAX + 1];
        strncpy(display_name, de->d_name, NAME_MAX);
        display_name[NAME_MAX] = '\0';

        // Hapus .enc dari nama saat ditampilkan
        size_t len = strlen(display_name);
        if (len > 4 && strcmp(display_name + len - 4, ".enc") == 0) {
            display_name[len - 4] = '\0';
        }

        filler(buf, display_name, NULL, 0);
    }
    closedir(dp);
    return 0;
}

// FUSE: mkdir
static int moo_mkdir(const char *path, mode_t mode) {
    char real[PATH_MAX * 2];
    snprintf(real, sizeof(real), "%s%s", enc_storage, path);
    if (mkdir(real, mode) == -1) return -errno;
    return 0;
}

// FUSE: rmdir
static int moo_rmdir(const char *path) {
    char real[PATH_MAX * 2];
    snprintf(real, sizeof(real), "%s%s", enc_storage, path);
    if (rmdir(real) == -1) return -errno;
    return 0;
}

// FUSE: create
static int moo_create(const char *path, mode_t mode,
                      struct fuse_file_info *fi) {
    char enc_path[PATH_MAX * 2];
    snprintf(enc_path, sizeof(enc_path), "%s%s.enc", enc_storage, path);

    int fd = open(enc_path, fi->flags | O_CREAT | O_WRONLY | O_TRUNC, mode);
    if (fd == -1) return -errno;

    fi->fh = fd;
    return 0;
}

// FUSE: open
static int moo_open(const char *path, struct fuse_file_info *fi) {
    char enc_path[PATH_MAX * 2];
    snprintf(enc_path, sizeof(enc_path), "%s%s.enc", enc_storage, path);

    int fd = open(enc_path, fi->flags);
    if (fd == -1) return -errno;

    fi->fh = fd;
    return 0;
}

// FUSE: read — baca .enc lalu XOR → plaintext
static int moo_read(const char *path, char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi) {
    int fd;
    int need_close = 0;

    if (fi && fi->fh) {
        fd = fi->fh;
    } else {
        char enc_path[PATH_MAX * 2];
        snprintf(enc_path, sizeof(enc_path), "%s%s.enc", enc_storage, path);
        fd = open(enc_path, O_RDONLY);
        if (fd == -1) return -errno;
        need_close = 1;
    }

    int res = pread(fd, buf, size, offset);
    if (res == -1) {
        if (need_close) close(fd);
        return -errno;
    }

    // Dekripsi XOR on-the-fly
    xor_buffer(buf, res);

    if (need_close) close(fd);
    return res;
}

// FUSE: write — XOR plaintext → simpan ke .enc
static int moo_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi) {
    char *enc_buf = malloc(size);
    if (!enc_buf) return -ENOMEM;

    memcpy(enc_buf, buf, size);
    xor_buffer(enc_buf, size);

    int fd;
    int need_close = 0;

    if (fi && fi->fh) {
        fd = fi->fh;
    } else {
        char enc_path[PATH_MAX * 2];
        snprintf(enc_path, sizeof(enc_path), "%s%s.enc", enc_storage, path);
        fd = open(enc_path, O_WRONLY);
        if (fd == -1) {
            free(enc_buf);
            return -errno;
        }
        need_close = 1;
    }

    int res = pwrite(fd, enc_buf, size, offset);
    free(enc_buf);

    if (res == -1) {
        if (need_close) close(fd);
        return -errno;
    }

    if (need_close) close(fd);
    return res;
}

// FUSE: truncate
static int moo_truncate(const char *path, off_t size) {
    char enc_path[PATH_MAX * 2];
    snprintf(enc_path, sizeof(enc_path), "%s%s.enc", enc_storage, path);
    if (truncate(enc_path, size) == -1) return -errno;
    return 0;
}

// FUSE: unlink — hapus file .enc
static int moo_unlink(const char *path) {
    char enc_path[PATH_MAX * 2];
    snprintf(enc_path, sizeof(enc_path), "%s%s.enc", enc_storage, path);
    if (unlink(enc_path) == -1) return -errno;
    return 0;
}

// FUSE: utimens
static int moo_utimens(const char *path, const struct timespec ts[2]) {
    char enc_path[PATH_MAX * 2];
    snprintf(enc_path, sizeof(enc_path), "%s%s.enc", enc_storage, path);
    int res = utimensat(0, enc_path, ts, 0);
    if (res == -1) return -errno;
    return 0;
}

static struct fuse_operations moo_ops = {
    .getattr  = moo_getattr,
    .access   = moo_access,
    .readdir  = moo_readdir,
    .mkdir    = moo_mkdir,
    .rmdir    = moo_rmdir,
    .create   = moo_create,
    .open     = moo_open,
    .read     = moo_read,
    .write    = moo_write,
    .truncate = moo_truncate,
    .unlink   = moo_unlink,
    .utimens  = moo_utimens,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <encrypted_storage> <mount_point>\n",
                argv[0]);
        return 1;
    }

    if (realpath(argv[1], enc_storage) == NULL) {
        perror("realpath encrypted_storage");
        return 1;
    }

    // Susun ulang argv: hapus argv[1], teruskan sisanya ke fuse_main
    int new_argc = argc - 1;
    char **new_argv = malloc(sizeof(char *) * (new_argc + 1));
    if (!new_argv) return 1;

    new_argv[0] = argv[0];
    for (int i = 1; i < new_argc; i++) {
        new_argv[i] = argv[i + 1];
    }
    new_argv[new_argc] = NULL;

    int ret = fuse_main(new_argc, new_argv, &moo_ops, NULL);
    free(new_argv);
    return ret;
}
