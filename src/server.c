#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#include "protocol.h" // 네트워크 패킷 정의
#include "game.h"     // 게임 로직 정의

// ============================================
// [전역 변수] 모든 스레드가 공유하는 데이터 (Shared Resources)
// ============================================
#define MAX_CLNT 2

int clnt_socks[MAX_CLNT];     // 접속한 클라이언트 소켓 목록
GameState game_states[MAX_CLNT]; // 각 플레이어의 게임 상태 (0번: 1P, 1번: 2P)
int clnt_cnt = 0;             // 현재 접속자 수
pthread_mutex_t mut;          // 게임 상태 보호를 위한 뮤텍스

void *handle_client(void *arg);
void send_game_state(int my_id);
void error_handling(const char *msg);

int main(int argc, char *argv[]) {
    int serv_sock;
    int clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    socklen_t clnt_adr_sz;
    pthread_t t_id;

    if (argc != 2) {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    // 1. 뮤텍스 초기화 (열쇠 만들기)
    pthread_mutex_init(&mut, NULL);

    // 2. 게임 상태 초기화 (1P, 2P 보드 준비)
    game_init(&game_states[0]);
    game_init(&game_states[1]);

    // 3. 소켓 생성 및 설정
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1) error_handling("socket() error");

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");

    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    printf("Game Server Started on port %s...\n", argv[1]);

    // 4. 클라이언트 수락 루프 (Thread-per-Client)
    while (1) {
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_sz);
        
        if (clnt_cnt >= MAX_CLNT) {
            printf("Server full! Connection rejected.\n");
            close(clnt_sock);
            continue;
        }

        // [Critical Section] 접속자 정보 갱신 (뮤텍스 잠금)
        pthread_mutex_lock(&mut);
        clnt_socks[clnt_cnt] = clnt_sock;
        
        // 스레드에게 넘겨줄 ID (0 또는 1)를 힙 메모리에 할당
        int *id_ptr = (int *)malloc(sizeof(int));
        *id_ptr = clnt_cnt; 
        
        clnt_cnt++;
        pthread_mutex_unlock(&mut);

        printf("Connected client IP: %s (Player %d)\n", inet_ntoa(clnt_adr.sin_addr), *id_ptr + 1);

        // 5. 클라이언트 전담 스레드 생성!
        pthread_create(&t_id, NULL, handle_client, (void *)id_ptr);
        pthread_detach(t_id); // 스레드가 종료되면 알아서 메모리 해제되도록 설정
    }

    close(serv_sock);
    return 0;
}

// ============================================
// [스레드 함수] 클라이언트 1명을 전담하여 처리
// ============================================
void *handle_client(void *arg) {
    int my_id = *((int *)arg);
    int opp_id = (my_id + 1) % 2;
    int sock = clnt_socks[my_id];
    free(arg);

    C2S_Packet req_packet;
    
    // 최초 접속 시 상태 전송
    pthread_mutex_lock(&mut);
    send_game_state(my_id);
    pthread_mutex_unlock(&mut);

    while (read(sock, &req_packet, sizeof(req_packet)) > 0) {
        
        // [수정 1] 종료 신호(9) 처리
        if (req_packet.action == 9) {
            printf("[Player %d] Quit request received.\n", my_id + 1);
            break; // 루프 탈출 -> 연결 종료 처리로 이동
        }

        pthread_mutex_lock(&mut);

        // 1. 게임 오버 체크 (움직임 방지)
        if (game_states[my_id].game_over) {
            send_game_state(my_id);
            if (clnt_cnt > 1) send_game_state(opp_id);
            pthread_mutex_unlock(&mut);
            continue;
        }

        // 2. 이동 로직
        // [경고 해결] 명시적 형변환
        int score_gained = game_move(&game_states[my_id], (Direction)req_packet.action);

        // 3. 타일 생성 및 공격 실행
        // 움직였으면 타일 생성
        if (game_states[my_id].moved) {
            game_spawn_tile(&game_states[my_id]);
        }
        
        // [핵심] 큐에 있는 공격 실행 (여기서 highlight_r/c가 설정됨)
        // 움직임 여부와 상관없이 시도하거나, moved일 때만 시도할 수 있음.
        // 보통 2048류 게임은 턴이 넘어갈 때 공격이 들어오므로 moved 체크를 하는 게 자연스러움.
        // 하지만 비동기성을 위해 매번 호출하되 game_logic 안에서 큐가 있을 때만 동작함.
        game_execute_attack(&game_states[my_id]); 

        // [CRITICAL FIX] 게임 상태 갱신 (반드시 호출해야 함!)
        // 이동과 생성이 끝난 후, 더 이상 움직일 수 있는지 체크하여 game_over 플래그를 업데이트합니다.
        game_is_over(&game_states[my_id]);

        // 4. 128점 달성 시 상대 큐에 공격 추가
        if (score_gained >= 128) {
            game_queue_attack(&game_states[opp_id], 2);
            printf("[P%d] Attack Queued -> [P%d] (Score: %d)\n", my_id + 1, opp_id + 1, score_gained);
        }

        // 5. 상태 전송
        send_game_state(my_id);
        if (clnt_cnt > 1) send_game_state(opp_id);

        pthread_mutex_unlock(&mut);
    }
    
    // 연결 종료 시 처리
    pthread_mutex_lock(&mut);
    clnt_cnt--;
    // (단순화를 위해 배열 정리는 생략, 실제로는 소켓 목록 당기기 등이 필요)
    printf("Player %d disconnected.\n", my_id + 1);
    pthread_mutex_unlock(&mut);
    
    close(sock);
    return NULL;
}

