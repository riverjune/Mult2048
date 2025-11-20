#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#include "protocol.h" // 서버와 동일한 프로토콜 사용

// ============================================
// [전역 변수]
// ============================================
int sock; // 서버와 연결된 소켓

// ============================================
// [함수 선언]
// ============================================
void *recv_msg(void *arg);
void print_game_state(S2C_Packet *packet);
void error_handling(const char *msg);

int main(int argc, char *argv[]) {
    struct sockaddr_in serv_addr;
    pthread_t snd_thread, rcv_thread;

    if (argc != 3) {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    // 1. 소켓 생성
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    // 2. 서버 연결
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error");

    printf("Connected to Game Server!\n");
    printf("Controls: w(UP), s(DOWN), a(LEFT), d(RIGHT), q(QUIT)\n");

    // 3. 수신 전담 스레드 생성 (서버가 보내는 보드를 계속 출력)
    pthread_create(&rcv_thread, NULL, recv_msg, (void *)&sock);

    // 4. 메인 스레드는 사용자 입력 전담
    // ... (main 함수 내부 while 루프 부분) ...

    while (1) {
        char ch;
        scanf(" %c", &ch);

        C2S_Packet req;
        int valid_input = 1;

        switch (ch) {
            case 'w': req.action = 0; break; // UP
            case 's': req.action = 1; break; // DOWN
            case 'a': req.action = 2; break; // LEFT
            case 'd': req.action = 3; break; // RIGHT
            case 'q': 
                req.action = 9; // [수정] QUIT 신호 설정
                break;
            default: valid_input = 0; break;
        }

        if (valid_input) {
            write(sock, &req, sizeof(req)); // 서버로 전송
            
            // [수정] QUIT 신호를 보냈으면 클라이언트도 안전하게 종료
            if (req.action == 9) {
                printf("Quitting game...\n");
                close(sock);
                exit(0);
            }
        }
    }

    close(sock);
    return 0;
}

// ============================================
// [수신 스레드] 서버로부터 게임 상태를 받아 화면에 출력
// ============================================
void *recv_msg(void *arg) {
    int sock = *((int *)arg);
    S2C_Packet packet;
    int str_len;

    while (1) {
        // 서버로부터 S2C_Packet 구조체 크기만큼 읽음
        str_len = read(sock, &packet, sizeof(packet));
        if (str_len == -1)
            return (void *)-1;
        if (str_len == 0) {
            printf("Server disconnected.\n");
            exit(0);
        }

        // 화면 지우기 (리눅스 명령어)
        system("clear"); 
        
        // 받은 데이터 출력
        print_game_state(&packet);
    }
    return NULL;
}

// ============================================
// [출력 헬퍼] 보드 상태를 텍스트로 예쁘게 출력
// ============================================
void print_game_state(S2C_Packet *packet) {
    printf("================[ 2048 PvP ]================\n");
    printf("Me (Score: %d) \t\t Opponent (Score: %d)\n", packet->my_score, packet->opp_score);
    printf("--------------------------------------------\n");

    for (int i = 0; i < 4; i++) {
        // 내 보드 출력
        for (int j = 0; j < 4; j++) {
            if (packet->my_board[i][j] == 0) printf("  . ");
            else printf("%3d ", packet->my_board[i][j]);
        }
        
        printf("\t|\t"); // 구분선

        // 상대 보드 출력
        for (int j = 0; j < 4; j++) {
            if (packet->opp_board[i][j] == 0) printf("  . ");
            else printf("%3d ", packet->opp_board[i][j]);
        }
        printf("\n");
    }
    printf("--------------------------------------------\n");
    
    // 공격 큐 정보 출력
    printf("Pending Attacks: ");
    for(int i=0; i<packet->attack_count; i++) {
        printf("[%d] ", packet->pending_attacks[i]);
    }
    printf("\n");
    
    if (packet->game_status == 3) printf("\n*** GAME OVER ***\n");
    
    printf("\nInput (w/a/s/d): ");
    fflush(stdout); // 출력 버퍼 비우기
}

void error_handling(const char *msg) {
    perror(msg);
    exit(1);
}