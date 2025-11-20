#include "game.h"
#include <stdlib.h> // rand(), srand()
#include <string.h> // memset()
#include <time.h>   // time()

// ==========================================
// [1] 내부 헬퍼 함수 (Internal Helper Functions)
// 이 함수들은 game.h에 선언하지 않고 여기서만 사용
// ==========================================

/**
 * @brief 4칸짜리 1차원 배열을 "왼쪽"으로 밀고 합치는 핵심 로직
 * @param line 4칸짜리 배열 포인터
 * @return 이번 줄에서 합쳐져서 획득한 점수
 */
static int process_line(int *line) {
    //process_line 함수는 무조건 "배열의 앞쪽(인덱스 0)으로 쏠리게 합친다"는 기능만 수행. 보드에서 데이터를 꺼낼 때, 이 함수가 처리하기 좋도록 순서를 바꿔서(뒤집어서) 넣어주기.
    int score = 0;
    int temp[4] = {0}; // 이동 결과를 임시 저장할 배열
    int temp_idx = 0;
    // 1. [Slide] 0이 아닌 숫자들을 왼쪽으로 밀어서 temp에 담기
    for (int i = 0; i < 4; i++) {
        if (line[i] != 0) {
            temp[temp_idx++] = line[i];
        }
    }
    // 2. [Merge] 인접한 같은 숫자 합치기
    for (int i = 0; i < temp_idx - 1; i++) {
        if (temp[i] != 0 && temp[i] == temp[i+1]) {
            temp[i] *= 2;      // 합체
            score += temp[i];  // 점수 획득
            temp[i+1] = 0;     // 합쳐진 뒤쪽은 0으로 비움
            i++;               // (중요)합쳐진 칸은 건너뜀
        }
    }
    // 3. [Slide Again] 합쳐지면서 생긴 0(빈 공간)을 다시 제거하며 원본 line에 복사
    int final_idx = 0;
    for (int i = 0; i < 4; i++) {
        if (temp[i] != 0) {
            line[final_idx++] = temp[i];
        } else {
            // temp[i]가 0이면 건너뜀 
        }
    }
    // 4. 나머지 칸은 0으로 채우기
    while (final_idx < 4) {
        line[final_idx++] = 0;
    }
    return score;
}

/**
 * @brief 빈 칸(0)이 하나라도 있는지 확인
 */
static bool can_spawn(GameState *state) {
    for(int i=0; i<4; i++) {
        for(int j=0; j<4; j++) {
            if (state->board[i][j] == 0) return true;
        }
    }
    return false;
}

// ==========================================
// [2] 공개 함수 구현 (Public API from game.h)
// ==========================================

void game_init(GameState *state) {
    // 1. 상태 초기화 (점수 0, 게임오버 false 등)
    memset(state, 0, sizeof(GameState));
    
    // 2. 랜덤 시드 설정 (프로그램 시작 시 한 번만 하는 게 좋지만 여기도 무방)
    srand(time(NULL)); 

    // 3. 초기 타일 2개 생성
    game_spawn_tile(state);
    game_spawn_tile(state);

    state->game_over = false;
    state->moved = false;
}

void game_spawn_tile(GameState *state) {
    if (!can_spawn(state)) return;
    int value = (rand() % 10 == 0) ? 4 : 2; // 10% 확률로 4
    while(1) {
        int r = rand() % 4;
        int c = rand() % 4;
        if (state->board[r][c] == 0) {
            state->board[r][c] = value;
            break;
        }
    }
}

int game_move(GameState *state, Direction dir) {
    int total_score = 0;
    int temp_line[4]; // 한 줄을 뽑아서 작업할 임시 배열
    
    // TODO: 이동 전 보드 상태를 백업 (이동 여부 판별용)
    int old_board[4][4]; 
    memcpy(old_board, state->board, sizeof(old_board));
    
    for (int i = 0; i < 4; i++) {
        // 1. 방향에 따라 한 줄(행 or 열)을 뽑아 temp_line에 넣음
        for (int j = 0; j < 4; j++) {
            switch(dir) {
                case UP:    temp_line[j] = state->board[j][i];     break; // 세로 정방향
                case DOWN:  temp_line[j] = state->board[3-j][i];   break; // 세로 역방향
                case LEFT:  temp_line[j] = state->board[i][j];     break; // 가로 정방향
                case RIGHT: temp_line[j] = state->board[i][3-j];   break; // 가로 역방향
            }
        }
        
        // 2. [처리] 핵심 로직 (무조건 왼쪽으로 쏠리게 합침)
        total_score += process_line(temp_line);
        
        // 3. [복구] 처리된 temp_line을 다시 원래 board 위치에 집어넣음
        for (int j = 0; j < 4; j++) {
            switch(dir) {
                case UP:    state->board[j][i]     = temp_line[j]; break;
                case DOWN:  state->board[3-j][i]   = temp_line[j]; break;
                case LEFT:  state->board[i][j]     = temp_line[j]; break;
                case RIGHT: state->board[i][3-j]   = temp_line[j]; break;
            }
        }
    }

    // TODO: 백업해둔 board와 현재 board를 비교
    state->moved = false;
    for (int i = 0; i < 4 && !state->moved; i++) {
        for (int j = 0; j < 4; j++) {
            if (old_board[i][j] != state->board[i][j]) {
                state->moved = true;
                break;
            }
        }
        if (state->moved) break;
    }
    state->score += total_score;
    
    return total_score;
}

// [PvP 확장 기능]
void game_queue_attack(GameState *state, int value) {
    if (state->attack_cnt < 10) {
        state->attack_queue[state->attack_cnt++] = value;
        // 타이머 설정 로직 (3번 움직임 후 발동 등)는 server.c에서 처리
    }
}

void game_execute_attack(GameState *state) {
    // 공격 큐가 비었으면 리턴
    if (state->attack_cnt <= 0) return;
    
    // 빈 공간 없으면 공격 불가 (혹은 게임오버 처리)
    if (!can_spawn(state)) return;

    // 1. 큐의 맨 앞 공격 꺼냄
    int attack_value = state->attack_queue[0];
    
    // 2. 큐 당기기 (Shift)
    for (int i = 0; i < state->attack_cnt - 1; i++) {
        state->attack_queue[i] = state->attack_queue[i + 1];
    }
    state->attack_cnt--;
    state->attack_queue[state->attack_cnt] = 0; // 끝부분 청소

    //공격 블록은 생성한 숫자가 클수록 높은 숫자가 나올 확률이 높게 설정(추후 서버에서 queue_attack 호출 시 결정)
    // 3. 보드의 빈 칸에 공격 타일 배치
    while(1) {
        int r = rand() % 4;
        int c = rand() % 4;
        if (state->board[r][c] == 0) {
            state->board[r][c] = attack_value;
            break;
        }
    }
}

bool game_is_over(GameState *state) {
    // 1. 빈 칸이 있으면 false
    if (can_spawn(state))
        return false;
    // 2. 빈 칸이 없어도, 상하좌우로 합칠 수 있는 숫자가 있으면 false
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            int current = state->board[i][j];
            if (i < 3 && current == state->board[i + 1][j]) {
                return false;
            }
            if (j < 3 && current == state->board[i][j + 1]) {
                return false;
            }
        }
    }
    // 3. 둘 다 아니면 true
    state->game_over = true;
    return true;
}
