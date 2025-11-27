#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sys/select.h>
#include <signal.h>

#include "protocol.h"
#include "game.h"

#define MAX_CLNT 2

// 전역 변수
int clnt_socks[MAX_CLNT];
GameState game_states[MAX_CLNT];
pthread_mutex_t mut;
int serv_sock_global;

// 함수 선언
void *handle_client(void *arg);
// [수정] send_game_state 삭제 -> compose_packet만 남김
void compose_packet(int id, S2C_Packet *res_packet);
void error_handling(const char *msg);

int get_client_count() {
    int count = 0;
    for (int i = 0; i < MAX_CLNT; i++) {
        if (clnt_socks[i] != -1) count++;
    }
    return count;
}

void handle_sigint(int sig) {
    printf("\n[Server] Shutting down ...\n");
    
    // 1. 서버 소켓 닫기 (포트 반납)
    close(serv_sock_global);
    
    // 2. 뮤텍스 파괴
    pthread_mutex_destroy(&mut);
    
    printf("[Server] Bye!\n");
    exit(0); // 프로그램 종료
}

int main(int argc, char *argv[]) {
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    socklen_t clnt_adr_sz;
    pthread_t t_id;

    if (argc == 1) {
        printf("Using default port 8080\n");
        argv[1] = "8080";
    } else if (argc != 2) {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }
    
    signal(SIGINT, handle_sigint);

    for(int i=0; i<MAX_CLNT; i++) clnt_socks[i] = -1;
    pthread_mutex_init(&mut, NULL);
    game_init(&game_states[0]);
    game_init(&game_states[1]);

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    serv_sock_global = serv_sock;
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

    while (1) {
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_sz);
        
        pthread_mutex_lock(&mut);

        int empty_slot = -1;
        for (int i = 0; i < MAX_CLNT; i++) {
            if (clnt_socks[i] == -1) {
                empty_slot = i;
                break;
            }
        }

        if (empty_slot == -1) {
            pthread_mutex_unlock(&mut);
            printf("Server full! Connection rejected.\n");
            close(clnt_sock);
            continue;
        }

        clnt_socks[empty_slot] = clnt_sock;
        game_init(&game_states[empty_slot]); 
        
        int *id_ptr = (int *)malloc(sizeof(int));
        *id_ptr = empty_slot;
        
        printf("Connected client IP: %s (Player %d)\n", inet_ntoa(clnt_adr.sin_addr), empty_slot + 1);

        // [수정] 매칭 성공 시 전송 (compose + write 분리)
        S2C_Packet start_pkt1, start_pkt2;
        int sock1 = -1, sock2 = -1;
        int need_start_send = 0;

        if (get_client_count() == 2) {
            printf("Match Found! Starting game...\n");
            if (clnt_socks[0] != -1) {
                compose_packet(0, &start_pkt1);
                sock1 = clnt_socks[0];
            }
            if (clnt_socks[1] != -1) {
                compose_packet(1, &start_pkt2);
                sock2 = clnt_socks[1];
            }
            need_start_send = 1;
        }

        pthread_mutex_unlock(&mut); // ★ 락 해제

        // [수정] 락 밖에서 전송
        if (need_start_send) {
            if (sock1 != -1) write(sock1, &start_pkt1, sizeof(S2C_Packet));
            if (sock2 != -1) write(sock2, &start_pkt2, sizeof(S2C_Packet));
            printf("Both players connected. Game start\n");
        }

        pthread_create(&t_id, NULL, handle_client, (void *)id_ptr);
        pthread_detach(t_id); 
    }

    close(serv_sock);
    return 0;
}

