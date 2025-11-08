#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

void error_handling(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_in serv_adr;

    if (argc != 3) {
        printf("Usage: %s <IP> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // 소켓 생성
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        error_handling("socket() error");

    // 서버 주소 설정
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = inet_addr(argv[1]); // 입력받은 IP
    serv_adr.sin_port = htons(atoi(argv[2]));      // 입력받은 포트

    // 서버에 연결 요청
    if (connect(sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("connect() error");

    //연결 성공
    printf("Connected to server %s:%s\n", argv[1], argv[2]);

    // 이후에는 여기서 read, write 등을 사용하여 서버와 통신
    char buffer[30];
    ssize_t str_len = read(sock, buffer, sizeof(buffer) - 1);
    if (str_len == -1)
        error_handling("read() error");
    buffer[str_len] = '\0';
    printf("Message from server: %s\n", buffer);

    char message[] = "Hello, Server!";
    write(sock, message, sizeof(message));


    close(sock);
    return 0;
}