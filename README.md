# Laporan Praktikum Sistem Operasi 2026 - Modul 5
## OS Building dan Bootloaders

**Nama:** Keisya Halimah Mulia
**NRP:** 5027251068
**Kelas:** A / Teknologi Informasi

---

## Daftar Isi

- [Soal 1 - Farewell Party](#soal-1---farewell-party)
  - [Struktur Repository Soal 1](#struktur-repository-soal-1)
  - [Penjelasan File Soal 1](#penjelasan-file-soal-1)
  - [Poin 2 kernel.sh](#poin-2-kernelsh)
  - [Poin 3 single.sh](#poin-3-singlesh)
  - [Poin 4 multi.sh](#poin-4-multish)
  - [Poin 5 iso.sh](#poin-5-isosh)
  - [Poin 6 qemu.sh](#poin-6-qemush)
  - [Poin 7 backup.sh](#poin-7-backupsh)
  - [Poin 8 Internet Access](#poin-8-internet-access)
  - [Poin 9 Package Manager party](#poin-9-package-manager-party)
  - [Poin 10 FUSE](#poin-10-fuse)
  - [Kendala dan Error Soal 1](#kendala-dan-error-soal-1)
- [Soal 2 - Season](#soal-2---season)
  - [Struktur Repository Soal 2](#struktur-repository-soal-2)
  - [Penjelasan File Soal 2](#penjelasan-file-soal-2)
    - [bochsrc.txt](#bochsrctxt)
    - [Makefile](#makefile)
    - [bootloader.asm](#bootloaderasm)
    - [kernel.asm Poin 1](#kernelasm-poin-1)
    - [kernel.c Poin 2-8](#kernelc-poin-2-8)
  - [Run dan Hasil Soal 2](#run-dan-hasil-soal-2)
    - [Poin 1 _getChar](#poin-1-_getchar)
    - [Poin 2 check](#poin-2-check)
    - [Poin 3 add](#poin-3-add)
    - [Poin 4 sub](#poin-4-sub)
    - [Poin 5 fac](#poin-5-fac)
    - [Poin 6 season](#poin-6-season)
    - [Poin 7 triangle](#poin-7-triangle)
    - [Poin 8 clear dan help](#poin-8-clear-dan-help)
  - [Kendala dan Error Soal 2](#kendala-dan-error-soal-2)

---

# Soal 1 - Farewell Party

## Struktur Repository Soal 1

```
soal_1/
├── .config              # Konfigurasi kernel Linux 6.1.1
├── .gitignore           # File dan folder yang tidak di-push ke GitHub
├── backup.sh            # Script untuk mengarsip hasil build
├── iso.sh               # Script untuk membuat bootable ISO
├── kernel.sh            # Script untuk download dan compile kernel Linux
├── multi.sh             # Script untuk membangun multi-user filesystem
├── osboot/              # Folder output hasil build
│   ├── .gitkeep
│   ├── bzImage          # Kernel hasil compile (di-ignore git)
│   ├── single.gz        # Single-user initramfs (di-ignore git)
│   ├── multi.gz         # Multi-user initramfs (di-ignore git)
│   ├── farewell.iso     # Bootable ISO (di-ignore git)
│   └── farewell_backup_[timestamp].zip
├── qemu.sh              # Script untuk menjalankan OS via QEMU
└── single.sh            # Script untuk membangun single-user filesystem
```

![Struktur Repo](asset/struk1.png)

---

## Penjelasan File Soal 1

### `.config`

File konfigurasi kernel Linux 6.1.1 yang digunakan saat compile. Dihasilkan dari `make defconfig` lalu dimodifikasi untuk mengaktifkan modul-modul wajib:

- `CONFIG_BLK_DEV_INITRD` - dukungan initramfs
- `CONFIG_DEVTMPFS` dan `CONFIG_DEVTMPFS_MOUNT` - auto-mount `/dev`
- `CONFIG_FUSE_FS` - dukungan FUSE (untuk poin 10)
- `CONFIG_SERIAL_8250` dan `CONFIG_SERIAL_8250_CONSOLE` - serial console (`ttyS0`)
- `CONFIG_BINFMT_ELF` dan `CONFIG_BINFMT_SCRIPT` - dukungan eksekusi binary dan script
- `CONFIG_PRINTK` - dukungan kernel log output

---

## Poin 2 kernel.sh

**Fungsi:** Download dan compile Linux kernel 6.1.1, output: `osboot/bzImage`

```bash
#!/bin/bash
echo "=== STEP 1: DOWNLOADING & COMPILING KERNEL ==="
mkdir -p osboot

if [ ! -f "linux-6.1.1.tar.xz" ]; then
    wget -nc https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.1.1.tar.xz
fi

rm -rf linux-6.1.1
tar -xf linux-6.1.1.tar.xz
cd linux-6.1.1

make defconfig
scripts/config --enable CONFIG_BLK_DEV_INITRD
scripts/config --enable CONFIG_DEVTMPFS
scripts/config --enable CONFIG_DEVTMPFS_MOUNT
scripts/config --enable CONFIG_FUSE_FS
scripts/config --enable CONFIG_BINFMT_ELF
scripts/config --enable CONFIG_BINFMT_SCRIPT
scripts/config --enable CONFIG_SERIAL_8250
scripts/config --enable CONFIG_SERIAL_8250_CONSOLE
scripts/config --disable SYSTEM_TRUSTED_KEYS
scripts/config --disable SYSTEM_REVOCATION_KEYS
scripts/config --disable DEBUG_INFO_BTF
scripts/config --disable WERROR

make olddefconfig
make -j$(nproc)

cp arch/x86/boot/bzImage ../osboot/
cd ..
echo ">>> bzImage BERHASIL DIBUAT! <<<"
```

**Cara menjalankan:**

```bash
cd ~/SISOP-5-2026-IT-068/soal_1
./kernel.sh
```

**Verifikasi:**

```bash
ls -lh osboot/bzImage
```

![kernel run](asset/kernel.png)

![bzimage](asset/bzimage.png)

---

## Poin 3 single.sh

**Fungsi:** Membangun single-user initramfs menggunakan BusyBox, output: `osboot/single.gz`

**Spesifikasi yang dipenuhi:**
- User: `root` (hanya root)
- Direktori: `bin/`, `dev/`, `proc/`, `sys/`, `etc/`, `tmp/`, `root/`
- Access: root bisa akses apapun

**Komponen utama:**
- BusyBox - menyediakan semua utilitas Unix dasar (`ls`, `sh`, `mount`, `ifconfig`, dll)
- `party` (apk-tools-static dari Alpine Linux) - package manager yang di-rename
- `/etc/passwd` - berisi data user root agar `whoami` bisa berjalan
- init script - mount `/proc`, `/sys`, `/dev`, setup jaringan static (10.0.2.15 via QEMU user network), lalu `exec /bin/sh`

**Cara menjalankan:**

```bash
cd ~/SISOP-5-2026-IT-068/soal_1
./single.sh
./qemu.sh --single
```

**Di dalam QEMU:**

```sh
whoami
ls /
ls /bin | head -20
```

![single](asset/single.png)

![single qemu](asset/single2.png)

![single qemu 3](asset/single3.png)

---

## Poin 4 multi.sh

**Fungsi:** Membangun multi-user initramfs, output: `osboot/multi.gz`

**User dan Password:**

| User | Password | UID | Home |
|------|----------|-----|------|
| root | root123 | 0 | /root |
| henn | henn123 | 1001 | /home/henn |
| hann | hann123 | 1002 | /home/hann |
| viii | viii123 | 1003 | /home/viii |
| kids | kids123 | 1004 | /home/kids |

**Implementasi access control via file permission dan group:**

| Direktori | Permission | Owner | Keterangan |
|-----------|-----------|-------|------------|
| `/root` | 700 | root:root | Hanya root |
| `/home/henn` | 700 | 1001:1001 | Hanya henn |
| `/home/hann` | 750 | 1002:1002 | hann dan henn (henn ada di group hann) |
| `/home/viii` | 750 | 1003:1003 | viii, hann, henn (keduanya ada di group viii) |
| `/home/kids` | 750 | 1004:1004 | kids, viii, hann, henn (semua ada di group kids) |
| `/tmp` | 1777 | - | Full akses semua user |

**Tabel access control:**

| User | Bisa akses | Tidak bisa akses |
|------|-----------|-----------------|
| root | Semua direktori | - |
| henn | `/home/*` semua | `/root` |
| hann | `/home/{hann,viii,kids}` | `/root`, `/home/henn` |
| viii | `/home/{viii,kids}` | `/root`, `/home/{henn,hann}` |
| kids | `/home/kids` saja | `/root`, `/home/{henn,hann,viii}` |

**Banner** - `/etc/profile` dijalankan saat login, menampilkan ASCII art "Farewell Party" dan `Welcome, <USER>.`

**Init script** - mount filesystems, setup jaringan static, init party database, lalu `getty` pada `ttyS0` untuk login prompt.

**Cara menjalankan:**

```bash
cd ~/SISOP-5-2026-IT-068/soal_1
./multi.sh
./qemu.sh --multi
```

**Test login root:**

```
Login: root
Password: root123
```

```sh
whoami
ls /root
ls /home
```

![multi](asset/multi.png)

![multi qemu](asset/multi2.png)

![multi root](asset/multiroot.png)

**Test login henn:**

```
Login: henn
Password: henn123
```

```sh
ls /home/henn    # bisa
ls /home/hann    # bisa (henn full /home/*)
ls /home/viii    # bisa
ls /root         # Permission denied
```

![multi henn](asset/multihenn.png)

**Test login hann:**

```
Login: hann
Password: hann123
```

```sh
ls /home/hann    # bisa
ls /home/viii    # bisa
ls /home/kids    # bisa
ls /home/henn    # Permission denied
ls /root         # Permission denied
```

![multi hann](asset/multihann.png)

**Test login viii:**

```
Login: viii
Password: viii123
```

```sh
ls /home/viii    # bisa
ls /home/kids    # bisa
ls /home/henn    # Permission denied
ls /home/hann    # Permission denied
ls /root         # Permission denied
```

![multi viii](asset/multiviii.png)

**Test login kids:**

```
Login: kids
Password: kids123
```

```sh
ls /home/kids    # bisa
ls /home/henn    # Permission denied
ls /home/hann    # Permission denied
ls /home/viii    # Permission denied
ls /root         # Permission denied
ls /tmp          # bisa (full access tmp semua user)
```

![multi kids](asset/multikids.png)

---

## Poin 5 iso.sh

**Fungsi:** Membuat bootable ISO dengan GRUB yang memuat kedua filesystem, output: `osboot/farewell.iso`

Script ini:
1. Membuat struktur direktori ISO: `osboot/isodir/boot/grub/`
2. Menyalin `bzImage`, `single.gz`, `multi.gz` ke dalam struktur ISO
3. Membuat `grub.cfg` dengan dua menu entry: Multi User dan Single User
4. Menjalankan `grub-mkrescue` untuk menghasilkan file `.iso`
5. Membersihkan direktori sementara

**grub.cfg:**

```
set timeout=10
set default=0
menuentry "Farewell Party - Multi User" {
    linux  /boot/bzImage console=ttyS0 nomodeset
    initrd /boot/multi.gz
}
menuentry "Farewell Party - Single User" {
    linux  /boot/bzImage console=ttyS0 nomodeset
    initrd /boot/single.gz
}
```

**Cara menjalankan:**

```bash
cd ~/SISOP-5-2026-IT-068/soal_1
./iso.sh
ls -lh osboot/farewell.iso
```

![farewell iso](asset/iso.png)

![ukuran iso](asset/ukiso.png)

---

## Poin 6 qemu.sh

**Fungsi:** Menjalankan OS hasil build dengan 3 mode

```bash
#!/bin/bash
NET_FLAGS="-netdev user,id=net0 -device e1000,netdev=net0"
QEMU_CMD="qemu-system-x86_64 -smp 2 -m 512 -nographic"

case "$1" in
    --single)
        $QEMU_CMD -kernel osboot/bzImage -initrd osboot/single.gz \
                  -append "console=ttyS0 rdinit=/init" $NET_FLAGS ;;
    --multi)
        $QEMU_CMD -kernel osboot/bzImage -initrd osboot/multi.gz \
                  -append "console=ttyS0 rdinit=/init" $NET_FLAGS ;;
    --all)
        $QEMU_CMD -cdrom osboot/farewell.iso $NET_FLAGS ;;
    *)
        echo "Usage: ./qemu.sh [--single|--multi|--all]"
        exit 1 ;;
esac
```

| Flag QEMU | Keterangan |
|-----------|-----------|
| `-smp 2` | 2 virtual CPU |
| `-m 512` | RAM 512 MB |
| `-nographic` | Output ke terminal tanpa window GUI |
| `-netdev user` dan `-device e1000` | Jaringan virtual QEMU (NAT) |
| `console=ttyS0` | Output kernel ke serial port ttyS0 |
| `rdinit=/init` | Program pertama yang dijalankan kernel |

**Cara menjalankan:**

```bash
# Boot single user langsung
./qemu.sh --single

# Boot multi user langsung
./qemu.sh --multi

# Boot dari ISO dengan menu GRUB
./qemu.sh --all
```

Keluar dari QEMU: `Ctrl+A` lalu `X`

![poin 6 single](asset/6single.png)

![poin 6 multi](asset/6multi.png)

![all multi](asset/allmulti.png)

![masuk all multi](asset/mskallmulti.png)

---

## Poin 7 backup.sh

**Fungsi:** Mengarsip semua file build ke dalam satu zip, lalu menghapus file aslinya

```bash
#!/bin/bash
TIMESTAMP=$(date +"%d%m%Y-%H%M%S")
ZIPNAME="osboot/farewell_backup_${TIMESTAMP}.zip"

zip -j "$ZIPNAME" osboot/bzImage osboot/single.gz osboot/multi.gz osboot/farewell.iso
rm -f osboot/bzImage osboot/single.gz osboot/multi.gz osboot/farewell.iso
echo ">>> Backup selesai: $ZIPNAME <<<"
```

Format nama file: `farewell_backup_[DDMMYYYY-HHMMSS].zip`

Flag `-j` pada zip berarti junk paths - simpan hanya file tanpa path lengkapnya.

**Cara menjalankan:**

```bash
cd ~/SISOP-5-2026-IT-068/soal_1
./backup.sh
ls -lh osboot/
```

![backup](asset/backup.png)

![isi backup](asset/isibackup.png)

---

## Poin 8 Internet Access

**Cara menjalankan:**

```bash
./qemu.sh --single
```

**Di dalam QEMU:**

```sh
ping 8.8.8.8 -c 4
wget example.com -O /dev/null
```

Catatan: Ping ke 8.8.8.8 menggunakan ICMP yang dibatasi oleh QEMU user networking di WSL2. Namun wget (TCP) berhasil, yang membuktikan koneksi internet berfungsi.

![internet access](asset/accessi.png)

---

## Poin 9 Package Manager party

`party` adalah binary `apk.static` dari Alpine Linux v3.18 yang diunduh saat build dan di-rename menjadi `party`. Ini adalah package manager yang fully static sehingga dapat berjalan di dalam initramfs minimal tanpa library tambahan.

**Cara menjalankan:**

```bash
./qemu.sh --single
```

**Di dalam QEMU:**

```sh
party --version
party update --allow-untrusted --no-cache
party add --allow-untrusted --no-cache busybox
```

![party](asset/party.png)

---

## Poin 10 FUSE

**Di dalam QEMU single user:**

```sh
# Install FUSE via party
party add --allow-untrusted --no-cache fuse

# Verifikasi
ls /bin/fusermount

# Buat program FUSE sederhana
cat > /tmp/hello_fuse.c << 'EOF'
#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <string.h>
#include <errno.h>

static int hello_getattr(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else if (strcmp(path, "/hello") == 0) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = 13;
    } else {
        return -ENOENT;
    }
    return 0;
}

static int hello_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi) {
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    filler(buf, "hello", NULL, 0);
    return 0;
}

static int hello_read(const char *path, char *buf, size_t size,
                      off_t offset, struct fuse_file_info *fi) {
    const char *content = "Hello, FUSE!\n";
    size_t len = 13;
    if (offset >= len) return 0;
    if (offset + size > len) size = len - offset;
    memcpy(buf, content + offset, size);
    return size;
}

static struct fuse_operations hello_oper = {
    .getattr = hello_getattr,
    .readdir = hello_readdir,
    .read    = hello_read,
};

int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &hello_oper, NULL);
}
EOF

gcc /tmp/hello_fuse.c -o /tmp/hello_fuse $(pkg-config fuse --cflags --libs)
mkdir -p /tmp/mnt
/tmp/hello_fuse /tmp/mnt &
sleep 1
cat /tmp/mnt/hello
fusermount -u /tmp/mnt
```

**Ekspektasi output:**

```
Hello, FUSE!
```

![fuse](asset/fuse.png)

---

## Kendala dan Error Soal 1

### 1. bzImage Terlalu Kecil (1.4MB)

**Masalah:** Kernel dikompilasi dengan config yang tidak lengkap sehingga menghasilkan bzImage hanya 1.4MB dan tidak bisa boot initramfs.

**Solusi:** Mengaktifkan `CONFIG_BLK_DEV_INITRD`, `CONFIG_PRINTK`, dan `CONFIG_SERIAL_8250_CONSOLE` via `scripts/config --enable` sebelum `make olddefconfig`.

### 2. QEMU Layar Kosong

**Masalah:** Setelah boot, terminal QEMU kosong tanpa output apapun.

**Solusi:** `CONFIG_PRINTK` tidak aktif di `.config`. Setelah diaktifkan dan kernel dikompilasi ulang, output muncul di terminal via `console=ttyS0`.

### 3. VFS: Unable to mount root fs

**Masalah:** Kernel panic karena tidak bisa mount initramfs.

**Solusi:** `CONFIG_BLK_DEV_INITRD` tidak aktif. Setelah diaktifkan dan kernel dikompilasi ulang, initramfs berhasil dimount.

### 4. Device Files Tidak Bisa Di-copy di WSL

**Masalah:** `cp -a /dev/null` dan perintah serupa gagal dengan error `Operation not permitted` di WSL.

**Solusi:** Menggunakan `CONFIG_DEVTMPFS_MOUNT=y` di kernel config sehingga kernel otomatis populate `/dev` saat boot tanpa perlu copy device files ke initramfs secara manual.

### 5. `party database` Error

**Masalah:** `party update` gagal dengan `Failed to open apk database: No such file or directory`.

**Solusi:** Membuat direktori `/lib/apk/db`, `/var/cache/apk`, dan file `/etc/apk/repositories` di dalam init script sebelum menjalankan `party update`.

### 6. Access Control kids Bisa Akses Folder Lain

**Masalah:** User `kids` bisa mengakses `/home/hann` dan `/home/viii` yang seharusnya tidak bisa.

**Solusi:** Proses `cpio` harus dijalankan dengan `sudo` agar ownership yang di-set lewat `sudo chown` tersimpan dengan benar ke dalam arsip initramfs.

### 7. Ping 8.8.8.8 Gagal

**Masalah:** `ping 8.8.8.8` gagal dengan "Network is unreachable" meskipun `wget` berhasil.

**Solusi:** QEMU user networking membatasi ICMP (ping) di WSL2. Koneksi internet tetap berfungsi via TCP, dibuktikan dengan `wget example.com` yang berhasil (HTTP 200 OK).

---

# Soal 2 - Season

## Struktur Repository Soal 2

```
soal_2/
├── Makefile          # Build automation dan run via Bochs Windows
├── README.md         # Laporan resmi
├── bochsrc.txt       # Konfigurasi emulator Bochs
├── bootloader.asm    # Bootloader 16-bit (memuat kernel ke memori)
├── build.sh          # Script build alternatif
├── kernel.asm        # Kernel assembly (_putInMemory, _getChar)
└── kernel.c          # Kernel C (semua logic command)
```

---

## Penjelasan File Soal 2

### `bochsrc.txt`

Konfigurasi emulator Bochs yang menjalankan floppy disk image.

```
megs: 32
romimage: file="C:/Program Files/Bochs-3.0/BIOS-bochs-latest"
vgaromimage: file="C:/Program Files/Bochs-3.0/VGABIOS-lgpl-latest.bin"
boot: floppy
floppya: 1_44="C:/Users/ASUS/Desktop/floppy.img", status=inserted
log: bochslog.txt
mouse: enabled=0
display_library: win32
```

- `megs: 32` - alokasi RAM 32 MB untuk emulasi
- `romimage` dan `vgaromimage` - BIOS dan VGA BIOS dari instalasi Bochs Windows
- `boot: floppy` - boot dari floppy disk
- `floppya` - path ke file `floppy.img` yang akan dijalankan (disalin ke Desktop Windows)
- `display_library: win32` - tampilkan output di window Windows

---

### `Makefile`

Build automation untuk compile dan menjalankan sistem operasi.

```makefile
prepare:
	dd if=/dev/zero of=floppy.img bs=512 count=2880

bootloader:
	nasm -f bin bootloader.asm -o bootloader.bin
	dd if=bootloader.bin of=floppy.img bs=512 count=1 conv=notrunc

kernel:
	nasm -f as86 kernel.asm -o kernel-asm.o
	bcc -ansi -c kernel.c -o kernel.o
	ld86 -0 -d -o kernel.bin kernel-asm.o kernel.o
	dd if=kernel.bin of=floppy.img bs=512 seek=1 conv=notrunc

build: prepare bootloader kernel

run:
	mkdir -p "/mnt/c/Users/ASUS/Desktop"
	cp floppy.img "/mnt/c/Users/ASUS/Desktop/floppy.img"
	"/mnt/c/Program Files/Bochs-3.0/bochs.exe" -f bochsrc.txt -q
```

| Target | Perintah | Fungsi |
|--------|---------|--------|
| `prepare` | `make prepare` | Buat floppy.img kosong 1.44 MB (2880 x 512 byte) |
| `bootloader` | `make bootloader` | Compile bootloader.asm, tulis ke sektor 0 floppy |
| `kernel` | `make kernel` | Compile kernel.asm dan kernel.c, tulis ke sektor 1-15 floppy |
| `build` | `make build` | Jalankan semua: prepare + bootloader + kernel |
| `run` | `make run` | Salin floppy.img ke Desktop Windows, jalankan Bochs |

**Alasan menggunakan Bochs Windows dari WSL:** Bochs diinstall di Windows (`C:/Program Files/Bochs-3.0/`). WSL dapat menjalankan executable Windows langsung via path `/mnt/c/...`, sehingga build dilakukan di WSL (Linux tools) tapi emulasi dilakukan di Bochs Windows yang punya display.

---

### `bootloader.asm`

Bootloader 16-bit yang bertugas memuat kernel dari floppy ke memori RAM.

```asm
bits 16

KERNEL_SEGMENT equ 0x1000
KERNEL_SECTORS equ 15
KERNEL_START   equ 1
```

**Alur kerja bootloader:**
1. BIOS memuat 512 byte pertama (sektor 0) ke alamat `0x7C00` dan menjalankannya
2. Bootloader membaca 15 sektor dari floppy menggunakan BIOS interrupt `int 0x13` ke alamat `0x1000:0x0000`
3. Setup segment registers ke `0x1000` dan set stack pointer
4. Jump ke kernel di `0x1000:0x0000`
5. Byte terakhir: `times 510-($-$$) db 0` dan `dw 0xAA55` (magic number MBR)

---

### `kernel.asm` (Soal Poin 1)

Assembly kernel yang mengekspos fungsi-fungsi low-level ke kode C.

```asm
bits 16

global _start
global _putInMemory
global _getChar
extern _main
```

**`_start`** - Entry point kernel:

```asm
_start:
    cli
    mov ax, cs
    mov ds, ax
    mov es, ax
    sti
    call _main
.hang:
    jmp .hang
```

**`_putInMemory(segment, address, character)`** - Tulis karakter ke VGA memory:

```asm
_putInMemory:
    push bp
    mov bp, sp
    push ds
    mov ax, [bp+4]
    mov si, [bp+6]
    mov cl, [bp+8]
    mov ds, ax
    mov [si], cl
    pop ds
    pop bp
    ret
```

VGA text mode memetakan karakter ke `0xB8000` = `0xB000:0x8000`. Setiap sel terdiri dari 2 byte: byte karakter dan byte atribut warna.

**`_getChar()`** (implementasi soal) - Baca karakter dari keyboard:

```asm
_getChar:
    push bp
    mov bp, sp
    mov ah, 0x00
    int 0x16
    xor ah, ah
    pop bp
    ret
```

`int 0x16` (AH=00h) adalah BIOS interrupt untuk membaca keystroke. Program akan blocking sampai ada tombol ditekan. Return value di `AL` adalah kode ASCII karakter.

---

### `kernel.c` (Soal Poin 2-8)

Implementasi semua command dalam sistem operasi sederhana 16-bit.

**Helper Functions:**

`mod()` dan `div()` - Menggantikan operator `/` dan `%`:

```c
int mod(int a, int b) { while (a >= b) a -= b; return a; }
int div(int a, int b) { int q = 0; while (a >= b) { a -= b; q++; } return q; }
```

`clearScreen()` - Isi seluruh VGA buffer (80x25 = 2000 sel) dengan spasi:

```c
void clearScreen() {
    int i;
    for (i = 0; i < 2000; i++) {
        putInMemory(0xB000, 0x8000 + i * 2, ' ');
        putInMemory(0xB000, 0x8001 + i * 2, color);
    }
    cursor = 0;
}
```

`atoi(str)` - Konversi string ke integer, mendukung angka negatif:

```c
int atoi(char *str) {
    int res = 0, sign = 1;
    if (*str == '-') { sign = -1; str++; }
    while (*str >= '0' && *str <= '9') {
        res = res * 10 + (*str - '0');
        str++;
    }
    return res * sign;
}
```

`printInt(n)` - Cetak integer signed, dengan `printUInt()` untuk nilai positif:

```c
void printInt(int n) {
    if (n == 0) { printChar('0'); return; }
    if (n < 0) { printChar('-'); n = -n; }
    printUInt((unsigned int)n);
}
```

**Command Parser di `main()`:**

```c
while (cmd[ptr] != ' ' && cmd[ptr] != '\0') { command[cmd_len++] = cmd[ptr++]; }
while (cmd[ptr] != ' ' && cmd[ptr] != '\0') { arg1[arg1_len++]   = cmd[ptr++]; }
while (cmd[ptr] != ' ' && cmd[ptr] != '\0') { arg2[arg2_len++]   = cmd[ptr++]; }
```

**Tabel Implementasi Command:**

| Command | Soal | Implementasi |
|---------|------|-------------|
| `check` | Poin 2 | `strcmp(command, "check") == 0` lalu print "ok" |
| `add a b` | Poin 3 | `atoi(arg1) + atoi(arg2)` lalu `printInt()` |
| `sub a b` | Poin 4 | `atoi(arg1) - atoi(arg2)` lalu `printInt()` |
| `fac n` | Poin 5 | Loop multiply, cek `n > 7` untuk overflow |
| `season name` | Poin 6 | Set global `color`, print mode |
| `triangle n` | Poin 7 | Nested loop cetak karakter 'x' |
| `clear` | Poin 8 | `clearScreen()` |
| `help` | Poin 8 | Print list semua command |
| `about` | - | Print info sistem |

**Faktorial dan batas 16-bit (Poin 5):**

```c
if (n < 0 || n > 7) {
    printString("know your limit little bro.\n");
} else {
    res = 1;
    for (i = 1; i <= n; i++) res = res * i;
    printUInt(res);
}
```

Batas `n > 7` karena `8! = 40320 > 32767` (max signed 16-bit int). Di sistem 16-bit, overflow tidak terdeteksi otomatis, sehingga dicek secara eksplisit. `res` bertipe `unsigned int` agar `7! = 5040` tetap aman.

**Season - Warna VGA (Poin 6):**

| Season | Color Code | Warna |
|--------|-----------|-------|
| winter | `0x09` | Light Blue |
| spring | `0x0A` | Light Green |
| summer | `0x0E` | Yellow |
| fall | `0x06` | Brown/Dark Yellow |
| radiant | `0x0D` | Light Magenta |

Color byte di VGA text mode = `(background << 4) | foreground`. Perubahan `color` global akan mempengaruhi semua teks yang dicetak sesudahnya.

---

## Run dan Hasil Soal 2

### Build

```bash
cd ~/SISOP-5-2026-IT-068/soal_2
make build
ls -lh floppy.img kernel.bin bootloader.bin
```

![make build](asset/makebuild.png)

![floppy hasil build](asset/floppy.png)

### Run Bochs

```bash
make run
```

Bochs Windows akan terbuka. Karena menggunakan flag `-q`, Bochs langsung start tanpa prompt. Program langsung berjalan menampilkan welcome screen.

![run make run](asset/makerun.png)

---

## Poin 1 _getChar

Dibuktikan dengan kemampuan mengetik di shell. Semua command berikut membuktikan `_getChar` berfungsi karena tanpa fungsi ini tidak ada input yang bisa diterima dari keyboard.

---

## Poin 2 check

```
> check
ok
```

---

## Poin 3 add

```
> add 5 3
8
> add 14 2
16
```

---

## Poin 4 sub

```
> sub 10 2
8
> sub 16 2
14
```

---

## Poin 5 fac

```
> fac 6
720
> fac 120
know your limit little bro.
```

![bochs 1](asset/bochs1.png)

---

## Poin 6 season

```
> season winter
winter mode
> season spring
spring mode
> season summer
summer mode
> season fall
fall mode
> season radiant
radiant mode
```

![bochs 2](asset/bochs2.png)

---

## Poin 7 triangle

```
> triangle 5
x
xx
xxx
xxxx
xxxxx
```

![bochs 3](asset/bochs3.png)

---

## Poin 8 clear dan help

```
> help
check add sub fac season triangle clear about
> add 14 2
16
> sub 16 2
14
> clear
```

![bochs 4](asset/bochs4.png)

![bochs 5](asset/bochs5.png)

---

## Kendala dan Error Soal 2

### 1. Bochs Tidak Bisa Run dari WSL Langsung

**Masalah:** Bochs Linux di WSL crash karena masalah ALSA audio dan display.

**Error:**
```
ALSA lib pcm.c:2721: Unknown PCM default
*** buffer overflow detected ***: terminated
Aborted (core dumped)
```

**Solusi:** Menggunakan Bochs Windows (`C:/Program Files/Bochs-3.0/bochs.exe`) yang dapat dipanggil dari WSL via path `/mnt/c/...`. File `floppy.img` disalin ke Desktop Windows sebelum dijalankan.

### 2. SDL Library Tidak Tersedia

**Masalah:** `display_library: sdl` tidak tersedia di instalasi Bochs WSL.

**Error:**
```
bochsrc.txt:8: display library 'sdl' not available
```

**Solusi:** Beralih ke Bochs Windows dengan `display_library: win32` yang berfungsi di environment Windows.

### 3. Overflow Deteksi Faktorial

**Masalah:** Di sistem 16-bit, `int` hanya 16-bit signed (max 32767). `8! = 40320` melebihi batas tanpa memberikan error otomatis.

**Solusi:** Batasan eksplisit `n > 7` sebelum komputasi. Tipe `unsigned int` digunakan untuk hasil faktorial agar nilai 5040 (7!) tidak overflow.

### 4. Operator `/` dan `%` Dilarang oleh bcc

**Masalah:** Compiler `bcc` untuk 16-bit tidak mendukung operator division dan modulo secara langsung untuk tipe int.

**Solusi:** Mengimplementasikan fungsi helper `div()` dan `mod()` menggunakan pengurangan berulang, lalu menggunakan keduanya untuk `printUInt()` (ekstrak digit) dan `printChar()` (kalkulasi posisi baris baru).

### 5. Angka Negatif pada `sub` dan `atoi`

**Masalah:** Hasil `sub` bisa negatif, perlu ditangani oleh `printInt()` dan `atoi()`.

**Solusi:** `atoi()` mendeteksi `-` di awal string dan menyimpan tanda dalam variabel `sign`. `printInt()` mencetak `-` lalu memanggil `printUInt()` dengan nilai absolut.

### 6. Bochs Versi 2.7 vs 3.0

**Masalah:** Bochs versi 2.7 di WSL membutuhkan input `c + Enter` untuk mulai berjalan. Bochs Windows versi 3.0 dengan flag `-q` langsung start tanpa prompt.

**Solusi:** Menggunakan flag `-q` (quiet mode) pada Bochs Windows 3.0 sehingga Bochs langsung menjalankan program tanpa menunggu input dari user.
