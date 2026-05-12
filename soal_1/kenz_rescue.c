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

//SOURCE DIR — diisi dari argumen saat program dijalankan
static char source_dir[4096];

//KONTEN tujuan.txt — dibuat on-the-fly dari KOORD: di tiap file
static char tujuan_content[4096];
static int  tujuan_size = 0;

//HELPER: build path asli ke source_dir
static void build_path(char *buf, size_t size, const char *path) {
    snprintf(buf, size, "%s%s", source_dir, path);
}

//HELPER: generate isi tujuan.txt on-the-fly
//Scan 1.txt sampai 7.txt, ambil baris KOORD:, gabungkan
static void generate_tujuan() {
    char fragments[7][512];
    int  frag_count = 0;

    /* scan file 1.txt sampai 7.txt */
    for (int i = 1; i <= 7; i++) {
        char filepath[4096];
        snprintf(filepath, sizeof(filepath), "%s/%d.txt", source_dir, i);

        FILE *fp = fopen(filepath, "r");
        if (!fp) continue;

        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            /* cek apakah baris diawali "KOORD:" */
            if (strncmp(line, "KOORD:", 6) == 0) {
                /* ambil isi setelah "KOORD: " */
                char *val = line + 7; /* skip "KOORD: " */
                /* hapus newline di akhir */
                val[strcspn(val, "\n")] = '\0';
                strncpy(fragments[frag_count], val, 511);
                frag_count++;
                break; /* satu KOORD per file */
            }
        }
        fclose(fp);
    }

    /* gabungkan semua fragmen */
    char combined[2048] = "";
    for (int i = 0; i < frag_count; i++) {
        strcat(combined, fragments[i]);
    }

    /* format final: "Tujuan Mas Amba: <gabungan>\n" */
    tujuan_size = snprintf(tujuan_content, sizeof(tujuan_content),
                           "Tujuan Mas Amba: %s\n", combined);
}

//FUSE: getattr — info/atribut file
static int kenz_getattr(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));

    /* khusus tujuan.txt — virtual, tidak ada di disk */
    if (strcmp(path, "/tujuan.txt") == 0) {
        generate_tujuan();
        stbuf->st_mode  = S_IFREG | 0444; /* regular file, read-only */
        stbuf->st_nlink = 1;
        stbuf->st_size  = tujuan_size;
        return 0;
    }

    /* file/folder lain — passthrough ke source */
    char real_path[4096];
    build_path(real_path, sizeof(real_path), path);

    int res = lstat(real_path, stbuf);
    if (res == -1) return -errno;
    return 0;
}

//FUSE: readdir — isi listing ls
static int kenz_readdir(const char *path, void *buf,
                         fuse_fill_dir_t filler, off_t offset,
                         struct fuse_file_info *fi) {
    (void) offset;
    (void) fi;

    /* buka source directory */
    char real_path[4096];
    build_path(real_path, sizeof(real_path), path);

    DIR *dp = opendir(real_path);
    if (!dp) return -errno;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        if (strcmp(de->d_name, ".") == 0) continue;
        if (strcmp(de->d_name, "..") == 0) continue;

        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino  = de->d_ino;
        st.st_mode = de->d_type << 12;
        filler(buf, de->d_name, &st, 0);
    }
    closedir(dp);

    /* tambahkan tujuan.txt virtual hanya di root */
    if (strcmp(path, "/") == 0) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_mode = S_IFREG | 0444;
        filler(buf, "tujuan.txt", &st, 0);
    }

    return 0;
}

//FUSE: open — buka file
static int kenz_open(const char *path, struct fuse_file_info *fi) {
    /* tujuan.txt virtual — selalu bisa dibuka read-only */
    if (strcmp(path, "/tujuan.txt") == 0) {
        if ((fi->flags & O_ACCMODE) != O_RDONLY)
            return -EACCES;
        return 0;
    }

    /* file lain — passthrough */
    char real_path[4096];
    build_path(real_path, sizeof(real_path), path);

    int res = open(real_path, fi->flags);
    if (res == -1) return -errno;
    close(res);
    return 0;
}

//FUSE: read — baca isi file
static int kenz_read(const char *path, char *buf, size_t size,
                      off_t offset, struct fuse_file_info *fi) {
    (void) fi;

    /* tujuan.txt virtual — generate on-the-fly lalu return */
    if (strcmp(path, "/tujuan.txt") == 0) {
        generate_tujuan();

        if (offset >= tujuan_size) return 0;
        if (offset + (off_t)size > tujuan_size)
            size = tujuan_size - offset;

        memcpy(buf, tujuan_content + offset, size);
        return size;
    }

    /* file lain — passthrough */
    char real_path[4096];
    build_path(real_path, sizeof(real_path), path);

    int fd = open(real_path, O_RDONLY);
    if (fd == -1) return -errno;

    int res = pread(fd, buf, size, offset);
    if (res == -1) res = -errno;

    close(fd);
    return res;
}

//FUSE OPERATIONS STRUCT
static struct fuse_operations kenz_oper = {
    .getattr = kenz_getattr,
    .readdir = kenz_readdir,
    .open    = kenz_open,
    .read    = kenz_read,
};

//MAIN — terima 2 argumen: source_dir dan mount_dir
int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_dir> <mount_dir>\n", argv[0]);
        return 1;
    }

    /* simpan source_dir dari argumen pertama */
    realpath(argv[1], source_dir);

    /* geser argv supaya FUSE dapat mount_dir di posisi yang benar */
    argv[1] = argv[2];
    argc = 2;

    umask(0);
    return fuse_main(argc, argv, &kenz_oper, NULL);
}
