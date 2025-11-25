#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <ncurses.h> 

#include "protocol.h" 

// ============================================
// [전역 변수]
// ============================================
int sock;                   
pthread_mutex_t draw_mutex; 

// ============================================
// [함수 선언]
// ============================================
void *recv_msg(void *arg);       
void draw_game(S2C_Packet *pkt); 
void init_ncurses_settings();    
void cleanup_and_exit(int exit_code, const char *msg); 

int main(int argc, char *argv[]) {
    struct sockaddr_in serv_addr;
    pthread_t rcv_thread;

    if (argc != 3) {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    sock = socket(PF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("connect() error");
        exit(1);
    }

    init_ncurses_settings();
    pthread_mutex_init(&draw_mutex, NULL);
    pthread_create(&rcv_thread, NULL, recv_msg, (void *)&sock);

    while (1) {
        int ch = getch(); 
        
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
            write(sock, &req, sizeof(req));
            if (req.action == QUIT) {
                cleanup_and_exit(0, "Quit by user.");
            }
        }
    }
    return 0;
}

void *recv_msg(void *arg) {
    (void)arg;
    S2C_Packet packet;
    int str_len;

    while (1) {
        str_len = read(sock, &packet, sizeof(packet));
        if (str_len <= 0) {
            cleanup_and_exit(1, "Server disconnected."); 
            break;
        }
        pthread_mutex_lock(&draw_mutex);
        draw_game(&packet);
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

void draw_game(S2C_Packet *packet) {
    clear();

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
                mvprintw(y_opp, x_opp, "%4d ", val_opp);
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
    close(sock);
    
    if (msg != NULL) {
        printf("%s\n", msg);
    }
    exit(exit_code);
}