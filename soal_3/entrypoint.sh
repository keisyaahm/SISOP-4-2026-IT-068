#!/bin/bash
set -e

userdel -r ubuntu 2>/dev/null || true
groupdel ubuntu 2>/dev/null || true

#1. Buat group
groupadd -g 50 staff    2>/dev/null || true
groupadd -g 1000 readonly 2>/dev/null || true  # akan dipakai member

#2. Buat user Linux
# member — group readonly (gid 1000)
useradd -M -s /sbin/nologin -u 1000 -g 1000 member       2>/dev/null || true

# contributor — group staff
useradd -M -s /sbin/nologin -u 1001 -g 50 contributor    2>/dev/null || true

# librarian — group staff
useradd -M -s /sbin/nologin -u 1002 -g 50 librarian      2>/dev/null || true

# Tambahkan ke group tambahan
usermod -aG readonly member       2>/dev/null || true
usermod -aG staff contributor     2>/dev/null || true
usermod -aG staff librarian       2>/dev/null || true

#3. Buat user Samba
(echo "member123"; echo "member123")   | smbpasswd -a -s member
(echo "contrib456"; echo "contrib456") | smbpasswd -a -s contributor
(echo "lib789";     echo "lib789")     | smbpasswd -a -s librarian

smbpasswd -e member
smbpasswd -e contributor
smbpasswd -e librarian

#4. Buat dan set permission folder
mkdir -p /libraryit/ebooks /libraryit/papers \
         /libraryit/sourcecode /libraryit/docs

# ebooks dan papers, staff RW, readonly R
chown root:staff /libraryit/ebooks /libraryit/papers
chmod 775 /libraryit/ebooks /libraryit/papers

# sourcecode, staff only (750)
chown root:staff /libraryit/sourcecode
chmod 750 /libraryit/sourcecode

# docs, librarian bisa tulis lewat samba, host read-only
chown librarian:staff /libraryit/docs
chmod 775 /libraryit/docs

#5. Pastikan log dir ada sesuai revisi
mkdir -p /logs
touch /logs/samba_raw.log
touch /logs/libraryit.log
chmod 666 /logs/samba_raw.log /logs/libraryit.log

#6. Jalankan Samba (foreground)
exec smbd --foreground --no-process-group --configfile=/etc/samba/smb.conf

