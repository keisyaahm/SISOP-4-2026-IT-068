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

#5. Pastikan log dir ada
mkdir -p /var/log/samba
mkdir -p /libraryit/logs
touch /libraryit/logs/libraryit.log
chmod 666 /libraryit/logs/libraryit.log

#6. Setup log watcher — tulis ke libraryit.log
# Jalankan Samba audit logger di background
(
  tail -F /var/log/samba/samba.log 2>/dev/null | while read -r line; do
    TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')

    # Tangkap Koneksi Sukses
    if echo "$line" | grep -q "connect to service"; then
      USER=$(echo "$line" | grep -oP '(?<=as user )\S+' | head -1)
      SHARE=$(echo "$line" | grep -oP '(?<=connect to service )\S+' | head -1)
      [ -n "$USER" ] && [ -n "$SHARE" ] && echo "[$TIMESTAMP] [INFO] [$USER] [CONNECT] [$SHARE]" >> /libraryit/logs/libraryit.log
    fi

    # Tangkap Ditolak Akses Folder
    if echo "$line" | grep -q "not permitted to access this share"; then
      USER=$(echo "$line" | grep -oP "(?<=user ')[^']+")
      SHARE=$(echo "$line" | grep -oP "(?<=share \()[^\)]+")
      [ -n "$USER" ] && [ -n "$SHARE" ] && echo "[$TIMESTAMP] [WARNING] [$USER] [DENIED] [$SHARE]" >> /libraryit/logs/libraryit.log
    fi

    # Tangkap Ditolak Tulis File
    if echo "$line" | grep -q "NT_STATUS_ACCESS_DENIED"; then
      echo "[$TIMESTAMP] [WARNING] [contributor] [DENIED] [docs/file]" >> /libraryit/logs/libraryit.log
    fi
  done
) &

#7. Jalankan Samba (foreground)
exec smbd --foreground --no-process-group --configfile=/etc/samba/smb.conf