void *handle_client(void *arg) {
    int my_id = *((int *)arg);
    int opp_id = (my_id + 1) % 2;
    int sock = clnt_socks[my_id];
    free(arg);

    C2S_Packet req_packet;
    int idle_timer = 0;

    // [수정] 최초 접속 시 패킷 전송 (compose + write)
    S2C_Packet init_pkt;
    pthread_mutex_lock(&mut);
    compose_packet(my_id, &init_pkt);
    pthread_mutex_unlock(&mut);
    write(sock, &init_pkt, sizeof(init_pkt));

    while (1) {
        fd_set reads;
        FD_ZERO(&reads);
        FD_SET(sock, &reads);

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int result = select(sock + 1, &reads, 0, 0, &timeout);

        if (result < 0) break; 

        // [Case A] 타임아웃
        if (result == 0) {
            S2C_Packet my_pkt, opp_pkt;
            int opp_sock = -1;
            int need_send = 0;

            pthread_mutex_lock(&mut);
            
            if (get_client_count() >= 2 && game_states[my_id].attack_cnt > 0) {
                idle_timer++;
                printf("[P%d] Warning: Idle for %d sec\n", my_id + 1, idle_timer);

                if (idle_timer >= 5) {
                    printf("[P%d] Timeout! Executing Attack.\n", my_id + 1);
                    game_execute_attack(&game_states[my_id]);
                    game_is_over(&game_states[my_id]);
                    
                    // [수정] 패킷 생성
                    compose_packet(my_id, &my_pkt);
                    opp_sock = clnt_socks[opp_id];
                    if (opp_sock != -1) compose_packet(opp_id, &opp_pkt);
                    
                    idle_timer = 0;
                    need_send = 1;
                }
            } else {
                idle_timer = 0;
            }
            pthread_mutex_unlock(&mut); // ★ 락 해제

            // [수정] 락 밖에서 전송
            if (need_send) {
                write(sock, &my_pkt, sizeof(my_pkt));
                if (opp_sock != -1) write(opp_sock, &opp_pkt, sizeof(opp_pkt));
            }
            continue;
        }

        // [Case B] 데이터 수신
        if (FD_ISSET(sock, &reads)) {
            int str_len = read(sock, &req_packet, sizeof(req_packet));
            if (str_len <= 0) break;

            if (req_packet.action == QUIT) {
                printf("[Player %d] Quit request received.\n", my_id + 1);
                break;
            }

            // 전송할 변수 준비
            S2C_Packet my_pkt, opp_pkt;
            int opp_sock = -1;
            int need_send = 0;

            pthread_mutex_lock(&mut);

            if (get_client_count() >= 2 && !game_states[my_id].game_over) {
                idle_timer = 0; 

                int score_gained = game_move(&game_states[my_id], (Direction)req_packet.action);

                if (game_states[my_id].moved) {
                    game_spawn_tile(&game_states[my_id]);
                }
                
                game_execute_attack(&game_states[my_id]);
                game_is_over(&game_states[my_id]);

                if (score_gained >= 128) {
                    int attack_count = score_gained / 128;
                    if (attack_count > 4) attack_count = 4;
                    for (int i = 0; i < attack_count; i++) {
                        int attack_value = (rand() % 10 == 0) ? 4 : 2;
                        game_queue_attack(&game_states[opp_id], attack_value);
                    }
                    printf("[P%d] Attack! Sent %d blocks\n", my_id + 1, attack_count);
                }
                
                // [수정] 패킷 생성
                compose_packet(my_id, &my_pkt);
                opp_sock = clnt_socks[opp_id];
                if (opp_sock != -1) compose_packet(opp_id, &opp_pkt);
                need_send = 1;
            }
            else if (game_states[my_id].game_over) {
                 // 게임오버 상태에서도 화면 갱신은 필요할 수 있음
                 compose_packet(my_id, &my_pkt);
                 opp_sock = clnt_socks[opp_id];
                 if (opp_sock != -1) compose_packet(opp_id, &opp_pkt);
                 need_send = 1;
            }

            pthread_mutex_unlock(&mut); // ★ 락 해제

            // [수정] 락 밖에서 전송
            if (need_send) {
                write(sock, &my_pkt, sizeof(my_pkt));
                if (opp_sock != -1) write(opp_sock, &opp_pkt, sizeof(opp_pkt));
            }
        }
    }
    
    // 연결 종료 처리
    S2C_Packet opp_wait_pkt;
    int opp_sock_final = -1;
    int send_wait = 0;

    pthread_mutex_lock(&mut);
    clnt_socks[my_id] = -1;
    printf("Player %d disconnected.\n", my_id + 1);

    int current_cnt = get_client_count();
    if (current_cnt == 0) {
        printf("All players disconnected. Resetting game states...\n");
        game_init(&game_states[0]);
        game_init(&game_states[1]);
    } else {
        if (clnt_socks[opp_id] != -1) {
            opp_sock_final = clnt_socks[opp_id];
            compose_packet(opp_id, &opp_wait_pkt);
            send_wait = 1;
        }
    }
    pthread_mutex_unlock(&mut);
    
    if (send_wait && opp_sock_final != -1) {
        write(opp_sock_final, &opp_wait_pkt, sizeof(opp_wait_pkt));
    }

    close(sock);
    return NULL;
}

// ============================================
// [Helper] 패킷 데이터 채우기 (전송 안 함)
// ============================================
void compose_packet(int id, S2C_Packet *res_packet) {
    // [수정] 내부 변수 선언 제거 -> 인자로 받은 res_packet 사용
    int opp_id = (id + 1) % 2;

    // 1. 내 정보 채우기
    memcpy(res_packet->my_board, game_states[id].board, sizeof(int) * 16);
    res_packet->my_score = game_states[id].score;
    
    // 2. 상대방 정보 채우기
    if (clnt_socks[opp_id] != -1) {
        memcpy(res_packet->opp_board, game_states[opp_id].board, sizeof(int) * 16);
        res_packet->opp_score = game_states[opp_id].score;
    } else {
        memset(res_packet->opp_board, 0, sizeof(int) * 16);
        res_packet->opp_score = 0;
    }

    // 3. 공격 정보
    memcpy(res_packet->pending_attacks, game_states[id].attack_queue, sizeof(int) * 10);
    res_packet->attack_count = game_states[id].attack_cnt;
    res_packet->highlight_r = game_states[id].highlight_r;
    res_packet->highlight_c = game_states[id].highlight_c;
    
    // 4. 게임 상태 판정
    if (get_client_count() < 2) {
        res_packet->game_status = GAME_WAITING;
    }
    else {
        bool i_am_over = game_states[id].game_over;
        bool opp_is_over = game_states[opp_id].game_over;

        if (i_am_over && opp_is_over) {
            if (game_states[id].score > game_states[opp_id].score) {
                res_packet->game_status = GAME_WIN;
            } else if (game_states[id].score < game_states[opp_id].score) { 
                res_packet->game_status = GAME_LOSE;
            } else {
                res_packet->game_status = GAME_LOSE; // 무승부=패배 처리
            }
        } else if (i_am_over) {
            res_packet->game_status = GAME_OVER_WAIT;
        } else {
            res_packet->game_status = GAME_PLAYING;
        }
    }
    // [수정] write 호출 없음
}

void error_handling(const char *message) {
    perror(message);
    exit(1);
}