// ============================================
// [Helper] S2C 패킷을 만들어 클라이언트에게 전송
// ============================================
void send_game_state(int id) {
    int opp_id = (id + 1) % 2;
    S2C_Packet res_packet;

    // 1. 내 정보 채우기
    memcpy(res_packet.my_board, game_states[id].board, sizeof(int) * 16);
    res_packet.my_score = game_states[id].score;
    
    // 2. 상대방 정보 채우기 (상대가 없으면 0)
    if (clnt_cnt > 1) {
        memcpy(res_packet.opp_board, game_states[opp_id].board, sizeof(int) * 16);
        res_packet.opp_score = game_states[opp_id].score;
    } else {
        memset(res_packet.opp_board, 0, sizeof(int) * 16);
        res_packet.opp_score = 0;
    }

    // 3. 공격 정보 채우기
    memcpy(res_packet.pending_attacks, game_states[id].attack_queue, sizeof(int) * 10);
    res_packet.attack_count = game_states[id].attack_cnt;
    // res_packet.attack_timer = ... (필요시 구현)
    // [핵심 추가] 하이라이트 정보 전송
    res_packet.highlight_r = game_states[id].highlight_r;
    res_packet.highlight_c = game_states[id].highlight_c;
    
    // 4. 게임 상태 판정 (대기 및 최종 승패 로직 적용)
    bool i_am_over = game_states[id].game_over;
    bool opp_is_over = (clnt_cnt > 1) ? game_states[opp_id].game_over : true; 

    // [중요: game_states[id].game_over 플래그 업데이트]
    // game_is_over 함수 내부에서 game_over 플래그를 업데이트 하므로, 여기서 따로 할 필요 없음.

    if (i_am_over && opp_is_over) {
        // [수정] 둘 다 움직일 수 없는 경우: 점수 비교하여 최종 승패 결정
        if (game_states[id].score > game_states[opp_id].score) {
            res_packet.game_status = GAME_WIN; // 내가 승리
        } else { 
            // 점수가 같거나 낮으면 패배로 간주
            res_packet.game_status = GAME_LOSE; // 내가 패배
        }
    } else if (i_am_over) {
        // [나만 멈춤] -> 상대방 기다리는 중
        res_packet.game_status = GAME_OVER_WAIT;
    } else {
        // [수정] 내가 진행 중 (상대 멈춤 여부 관계없이): GAME_PLAYING (1)
        res_packet.game_status = GAME_PLAYING;
    }
    
    // 5. 전송
    write(clnt_socks[id], &res_packet, sizeof(res_packet));
}

void error_handling(const char *message) {
    perror(message);
    exit(1);
}