# Laporan Praktikum Sistem Operasi 2026 - Modul 4
## FUSE, Docker, dan Samba

**Nama:** Keisya Halimah Mulia  
**NRP:** 5027251068  
**Kelas:** A

---

## Daftar Isi

- [Persiapan Direktori dan Repository](#persiapan-direktori-dan-repository)
- [Soal 1: Save Asisten Kenz (FUSE Passthrough + Virtual File)](#soal-1-save-asisten-kenz)
  - [Deskripsi Soal 1](#deskripsi-soal-1)
  - [Penjelasan Kode Soal 1](#penjelasan-kode-soal-1)
  - [Cara Kompilasi dan Menjalankan Soal 1](#cara-kompilasi-dan-menjalankan-soal-1)
  - [Output dan Hasil Soal 1](#output-dan-hasil-soal-1)
  - [Error dan Solusi Soal 1](#error-dan-solusi-soal-1)
- [Soal 2: Poke MOO (FUSE Enkripsi XOR + Docker + Client)](#soal-2-poke-moo)
  - [Deskripsi Soal 2](#deskripsi-soal-2)
  - [Penjelasan Kode Soal 2](#penjelasan-kode-soal-2)
  - [Cara Kompilasi dan Menjalankan Soal 2](#cara-kompilasi-dan-menjalankan-soal-2)
  - [Output dan Hasil Soal 2](#output-dan-hasil-soal-2)
  - [Error dan Solusi Soal 2](#error-dan-solusi-soal-2)
- [Soal 3: LibraryIT (Docker + Samba)](#soal-3-libraryit)
  - [Deskripsi Soal 3](#deskripsi-soal-3)
  - [Penjelasan Kode Soal 3](#penjelasan-kode-soal-3)
  - [Cara Menjalankan Soal 3](#cara-menjalankan-soal-3)
  - [Output dan Hasil Soal 3](#output-dan-hasil-soal-3)
  - [Error dan Solusi Soal 3](#error-dan-solusi-soal-3)

---

## Persiapan Direktori dan Repository

```bash
mkdir SISOP-4-2026-IT-068 && cd SISOP-4-2026-IT-068
git init
git remote add origin https://github.com/keisyaahm/SISOP-4-2026-IT-068.git
git branch -M main

# Struktur folder
mkdir -p soal_1/mnt
mkdir -p soal_2/{encrypted_storage/tests,fuse_mount}
mkdir -p soal_3/{data/{ebooks,papers,sourcecode,docs},logs}

# .gitignore soal_1
cat > soal_1/.gitignore << 'EOF'
kenz_rescue
mnt/
amba_files/
EOF

# .gitignore soal_2
cat > soal_2/.gitignore << 'EOF'
fuse
fuse_mount/
client
server
EOF

git add .
git commit -m "init: setup struktur repo modul 4"
git push -u origin main
```

Struktur repository yang masuk GitHub:
```
SISOP-4-2026-IT-068/
├── soal_1/
│   ├── kenz_rescue.c
│   └── amba_files/
│       ├── 1.txt ... 7.txt
├── soal_2/
│   ├── fuse.c
│   ├── client.c
│   └── Dockerfile
└── soal_3/
    ├── Dockerfile
    ├── docker-compose.yml
    ├── smb.conf
    ├── entrypoint.sh
    ├── data/
    │   ├── ebooks/
    │   ├── papers/
    │   ├── sourcecode/
    │   └── docs/
    └── logs/
        └── libraryit.log
```

---

## Soal 1: Save Asisten Kenz

### Deskripsi Soal 1

Sebastian menemukan flashdisk berisi 7 file log ekspedisi (`1.txt` s/d `7.txt`). Setiap file punya satu baris `KOORD: <fragmen>`. Ia harus menggabungkan fragmen dari semua file tanpa mengubah isi flashdisk.

Soal minta membuat program FUSE `kenz_rescue.c` dengan 4 poin:

**Poin A** — Download arsip `amba_files.zip`, unzip ke `amba_files/`, hapus zip-nya.

**Poin B** — FUSE passthrough: `cat mnt/1.txt` = `cat amba_files/1.txt` (byte-identical).

**Poin C** — Tambahkan file virtual `tujuan.txt` di mount directory. File ini muncul di `ls mnt/` tapi tidak ada fisiknya di `amba_files/`.

**Poin D** — Saat `cat mnt/tujuan.txt`, isi dibuat on-the-fly dengan menggabungkan semua fragmen `KOORD:` dari `1.txt`–`7.txt`, format: `Tujuan Mas Amba: <gabungan>\n`.

---

### Penjelasan Kode Soal 1

#### Poin A — Download dan Setup

```bash
cd ~/SISOP-4-2026-IT-068/soal_1
curl -L "https://drive.google.com/uc?export=download&id=1nLXFhptDo2mnUlZsw8pTWyAVpV49W20U" -o amba_files.zip
unzip amba_files.zip
rm amba_files.zip   # wajib dihapus sesuai soal
ls amba_files/      # harus muncul 1.txt s/d 7.txt
```

#### Poin B — Passthrough (getattr, readdir, open, read)

Semua operasi untuk file `1.txt`–`7.txt` diteruskan langsung ke source directory menggunakan path fisik yang dibangun dari `source_dir + path`:

```c
static void build_path(char *buf, size_t size, const char *path) {
    snprintf(buf, size, "%s%s", source_dir, path);
}

// getattr passthrough — ambil metadata dari file fisik
char real[PATH_MAX];
build_path(real, sizeof(real), path);
int res = lstat(real, st);

// readdir — baca isi folder source
DIR *dp = opendir(source_dir);
while ((de = readdir(dp)) != NULL) {
    if (de->d_name[0] == '.') continue;
    filler(buf, de->d_name, NULL, 0);
}

// read passthrough — baca isi file fisik
int fd = open(real, O_RDONLY);
int res = pread(fd, buf, size, offset);
```

#### Poin C — Virtual File tujuan.txt

`tujuan.txt` tidak ada di `amba_files/` tapi harus muncul di `ls mnt/` dan bisa di-`stat`. Ini dicapai dengan membuat stat buatan di `getattr` dan menambahkan entry di `readdir`:

```c
// getattr — deteksi path /tujuan.txt, return stat buatan
if (strcmp(path, "/tujuan.txt") == 0) {
    char tmp[4096];
    int len = generate_tujuan(tmp, sizeof(tmp)); // hitung ukuran on-the-fly
    st->st_mode  = S_IFREG | 0444; // read-only
    st->st_nlink = 1;
    st->st_size  = len;
    // timestamp 0 (epoch) sesuai spesifikasi soal
    st->st_atime = st->st_mtime = st->st_ctime = 0;
    return 0;
}

// readdir — tambahkan tujuan.txt di listing
filler(buf, "tujuan.txt", NULL, 0);
```

#### Poin D — On-the-fly Content tujuan.txt

Saat `cat mnt/tujuan.txt`, fungsi `generate_tujuan` dipanggil. Fungsi ini membuka `1.txt`–`7.txt` secara berurutan, mencari baris yang diawali `KOORD: `, mengambil nilainya, dan menggabungkannya:

```c
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
            if (strncmp(line, "KOORD: ", 7) == 0) {
                char *val = line + 7;
                size_t len = strlen(val);
                // Hapus newline di akhir
                if (len > 0 && val[len-1] == '\n') val[len-1] = '\0';
                strncat(result, val, sizeof(result) - strlen(result) - 1);
                break; // satu KOORD per file
            }
        }
        fclose(fp);
    }
    // Tambah tepat satu newline di akhir (sesuai spesifikasi)
    strncat(result, "\n", sizeof(result) - strlen(result) - 1);

    size_t total = strlen(result);
    if (total > buf_size) total = buf_size;
    memcpy(buf, result, total);
    return (int)total;
}
```

---

### Cara Kompilasi dan Menjalankan Soal 1

```bash
cd ~/SISOP-4-2026-IT-068/soal_1

# Install dependency
sudo apt install -y libfuse-dev pkg-config fuse

# Compile
gcc -Wall -o kenz_rescue kenz_rescue.c $(pkg-config fuse --cflags --libs)

# Mount
./kenz_rescue amba_files mnt

# Verifikasi mount
mountpoint mnt
mount | grep fuse_mount
```

---

### Output dan Hasil Soal 1

**Test Poin B — Passthrough byte-identical:**
```bash
for i in 1 2 3 4 5 6 7; do
    diff mnt/$i.txt amba_files/$i.txt && echo "$i.txt OK"
done
```
Expected:
```
1.txt OK
2.txt OK
3.txt OK
4.txt OK
5.txt OK
6.txt OK
7.txt OK
```

**Test Poin C — Virtual file:**
```bash
ls mnt/        # muncul tujuan.txt
ls amba_files/ # tidak ada tujuan.txt
stat mnt/tujuan.txt
```
Expected `stat`:
```
Access: (0444/-r--r--r--)
Size: 66
```

**Test Poin D — On-the-fly content:**
```bash
cat mnt/tujuan.txt
# Output: Tujuan Mas Amba: <gabungan KOORD dari 7 file>

wc -c mnt/tujuan.txt
# Angka harus sama dengan Size di stat
```

**Unmount:**
```bash
fusermount -u mnt
mountpoint mnt  # harus: mnt is not a mountpoint
ls mnt/         # harus kosong
```

---

### Error dan Solusi Soal 1

**Error 1 — Warning compile: `'%d' directive output may be truncated`**

Muncul saat compile karena buffer `PATH_MAX` (4096) secara teoritis bisa penuh jika dikombinasikan dengan format string `%s/%d.txt`.

```
kenz_rescue.c:29:50: warning: '%d' directive output may be truncated
```

Solusi: Perbesar ukuran buffer filepath menjadi `PATH_MAX * 2`:
```c
// Sebelum
char filepath[PATH_MAX];

// Sesudah
char filepath[PATH_MAX * 2];
```

**Error 2 — `cat mnt/tujuan.txt: No such file or directory`**

Terjadi saat working directory salah (masih di dalam `amba_files/` bukan di `soal_1/`).

Solusi:
```bash
cd ~/SISOP-4-2026-IT-068/soal_1  # pastikan di folder soal_1
cat mnt/tujuan.txt
```

---

## Soal 2: Poke MOO

### Deskripsi Soal 2

MOO ingin mini-database service yang aman. Semua file yang dibuat lewat `fuse_mount` harus terenkripsi otomatis menggunakan XOR key `0x76` dan disimpan di `encrypted_storage` dengan ekstensi `.enc`.

**Poin A & B** — FUSE penuh dengan 12 operasi: `getattr`, `readdir`, `mkdir`, `rmdir`, `create`, `open`, `read`, `write`, `truncate`, `unlink`, `access`, `utimens`. `fuse_mount` berfungsi seperti filesystem biasa.

**Poin C** — Enkripsi/dekripsi XOR on-the-fly:
- Tulis `halo.txt` di `fuse_mount` → tersimpan sebagai `halo.txt.enc` di `encrypted_storage` (isi terenkripsi)
- Baca `halo.txt` di `fuse_mount` → dibaca dari `halo.txt.enc` dan didekripsi otomatis

**Poin D** — Download `notes.csv.enc` ke `encrypted_storage/tests/`. Baca lewat `fuse_mount/tests/notes.csv` → harus terdekripsi.

**Containerization** — Build image Docker `soal-2-modul-4-sisop`, jalankan container `db_app` dengan bind mount `fuse_mount` ke `/app/db`.

**Integration** — Buat `client.c` untuk berinteraksi dengan server via TCP port 9000.

---

### Penjelasan Kode Soal 2

#### Poin A & B — FUSE Penuh

Semua 12 operasi diimplementasikan. Path di `fuse_mount` dipetakan ke path `.enc` di `encrypted_storage`:

```c
// Untuk file: tambahkan .enc
snprintf(enc_path, sizeof(enc_path), "%s%s.enc", enc_storage, path);

// Untuk folder: tidak tambahkan .enc
snprintf(dir_path, sizeof(dir_path), "%s%s", enc_storage, path);
```

`readdir` menampilkan nama file tanpa `.enc` dengan cara memotong ekstensi saat listing:
```c
// Hapus .enc dari nama saat ditampilkan ke user
size_t len = strlen(display_name);
if (len > 4 && strcmp(display_name + len - 4, ".enc") == 0) {
    display_name[len - 4] = '\0';
}
filler(buf, display_name, NULL, 0);
```

#### Poin C — Enkripsi/Dekripsi XOR On-the-fly

XOR key `0x76` dipakai untuk enkripsi dan dekripsi (operasi XOR bersifat reversibel):

```c
#define XOR_KEY 0x76

static void xor_buffer(char *buf, size_t size) {
    for (size_t i = 0; i < size; i++) {
        buf[i] ^= XOR_KEY;
    }
}

// READ: baca .enc → XOR → tampilkan plaintext
static int moo_read(...) {
    int res = pread(fd, buf, size, offset);
    xor_buffer(buf, res); // dekripsi on-the-fly
    return res;
}

// WRITE: terima plaintext → XOR → simpan ke .enc
static int moo_write(...) {
    char *enc_buf = malloc(size);
    memcpy(enc_buf, buf, size);
    xor_buffer(enc_buf, size); // enkripsi on-the-fly
    int res = pwrite(fd, enc_buf, size, offset);
    free(enc_buf);
    return res;
}
```

#### Dockerfile

```dockerfile
FROM ubuntu:latest
WORKDIR /app
COPY server /app/server
RUN mkdir -p /app/db && chmod +x /app/server
EXPOSE 9000
CMD ["./server"]
```

#### client.c — TCP Client Interaktif

```c
// Connect ke server port 9000
sock = socket(AF_INET, SOCK_STREAM, 0);
connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

// Loop interaktif: input dari user → kirim → terima balasan
while (1) {
    printf("db > ");
    fgets(send_buf, sizeof(send_buf), stdin);
    // strip newline, tambah \n sebagai terminator
    send(sock, send_buf, strlen(send_buf), 0);
    int n = recv(sock, recv_buf, sizeof(recv_buf) - 1, 0);
    printf("%s", recv_buf);
}
```

---

### Cara Kompilasi dan Menjalankan Soal 2

```bash
cd ~/SISOP-4-2026-IT-068/soal_2

# Compile FUSE
gcc -Wall -o fuse fuse.c $(pkg-config fuse --cflags --libs)

# Mount FUSE
./fuse encrypted_storage fuse_mount
mountpoint fuse_mount  # verifikasi

# Build Docker image
docker build -t soal-2-modul-4-sisop .

# Jalankan container dengan bind mount
docker run -d \
  --name db_app \
  -p 9000:9000 \
  -v $(pwd)/fuse_mount:/app/db \
  soal-2-modul-4-sisop

# Compile client
gcc -Wall -o client client.c

# Jalankan client
./client
```

---

### Output dan Hasil Soal 2

**Test Poin B & C — Enkripsi XOR:**
```bash
echo "isinya ini harusnya" > fuse_mount/file1.txt

# Baca lewat fuse_mount → plaintext
cat fuse_mount/file1.txt
# Output: isinya ini harusnya

# Cek file .enc tersimpan di encrypted_storage
ls encrypted_storage/
# Output: file1.txt.enc

# Cek isi .enc → terenkripsi (karakter aneh)
cat encrypted_storage/file1.txt.enc
# Output: karakter garbled (VV|...)
```

**Test Poin D — notes.csv.enc:**
```bash
cat fuse_mount/tests/notes.csv
# Output: author,notes
#         admin,TEST_SUCCESS
```

**Test Integration — Client:**
```
Connected to DB Server on port 9000
Type HELP for available commands

db > CREATE DATABASE tests
DATABASE CREATED

db > CREATE TABLE tests users email password
TABLE CREATED

db > LIST DATABASE
tests
```

**Verifikasi file terenkripsi dari operasi database:**
```bash
ls encrypted_storage/tests/
# Output: history.log.enc  users.csv.enc
```

---

### Error dan Solusi Soal 2

**Error 1 — `fuse: mountpoint is not empty`**

Terjadi saat `fuse_mount` sudah berisi data dari Docker bind mount sebelumnya.

```
./fuse encrypted_storage fuse_mount
fuse: mountpoint is not empty
```

Solusi: Unmount dulu, lalu mount ulang dengan flag `nonempty`:
```bash
fusermount -u fuse_mount 2>/dev/null
./fuse encrypted_storage fuse_mount -o nonempty
```

**Error 2 — `realpath encrypted_storage: No such file or directory`**

Flag `-o` diletakkan sebelum argumen source dan mount, sehingga FUSE salah parsing.

```
./fuse -o nonempty encrypted_storage fuse_mount
realpath encrypted_storage: No such file or directory
```

Solusi: Flag `-o` harus di akhir:
```bash
# SALAH
./fuse -o nonempty encrypted_storage fuse_mount

# BENAR
./fuse encrypted_storage fuse_mount -o nonempty
```

**Error 3 — `allow_other only allowed if user_allow_other is set`**

```
fusermount: option allow_other only allowed if 'user_allow_other'
is set in /etc/fuse.conf
```

Solusi:
```bash
sudo nano /etc/fuse.conf
# Uncomment baris: user_allow_other
# Pastikan ada di baris sendiri tanpa komentar di baris yang sama
```

**Error 4 — `bind: Address already in use`**

Port 9000 sudah dipakai proses lain.

```
./server
bind: Address already in use
```

Solusi:
```bash
sudo kill -9 $(sudo lsof -t -i :9000)
```

**Error 5 — `ERROR: Database not found` setelah `CREATE DATABASE`**

Server binary hardcode path `/app/db` yang tidak ada di host.

```
db > CREATE DATABASE tests
DATABASE CREATED
db > CREATE TABLE tests users email password
ERROR: Database not found
```

Solusi:
```bash
sudo mkdir -p /app/db
sudo mount --bind $(pwd)/fuse_mount /app/db
```

---

## Soal 3: LibraryIT

### Deskripsi Soal 3

Membangun infrastruktur perpustakaan digital IT Library Nusantara menggunakan Docker dan Samba.

**Poin A** — Container `libraryit-server` dengan Samba. Otomatis membuat:
- 4 folder koleksi: `ebooks`, `papers`, `sourcecode`, `docs` di `/libraryit/`
- 3 user: `member` (pw: member123), `contributor` (pw: contrib456), `librarian` (pw: lib789)
- 2 group: `readonly` (berisi member), `staff` (berisi contributor + librarian)

**Poin B** — Aturan akses per koleksi:
- `ebooks` & `papers`: staff = RW, readonly = R only
- `sourcecode`: **staff = RW, readonly = tidak bisa akses dan tidak kelihatan di list**
- `docs`: semua bisa baca, **hanya librarian yang bisa tulis**

**Poin C** — Semua koleksi persistent (bind mount ke host). `sourcecode` permission 750 di host. `docs` read-only dari host.

**Poin D** — Log aktivitas format `[YYYY-MM-DD HH:MM:SS] [LEVEL] [USERNAME] [AKSI] [NAMA FILE/SHARE]`. Service terpisah `libraryit-logger` memonitor log real-time via `docker logs`.

---

### Penjelasan Kode Soal 3

#### `smb.conf` — Konfigurasi Samba

```ini
[global]
   workgroup = WORKGROUP
   server string = LibraryIT Server
   security = user
   map to guest = never
   log level = 3
   log file = /var/log/samba/samba.log

[ebooks]
   path = /libraryit/ebooks
   valid users = @staff, @readonly
   write list = @staff
   browseable = yes
   guest ok = no

[sourcecode]
   path = /libraryit/sourcecode
   valid users = @staff
   write list = @staff
   browseable = no    # ← tidak muncul di list share untuk readonly
   guest ok = no

[docs]
   path = /libraryit/docs
   valid users = @staff, @readonly
   read only = yes
   write list = librarian    # ← hanya librarian, bukan @staff
   browseable = yes
   guest ok = no
```

Kunci poin B:
- `browseable = no` pada `[sourcecode]` → tidak muncul di `smbclient -L` untuk readonly
- `write list = librarian` pada `[docs]` → contributor yang bagian staff tetap tidak bisa tulis

#### `entrypoint.sh` — Auto-setup saat Container Start

```bash
# Buat group dengan GID spesifik
groupadd -g 50 staff    2>/dev/null || true
groupadd -g 51 readonly 2>/dev/null || true

# Buat user sistem
useradd -M -s /sbin/nologin -u 1000 -g readonly member
useradd -M -s /sbin/nologin -u 1001 -g staff    contributor
useradd -M -s /sbin/nologin -u 1002 -g staff    librarian

# Daftarkan ke Samba
(echo "member123";  echo "member123")  | smbpasswd -a -s member
(echo "contrib456"; echo "contrib456") | smbpasswd -a -s contributor
(echo "lib789";     echo "lib789")     | smbpasswd -a -s librarian

# Set permission folder
chown root:staff /libraryit/sourcecode && chmod 750 /libraryit/sourcecode
chown root:staff /libraryit/docs       && chmod 775 /libraryit/docs

# Jalankan Samba
exec smbd --foreground --no-process-group --configfile=/etc/samba/smb.conf
```

#### `docker-compose.yml` — Dua Service

```yaml
services:
  libraryit-server:
    build: .
    container_name: libraryit-server
    ports:
      - "1445:445"
    volumes:
      - ./data/ebooks:/libraryit/ebooks
      - ./data/papers:/libraryit/papers
      - ./data/sourcecode:/libraryit/sourcecode
      - ./data/docs:/libraryit/docs
      - ./logs:/libraryit/logs

  libraryit-logger:
    image: ubuntu:latest
    container_name: libraryit-logger
    depends_on:
      - libraryit-server
    volumes:
      - ./logs:/libraryit/logs
    command: >
      bash -c "tail -f /libraryit/logs/libraryit.log"
```

---

### Cara Menjalankan Soal 3

```bash
cd ~/SISOP-4-2026-IT-068/soal_3

# Set permission host (poin C)
chmod 750 data/sourcecode
chmod 555 data/docs

# Build dan jalankan
sudo docker-compose up -d --build

# Verifikasi container jalan
sudo docker ps -a
```

---

### Output dan Hasil Soal 3

**Test Poin A — Verifikasi user, group, folder:**
```bash
sudo docker exec -it libraryit-server pdbedit -L
```
Expected:
```
member:1000:
contributor:1001:
librarian:1002:
```

```bash
sudo docker exec -it libraryit-server getent group staff readonly
```
Expected:
```
staff:x:50:contributor,librarian
readonly:x:51:member
```

```bash
sudo docker exec -it libraryit-server ls /libraryit/
```
Expected:
```
docs  ebooks  logs  papers  sourcecode
```

**Test Poin B — Akses per role:**

Member list share → `sourcecode` tidak muncul:
```bash
smbclient -L //localhost -p 1445 -U member%member123
```
Expected:
```
Sharename       Type      Comment
ebooks          Disk
papers          Disk
docs            Disk
IPC$            IPC       IPC Service (LibraryIT Server)
```

Member akses sourcecode → denied:
```bash
smbclient //localhost/sourcecode -p 1445 -U member%member123
# Output: tree connect failed: NT_STATUS_ACCESS_DENIED
```

Contributor tulis docs → denied:
```bash
smbclient //localhost/docs -p 1445 -U contributor%contrib456 \
  -c "put /tmp/test.txt test.txt"
# Output: NT_STATUS_ACCESS_DENIED opening remote file \test.txt
```

Librarian tulis docs → berhasil:
```bash
smbclient //localhost/docs -p 1445 -U librarian%lib789 \
  -c "put /tmp/test.txt test.txt"
# Output: putting file /tmp/test.txt as \test.txt
```

**Test Poin C — Permission host:**
```bash
ls -ld data/sourcecode
# Output: drwxr-x--- 2 root staff 4096 ... data/sourcecode

touch ./data/docs/test_dari_host.txt
# Output: touch: cannot touch './data/docs/test_dari_host.txt': Permission denied
```

**Test Poin D — Log real-time (2 terminal):**

Terminal 1:
```bash
sudo docker logs -f libraryit-logger
```

Terminal 2 (trigger aktivitas):
```bash
smbclient //localhost/sourcecode -p 1445 -U member%member123
smbclient //localhost/docs -p 1445 -U librarian%lib789 \
  -c "put /tmp/test.txt report.txt"
```

Terminal 1 expected:
```
[2026-05-13 10:01:22] [WARNING] [member] [DENIED] [sourcecode]
[2026-05-13 10:02:45] [INFO] [librarian] [WRITE] [report.txt]
```

```bash
cat logs/libraryit.log  # isi sama dengan docker logs
```

---

### Error dan Solusi Soal 3

**Error 1 — `docker-compose` tidak kompatibel dengan Docker versi baru**

```
docker.errors.DockerException: Error while fetching server API version:
Not supported URL scheme http+docker
```

Penyebab: `docker-compose` versi 1.29.2 (Python) tidak kompatibel dengan Docker engine versi baru.

Solusi: Selalu pakai `sudo`:
```bash
sudo service docker start
sudo docker-compose up -d --build
```

**Error 2 — `libraryit-server` terus Restarting**

```
libraryit-server   Restarting (1) 8 seconds ago
```

Penyebab: Typo di `entrypoint.sh` atau `smb.conf` menyebabkan Samba gagal start.

Diagnosa dan solusi:
```bash
sudo docker logs libraryit-server
# Baca error yang muncul

# Kalau typo di smb.conf (misal smb.confx):
sed -i 's/smb.confx/smb.conf/' entrypoint.sh
sudo docker-compose down
sudo docker-compose up -d --build
```

**Error 3 — `sourcecode` masih muncul di list share untuk member**

Penyebab: Parameter `browseable = no` belum ada atau salah di `smb.conf`.

Solusi: Pastikan di blok `[sourcecode]`:
```ini
[sourcecode]
   browseable = no
```
Lalu rebuild:
```bash
sudo docker-compose down
sudo docker-compose up -d --build
```

**Error 4 — Log tidak muncul di `docker logs libraryit-logger`**

Penyebab: Log level Samba terlalu rendah (default 0) sehingga tidak mencatat aktivitas.

Solusi: Tambahkan di `smb.conf` bagian `[global]`:
```ini
log level = 3
```
Rebuild container.

**Error 5 — `Permission Denied` saat `touch data/sourcecode`**

Penyebab: Permission 750 di host dikontrol oleh root (Docker), bukan user lokal.

Ini adalah behavior yang benar sesuai poin C soal. Untuk keperluan Git, gunakan `.gitkeep`:
```bash
sudo touch data/sourcecode/.gitkeep
git add data/sourcecode/.gitkeep
```

**Error 6 — Contributor bisa tulis docs (seharusnya tidak bisa)**

Penyebab: `write list = @staff` dipakai alih-alih `write list = librarian`.

Solusi: Di `smb.conf` blok `[docs]`:
```ini
[docs]
   read only = yes
   write list = librarian   # hanya librarian, bukan @staff
```

---

*Laporan ini mencakup implementasi FUSE (Filesystem in Userspace), enkripsi XOR, Docker containerization, bind mount, dan Samba file sharing dalam bahasa C dan Linux Ubuntu (WSL) untuk Praktikum Sistem Operasi Modul 4.*
