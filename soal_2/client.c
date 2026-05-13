#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP   "127.0.0.1"
#define SERVER_PORT 9000
#define BUFFER_SIZE 4096

int main() {
    int sock;
    struct sockaddr_in serv_addr;
    char send_buf[BUFFER_SIZE];
    char recv_buf[BUFFER_SIZE];

    // Buat socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("inet_pton");
        return 1;
    }

    // Connect
    if (connect(sock, (struct sockaddr *)&serv_addr,
                sizeof(serv_addr)) < 0) {
        perror("connect");
        return 1;
    }

    printf("Connected to DB Server on port %d\n", SERVER_PORT);
    printf("Type HELP for available commands\n");
    printf("Type EXIT to quit\n\n");

    while (1) {
        printf("db > ");
        fflush(stdout);

        // Baca input dari user
        if (fgets(send_buf, sizeof(send_buf), stdin) == NULL) break;

        // Hapus newline
        size_t len = strlen(send_buf);
        if (len > 0 && send_buf[len - 1] == '\n')
            send_buf[len - 1] = '\0';

        // Keluar kalau EXIT
        if (strcmp(send_buf, "EXIT") == 0 ||
            strcmp(send_buf, "exit") == 0) {
            printf("Disconnected.\n");
            break;
        }

        // Tambahkan newline sebelum kirim (server butuh ini sebagai terminator)
        strcat(send_buf, "\n");
        if (send(sock, send_buf, strlen(send_buf), 0) < 0) {
            perror("send");
            break;
        }

        // Terima dan tampilkan respons
        // Loop recv sampai dapat data penuh
        printf("\n");
        int total = 0;
        while (1) {
            int n = recv(sock, recv_buf, sizeof(recv_buf) - 1, 0);
            if (n <= 0) break;
            recv_buf[n] = '\0';
            printf("%s", recv_buf);
            total += n;

            // Heuristik: kalau data < buffer size, kemungkinan sudah selesai
            if (n < (int)(sizeof(recv_buf) - 1)) break;
        }
        printf("\n");
    }

    close(sock);
    return 0;
}
