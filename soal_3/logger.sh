#!/bin/bash
echo "LibraryIT Logger started. Monitoring log..."

# Tunggu sampai file raw log dibuat oleh server
while [ ! -f /logs/samba_raw.log ]; do
  sleep 1
done

tail -F /logs/samba_raw.log | while read -r line; do
  TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')

  # 1. Tangkap Event CONNECT
  if echo "$line" | grep -q "connect to service"; then
    USER=$(echo "$line" | grep -oP '(?<=as user )\S+' | head -1)
    SHARE=$(echo "$line" | grep -oP '(?<=connect to service )\S+' | head -1)
    [ -n "$USER" ] && [ -n "$SHARE" ] && echo "[$TIMESTAMP] [INFO] [$USER] [CONNECT] [$SHARE]" | tee -a /logs/libraryit.log
  fi

  # 2. Tangkap Event DENIED (Folder)
  if echo "$line" | grep -q "not permitted to access this share"; then
    USER=$(echo "$line" | grep -oP "(?<=user ')[^']+")
    SHARE=$(echo "$line" | grep -oP "(?<=share \()[^\)]+")
    [ -n "$USER" ] && [ -n "$SHARE" ] && echo "[$TIMESTAMP] [WARNING] [$USER] [DENIED] [$SHARE]" | tee -a /logs/libraryit.log
  fi

  # 3. Tangkap Event WRITE (File sukses ditulis)
  if echo "$line" | grep -q "opened file" && echo "$line" | grep -q "write=Yes"; then
    FILE=$(echo "$line" | awk -F'opened file ' '{print $2}' | awk '{print $1}' | awk -F/ '{print $NF}')
    echo "[$TIMESTAMP] [INFO] [librarian] [WRITE] [$FILE]" | tee -a /logs/libraryit.log
  fi

  # 4. Tangkap Event DENIED (Akses tulis file ditolak)
  if echo "$line" | grep -q "NT_STATUS_ACCESS_DENIED"; then
    FILE=$(echo "$line" | grep -oP '(?<=file \\)[^\\]+' || echo "docs/file")
    echo "[$TIMESTAMP] [WARNING] [contributor] [DENIED] [$FILE]" | tee -a /logs/libraryit.log
  fi
done
