#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <ncurses.h> 

#include "protocol.h" 
#include "game.h"
// ============================================
// [전역 변수]
// ============================================
int sock = -1;                   
pthread_mutex_t draw_mutex; 
char *server_ip = "127.0.0.1"; // 기본값 설정
int server_port = 8080;        // 기본값 설정

// ============================================
// [함수 선언]
// ============================================
void *recv_msg(void *arg);       
void draw_game(S2C_Packet *pkt, int is_single_mode); 
void init_ncurses_settings();    
void cleanup_and_exit(int exit_code, const char *msg); 
int recv_all(int sock, void *buffer, size_t len);

// 메뉴 및 모드 관련
int show_main_menu();
void run_single_player_mode();
void run_multiplayer_mode();

int main(int argc, char *argv[]) {

    if (argc == 1) {
        printf("Using default IP %s and port %d\n", server_ip, server_port);
    } else if (argc == 2) {
        server_port = atoi(argv[1]);
        printf("Using default IP %s and custom port %d\n", server_ip, server_port);
    } else if (argc == 3) {
        server_ip = argv[1];
        server_port = atoi(argv[2]);
        printf("Using custom IP %s and port %d\n", server_ip, server_port);
    } else {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    sleep(1);

    // 서버연결부분 추후에 실행
    init_ncurses_settings();
    pthread_mutex_init(&draw_mutex, NULL);

    while(1) {
        int choice = show_main_menu();
        switch (choice) {
            case 1:
                nodelay(stdscr, FALSE); // 싱글은 블로킹 모드
                run_single_player_mode();
                break;
            case 2:
                nodelay(stdscr, TRUE); // 멀티는 논블로킹 모드
                run_multiplayer_mode();
                break;
            case 3:
                cleanup_and_exit(0, "Exiting the game");
                break;
            default:
                break;
        }
    }

    return 0;
}

// 메인 메뉴 UI 구현
int show_main_menu() {
    clear();
    int row, col;
    getmaxyx(stdscr, row, col); //화면 크기 구하기

    int center_y = row / 2;
    int center_x = col / 2 - 15;
    if(center_x < 0) center_x = 0;  
    if (center_y < 4) center_y = 4;  

    attron(COLOR_PAIR (3) | A_BOLD);
    mvprintw(center_y - 4, center_x, "===============================");
    mvprintw(center_y - 3, center_x, "|       2048 PvP CLIENT       |");
    mvprintw(center_y - 2, center_x, "===============================");
    attroff(COLOR_PAIR (3) | A_BOLD);

    mvprintw(center_y, center_x, "1. Single Player Mode");
    mvprintw(center_y + 1, center_x, "2. Multiplayer Mode");
    mvprintw(center_y + 2, center_x, "3. Exit");
    mvprintw(center_y + 4, center_x, "Select an option [1-3]: ");
    refresh();

    while(1) {
        int ch = getch();
        if (ch == '1') {
            return 1;
        } else if (ch == '2') {
            return 2;
        } else if (ch == '3') {
            return 3;
        }
    }
}

// ============================================
// [싱글 플레이어 모드]
// ============================================
void run_single_player_mode() {
    // 1. 로컬 게임 상태 생성 및 초기화
    GameState local_state;
    game_init(&local_state);

    while (1) {
        // 2. 화면 그리기 (GameState -> S2C_Packet 변환)
        // draw_game 함수를 재사용하기 위해 가짜 패킷생성.
        S2C_Packet display_packet;
        memset(&display_packet, 0, sizeof(display_packet));

        // 내 보드와 점수 복사
        memcpy(display_packet.my_board, local_state.board, sizeof(int) * 16);
        display_packet.my_score = local_state.score;
        
        memset(display_packet.opp_board, 0, sizeof(int) * 16);
        display_packet.opp_score = 0;
        display_packet.attack_count = 0;
        display_packet.highlight_r = -1; 
        display_packet.highlight_c = -1;

        // 게임 상태 설정
        if (local_state.game_over) {
            display_packet.game_status = GAME_LOSE; // 싱글에선 오버되면 LOSE 취급 (혹은 메시지 따로 처리 가능)
        } else {
            display_packet.game_status = GAME_PLAYING;
        }

        // 화면 그리기 (뮤텍스 필요 없음 - 싱글 스레드)
        draw_game(&display_packet, 1);

        // 3. 게임 오버 시 종료 처리
        if (local_state.game_over) {
            // 게임 오버 상태에서 키 입력 대기 (바로 꺼지지 않게)
            int ch = getch();
            if (ch == 'q' || ch == 'Q') break; // q 누르면 메뉴로
            continue; 
        }

        // 4. 입력 처리
        int ch = getch();
        Direction dir;
        int valid_move = 0;

        switch (ch) {
            case 'w': case 'W': case KEY_UP:    dir = UP;    valid_move = 1; break;
            case 's': case 'S': case KEY_DOWN:  dir = DOWN;  valid_move = 1; break;
            case 'a': case 'A': case KEY_LEFT:  dir = LEFT;  valid_move = 1; break;
            case 'd': case 'D': case KEY_RIGHT: dir = RIGHT; valid_move = 1; break;
            case 'q': case 'Q': return; // 메뉴로 복귀
            default: valid_move = 0; break;
        }

        // 5. 로직 실행 (서버 없이 직접 계산)
        if (valid_move) {
            game_move(&local_state, dir);
            if (local_state.moved) {
                game_spawn_tile(&local_state);
            }
            game_is_over(&local_state); // 게임 오버 체크
        }
    }
}
// 기존 로직 멀티 플레이어 모드
void run_multiplayer_mode() {
    struct sockaddr_in serv_addr;
    pthread_t rcv_thread;
    // 소켓 생성
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if( sock == -1) {
        cleanup_and_exit(1, "Socket creation error");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;

    serv_addr.sin_addr.s_addr = inet_addr(server_ip);
    serv_addr.sin_port = htons(server_port);
    // 연결 대기 화면
    clear();
    mvprintw(10, 20, "Connecting to server...( %s:%d )", server_ip, server_port);
    refresh();

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        // 연결 실패 시 처리
        mvprintw(12, 20, "Connection failed! Press any key...");
        refresh();
        
        nodelay(stdscr, FALSE); // 키 입력 기다림
        getch(); 
        
        close(sock);
        sock = -1;
        return; // 메뉴로 복귀
    }

    pthread_create(&rcv_thread, NULL, recv_msg, (void *)&sock);

    while (1) {
        int ch = getch(); 
        
        if (ch == ERR) {
            usleep(10000);
            continue;
        }

        C2S_Packet req;
        int valid_input = 0; 
        
        switch (ch) {
            case 'w': case 'W': case KEY_UP:    req.action = MOVE_UP; valid_input = 1; break;
            case 's': case 'S': case KEY_DOWN:  req.action = MOVE_DOWN; valid_input = 1; break;
            case 'a': case 'A': case KEY_LEFT:  req.action = MOVE_LEFT; valid_input = 1; break;
            case 'd': case 'D': case KEY_RIGHT: req.action = MOVE_RIGHT; valid_input = 1; break;
            case 'q': case 'Q': req.action = QUIT; valid_input = 1; break;
        }

        if (valid_input) {
            if(sock != -1) {
                write(sock, &req, sizeof(req));
                if(req.action == QUIT) {
                    // 서버에 종료 알리고 루프 탈출
                    pthread_cancel(rcv_thread);pthread_join(rcv_thread, NULL);
                    close(sock);
                    sock = -1;
                    return;
                }
            }
        }
    }
}

int recv_all(int sock, void *buffer, size_t len) {
    size_t received = 0;
    char *ptr = (char *)buffer;
    while (received < len) {
        int ret = read(sock, ptr + received, len - received);
        if (ret <= 0) return ret;
        received += ret;
    }
    return received;
}

void *recv_msg(void *arg) {
    (void)arg;
    S2C_Packet packet;
    int str_len;

    while (1) {
        str_len = recv_all(sock, &packet, sizeof(packet));
        
        if (str_len <= 0) {
            // 서버 끊김 처리
            pthread_mutex_lock(&draw_mutex);
            clear();
            mvprintw(10, 20, "Server disconnected! Press 'q' to exit...");
            refresh();
            pthread_mutex_unlock(&draw_mutex);
            
            // 메인 루프 종료를 유도하거나 여기서 대기
            // 간단하게는 소켓 닫고 break하면 메인 루프의 write에서 에러나서 종료됨
            close(sock);
            sock = -1;
            break;
        }
        pthread_mutex_lock(&draw_mutex);
        draw_game(&packet, 0);
        pthread_mutex_unlock(&draw_mutex);
    }
    return NULL;
}

// ============================================
// [UI 그리기] 
// ============================================

#define START_Y 6
#define START_X_ME 2
#define START_X_OPP 40
#define CELL_WIDTH 6
#define CELL_HEIGHT 2 

void draw_game(S2C_Packet *packet, int is_single_mode) {
    clear();

    int row, col;
    getmaxyx(stdscr, row, col); //화면 크기 구하기
    (void)row;
    if (is_single_mode) {
        int board_width = (CELL_WIDTH * 4);
        int start_x = (col - board_width)/ 2;
        if(start_x < 0) start_x = 0;

        const char* title = "======[ 2048 Single Player ]======";
        mvprintw(1, (col - strlen(title)) / 2, "%s", title);

        char score_str[50];
        sprintf(score_str, "Score: %d", packet->my_score);
        attron(COLOR_PAIR(4));
        mvprintw(3, (col - strlen(score_str)) / 2, "%s", score_str);
        attroff(COLOR_PAIR(4));

        for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    int val = packet->my_board[i][j];
                    int x = start_x + (j * CELL_WIDTH);
                    int y = START_Y + (i * CELL_HEIGHT);
                    
                    if (val == 0) {
                        mvprintw(y, x, "  .  ");
                    } else {
                        if (val < 10) mvprintw(y, x, "  %d  ", val);
                        else if (val < 100) mvprintw(y, x, " %d  ", val);
                        else if (val < 1000) mvprintw(y, x, " %d ", val);
                        else mvprintw(y, x, "%d ", val);
                    }
                }
            }

            // 게임 종료 메시지
            int status_y = START_Y + 9; // 보드 바로 아래
        
            if (packet->game_status == GAME_LOSE) {
                char msg[100];
                sprintf(msg, "!!! GAME OVER (Final Score: %d) !!!", packet->my_score);
                attron(COLOR_PAIR(2) | A_BLINK | A_STANDOUT);
                mvprintw(status_y, (col - strlen(msg)) / 2, "%s", msg);
                attroff(COLOR_PAIR(2) | A_BLINK | A_STANDOUT);
            }

            const char* guide = "Input (w/a/s/d) or 'q' to Quit";
            mvprintw(status_y + 2, (col - strlen(guide)) / 2, "%s", guide);
            
            refresh();
            return; // 싱글 모드 그리기 종료
        }
    // 대기실 화면 처리
    if (packet->game_status == GAME_WAITING) {
        attron(COLOR_PAIR(3) | A_BOLD); // Cyan 색상
        mvprintw(10, 20, "===================================");
        mvprintw(11, 20, "|     WAITING FOR OPPONENT...     |");
        mvprintw(12, 20, "|                                 |");
        mvprintw(13, 20, "|     [ 1 / 2 Players Ready ]     |");
        mvprintw(14, 20, "|                                 |");
        mvprintw(15, 20, "|   Press 'q' to quit the game    |");
        mvprintw(16, 20, "===================================");
        attroff(COLOR_PAIR(3) | A_BOLD);
        refresh();
        return; // 보드 그리지 않고 리턴
    }
    
    // 1. 타이틀 및 점수
    mvprintw(1, 25, "======[ 2048 PvP ]======");

    attron(COLOR_PAIR(4));
    mvprintw(3, 2,  "Me (Score: %d)", packet->my_score);
    mvprintw(3, 40, "Opponent (Score: %d)", packet->opp_score);
    attroff(COLOR_PAIR(4));

    // 2. 보드 그리기
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            
            // --- 내 보드 ---
            int val_me = packet->my_board[i][j];
            int x_me = START_X_ME + (j * CELL_WIDTH);
            int y_me = START_Y + (i * CELL_HEIGHT);
            
            bool is_highlight = (i == packet->highlight_r && j == packet->highlight_c);
            if (is_highlight) attron(COLOR_PAIR(2) | A_STANDOUT | A_BOLD);

            if (val_me == 0) {
                mvprintw(y_me, x_me, "  .  ");
            } else {
                //중앙정렬 구현
                if (val_me < 10)
                    mvprintw(y_me, x_me, "  %d  ", val_me);
                else if (val_me < 100)
                    mvprintw(y_me, x_me, " %d  ", val_me);
                else if (val_me < 1000)
                    mvprintw(y_me, x_me, " %d ", val_me);
                else
                    mvprintw(y_me, x_me, "%d ", val_me);
            }

            if (is_highlight) attroff(COLOR_PAIR(2) | A_STANDOUT | A_BOLD);
            
            // --- 상대 보드 ---
            int val_opp = packet->opp_board[i][j];
            int x_opp = START_X_OPP + (j * CELL_WIDTH);
            int y_opp = START_Y + (i * CELL_HEIGHT);
            
            if (val_opp == 0) {
                mvprintw(y_opp, x_opp, "  .  ");
            } else {
                //중앙정렬 구현
                if (val_opp < 10)
                    mvprintw(y_opp, x_opp, "  %d  ", val_opp);
                else if (val_opp < 100)
                    mvprintw(y_opp, x_opp, " %d  ", val_opp);
                else if (val_opp < 1000)
                    mvprintw(y_opp, x_opp, " %d ", val_opp);
                else
                    mvprintw(y_opp, x_opp, "%d ", val_opp);
            }
        }
    }
    
    // 3. 하단 정보 (공격 대기열 출력) -> [좌표: 16번째 줄]
    mvprintw(16, 2, "Pending Attacks (Queue): ");
    
    if (packet->attack_count > 0) {
        attron(COLOR_PAIR(2)); 
        for(int i = 0; i < packet->attack_count; i++) {
            printw("[%d] ", packet->pending_attacks[i]);
        }
        attroff(COLOR_PAIR(2));
    } else {
        printw("None");
    }
    
    // 4. 게임 종료 상태 메시지 -> [수정: 좌표를 18번째 줄로 이동하여 겹침 방지]
    int status_y = 18; 
    
    if (packet->game_status == GAME_OVER_WAIT) {
        // [대기 상태] 파랑배경
        attron(COLOR_PAIR(3)); 
        mvprintw(status_y, 2, "!!! NO MOVES - WAITING FOR OPPONENT (%d) !!!", packet->opp_score);
        mvprintw(status_y + 1, 2, "YOUR FINAL SCORE: %d", packet->my_score);
        attroff(COLOR_PAIR(3));
        
    } else if (packet->game_status == GAME_LOSE) {
        // [최종 패배]
        attron(COLOR_PAIR(2) | A_BLINK | A_STANDOUT);
        mvprintw(status_y, 2, "!!! GAME OVER - YOU LOSE (%d) !!!", packet->my_score);
        attroff(COLOR_PAIR(2) | A_BLINK | A_STANDOUT);
        
    } else if (packet->game_status == GAME_WIN) {
        // [최종 승리]
        attron(COLOR_PAIR(4) | A_STANDOUT);
        mvprintw(status_y, 2, "*** VICTORY - YOU WIN (%d) ***", packet->my_score);
        attroff(COLOR_PAIR(4) | A_STANDOUT);
        
    } else {
         // [진행 중] 메시지 영역 지우기
        mvprintw(status_y, 2, "                                                    ");
        mvprintw(status_y + 1, 2, "                                                    ");
    }

    // 입력 가이드 위치도 살짝 아래로 조정 (21번째 줄)
    mvprintw(21, 2, "Input (w/a/s/d) or 'q' to Quit");
    
    refresh();
}

void init_ncurses_settings() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    attron(A_BOLD); 
    
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_WHITE, COLOR_BLACK); 
        init_pair(2, COLOR_RED,   COLOR_BLACK); // 공격 경고, 패배
        init_pair(3, COLOR_CYAN, COLOR_BLACK); // 대기 상태
        init_pair(4, COLOR_YELLOW, COLOR_BLACK); // 승리, 점수
    }
}

void cleanup_and_exit(int exit_code, const char *msg) {
    pthread_mutex_destroy(&draw_mutex);
    endwin(); 
    if (sock > 0) {
        close(sock);
    }
    
    if (msg != NULL) {
        printf("%s\n", msg);
    }
    exit(exit_code);
}