#define FUSE_USE_VERSION 28
#define _DEFAULT_SOURCE

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <limits.h>

static char source_dir[PATH_MAX];

//HELPER: build path asli
static void build_path(char *buf, size_t size, const char *path) {
    snprintf(buf, size, "%s%s", source_dir, path);
}

//HELPER: generate tujuan.txt on-the-fly (buffer lokal, aman)
static int generate_tujuan(char *buf, size_t buf_size) {
    char result[4096] = {0};
    strncpy(result, "Tujuan Mas Amba: ", sizeof(result) - 1);

    for (int i = 1; i <= 7; i++) {
	char filepath[PATH_MAX * 2];
	snprintf(filepath, sizeof(filepath), "%s/%d.txt", source_dir, i);

        FILE *fp = fopen(filepath, "r");
        if (!fp) continue;

        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            /* konsisten cek dan skip "KOORD: " 7 karakter */
            if (strncmp(line, "KOORD: ", 7) == 0) {
                char *val = line + 7;
                size_t len = strlen(val);
                if (len > 0 && val[len-1] == '\n') val[len-1] = '\0';
                strncat(result, val, sizeof(result) - strlen(result) - 1);
                break;
            }
        }
        fclose(fp);
    }

    strncat(result, "\n", sizeof(result) - strlen(result) - 1);

    size_t total = strlen(result);
    if (total > buf_size) total = buf_size;
    memcpy(buf, result, total);
    return (int)total;
}

//FUSE: getattr
static int kenz_getattr(const char *path, struct stat *st) {
    memset(st, 0, sizeof(struct stat));

    /* handle root directory eksplisit */
    if (strcmp(path, "/") == 0) {
        st->st_mode  = S_IFDIR | 0755;
        st->st_nlink = 2;
        return 0;
    }

    /* file virtual tujuan.txt */
    if (strcmp(path, "/tujuan.txt") == 0) {
        char tmp[4096];
        int len = generate_tujuan(tmp, sizeof(tmp));
        st->st_mode  = S_IFREG | 0444;
        st->st_nlink = 1;
        st->st_size  = len;
        st->st_uid   = 0;
        st->st_gid   = 0;
        st->st_atime = 0;
        st->st_mtime = 0;
        st->st_ctime = 0;
        return 0;
    }

    /* passthrough */
    char real[PATH_MAX];
    build_path(real, sizeof(real), path);
    int res = lstat(real, st);
    if (res == -1) return -errno;
    return 0;
}

//FUSE: readdir
static int kenz_readdir(const char *path, void *buf,
                         fuse_fill_dir_t filler, off_t offset,
                         struct fuse_file_info *fi) {
    (void) offset;
    (void) fi;

    /* hanya handle root */
    if (strcmp(path, "/") != 0) return -ENOENT;

    filler(buf, ".",  NULL, 0);
    filler(buf, "..", NULL, 0);

    /* baca isi source directory */
    DIR *dp = opendir(source_dir);
    if (!dp) return -errno;

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        if (de->d_name[0] == '.') continue;
        filler(buf, de->d_name, NULL, 0);
    }
    closedir(dp);

    /* tambahkan file virtual */
    filler(buf, "tujuan.txt", NULL, 0);
    return 0;
}

//FUSE: open
static int kenz_open(const char *path, struct fuse_file_info *fi) {
    if (strcmp(path, "/tujuan.txt") == 0) {
        if ((fi->flags & O_ACCMODE) != O_RDONLY) return -EACCES;
        return 0;
    }

    char real[PATH_MAX];
    build_path(real, sizeof(real), path);
    int fd = open(real, fi->flags);
    if (fd == -1) return -errno;
    close(fd);
    return 0;
}

//FUSE: read
static int kenz_read(const char *path, char *buf, size_t size,
                      off_t offset, struct fuse_file_info *fi) {
    (void) fi;

    if (strcmp(path, "/tujuan.txt") == 0) {
        char content[4096];
        int total = generate_tujuan(content, sizeof(content));
        if (offset >= total) return 0;
        if (offset + (off_t)size > total)
            size = total - offset;
        memcpy(buf, content + offset, size);
        return (int)size;
    }

    char real[PATH_MAX];
    build_path(real, sizeof(real), path);
    int fd = open(real, O_RDONLY);
    if (fd == -1) return -errno;
    int res = pread(fd, buf, size, offset);
    if (res == -1) res = -errno;
    close(fd);
    return res;
}

//FUSE operations struct
static struct fuse_operations kenz_ops = {
    .getattr = kenz_getattr,
    .readdir = kenz_readdir,
    .open    = kenz_open,
    .read    = kenz_read,
};

//MAIN
int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_dir> <mount_dir>\n", argv[0]);
        return 1;
    }

    if (realpath(argv[1], source_dir) == NULL) {
        perror("realpath");
        return 1;
    }

    char *fuse_argv[3];
    fuse_argv[0] = argv[0];
    fuse_argv[1] = argv[2];
    fuse_argv[2] = NULL;

    return fuse_main(2, fuse_argv, &kenz_ops, NULL);
}
