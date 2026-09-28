/* เปิด network — คาดหวัง SV (seccomp บล็อก socket) */
#include <arpa/inet.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
int main(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(80) };
    inet_pton(AF_INET, "1.1.1.1", &addr.sin_addr);
    connect(fd, (struct sockaddr *)&addr, sizeof addr);
    printf("connected\n");
    close(fd);
    return 0;
}
