FROM ubuntu:latest

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    samba \
    samba-common-bin \
    && rm -rf /var/lib/apt/lists/*

# Copy konfigurasi
COPY smb.conf /etc/samba/smb.conf
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

# Port Samba
EXPOSE 445 139

ENTRYPOINT ["/entrypoint.sh"]
