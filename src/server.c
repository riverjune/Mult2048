#include "game.h"
#include "protocol.h"
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
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    socklen_t clnt_adr_sz;

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // 소켓 생성
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1)
        error_handling("socket() error");

    // 서버 주소 설정
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY); // 내 컴퓨터의 아무 IP나
    serv_adr.sin_port = htons(atoi(argv[1]));     // 입력받은 포트

    // 소켓에 주소 할당
    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");

    // 연결 대기 상태로 전환
    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    printf("Server is listening on port %s...\n", argv[1]);

    // 클라이언트 연결 수락
    clnt_adr_sz = sizeof(clnt_adr);
    clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_sz);
    if (clnt_sock == -1)
        error_handling("accept() error");

    //연결 성공
    printf("Client connected.\n");

    // 이후에는 여기서 read, write 등을 사용하여 클라이언트와 통신
    char message[] = "Hello, Client!";
    write(clnt_sock, message, sizeof(message));
    
    char buffer[30];
    ssize_t str_len = read(clnt_sock, buffer, sizeof(buffer) - 1);
    if (str_len == -1)
        error_handling("read() error");
    buffer[str_len] = '\0';
    printf("Message from client: %s\n", buffer);

    close(clnt_sock);
    close(serv_sock);
    return 0;
}