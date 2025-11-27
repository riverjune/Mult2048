#include "game.h"
#include <stdlib.h> // rand(), srand()
#include <string.h> // memset(), memcpy()
#include <time.h>   // time()

// ==========================================
// [1] 내부 헬퍼 함수 (Internal Helper Functions)
// ==========================================

/**
 * @brief 4칸짜리 1차원 배열을 "왼쪽"으로 밀고 합치는 핵심 로직
 * @return 이번 줄에서 합쳐져서 획득한 점수
 */
static int process_line(int *line) {
    int score = 0;
    int temp[4] = {0};
    int temp_idx = 0;

    // 1. [Slide] 0이 아닌 숫자들을 앞으로 당김
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
            temp[i+1] = 0;     // 뒷자리 비움
            i++;               // 합쳐진 칸 건너뜀
        }
    }

    // 3. [Slide Again] 합쳐진 후 생긴 빈 공간 정리
    int final_idx = 0;
    for (int i = 0; i < 4; i++) {
        if (temp[i] != 0) {
            line[final_idx++] = temp[i];
        }
    }
    // 남은 뒷부분 0 채우기
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
// [2] 공개 함수 구현 (Public API)
// ==========================================

void game_init(GameState *state) {
    memset(state, 0, sizeof(GameState));
    
    // 주의: srand는 서버 main에서 한 번만 호출하는 것이 좋음
    // srand(time(NULL)); 

    game_spawn_tile(state);
    game_spawn_tile(state);

    state->game_over = false;
    state->moved = false;
    
    // 하이라이트 초기화
    state->highlight_r = -1;
    state->highlight_c = -1;
}

void game_spawn_tile(GameState *state) {
    if (!can_spawn(state)) return;
    
    // 10% 확률로 4, 90% 확률로 2
    int value = (rand() % 10 == 0) ? 4 : 2; 
    
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
    int temp_line[4]; 
    
    // 이동 전 상태 백업
    int old_board[4][4]; 
    memcpy(old_board, state->board, sizeof(old_board));
    
    // 하이라이트 리셋 (이동하면 사라짐)
    state->highlight_r = -1;
    state->highlight_c = -1;
    
    for (int i = 0; i < 4; i++) {
        // 1. 방향에 따라 한 줄 추출
        for (int j = 0; j < 4; j++) {
            switch(dir) {
                case UP:    temp_line[j] = state->board[j][i];     break;
                case DOWN:  temp_line[j] = state->board[3-j][i];   break;
                case LEFT:  temp_line[j] = state->board[i][j];     break;
                case RIGHT: temp_line[j] = state->board[i][3-j];   break;
            }
        }
        
        // 2. 핵심 로직 실행
        total_score += process_line(temp_line);
        
        // 3. 다시 보드에 복사
        for (int j = 0; j < 4; j++) {
            switch(dir) {
                case UP:    state->board[j][i]     = temp_line[j]; break;
                case DOWN:  state->board[3-j][i]   = temp_line[j]; break;
                case LEFT:  state->board[i][j]     = temp_line[j]; break;
                case RIGHT: state->board[i][3-j]   = temp_line[j]; break;
            }
        }
    }

    // 이동 여부 확인
    state->moved = false;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (old_board[i][j] != state->board[i][j]) {
                state->moved = true;
                goto check_end; // 2중 루프 탈출
            }
        }
    }
check_end:
    
    state->score += total_score;
    return total_score;
}

// [PvP] 공격 예약
void game_queue_attack(GameState *state, int value) {
    if (state->attack_cnt < 10) {
        state->attack_queue[state->attack_cnt++] = value;
    }
}

// [PvP] 공격 실행 및 하이라이트
void game_execute_attack(GameState *state) {
    // 1. 하이라이트 초기화
    state->highlight_r = -1;
    state->highlight_c = -1;

    // 2. 큐 확인
    if (state->attack_cnt <= 0) return;

    int attack_value = state->attack_queue[0];
    
    // 3. 공격 가능한 위치(빈칸 0 또는 값이 2인 칸) 찾기
    typedef struct { int r; int c; } Pos;
    Pos targets[16];
    int target_cnt = 0;

    for(int i=0; i<4; i++) {
        for(int j=0; j<4; j++) {
            // 빈칸이거나, 공격 타일이 2일 때 기존 타일이 2면 합체 가능
            // (여기서는 공격 타일 값이 2라고 가정)
            if (state->board[i][j] == 0 || state->board[i][j] == attack_value) {
                targets[target_cnt].r = i;
                targets[target_cnt].c = j;
                target_cnt++;
            }
        }
    }

    // 공격할 곳이 없으면 취소
    if (target_cnt == 0) return;

    // 4. 큐에서 꺼내기 (Shift Left)
    int attack_value = state->attack_queue[0];
    for (int i = 0; i < state->attack_cnt - 1; i++) {
        state->attack_queue[i] = state->attack_queue[i + 1];
    }
    state->attack_cnt--;
    state->attack_queue[state->attack_cnt] = 0;

    // 5. 랜덤 위치에 공격 적용
    int idx = rand() % target_cnt;
    int r = targets[idx].r;
    int c = targets[idx].c;

    if (state->board[r][c] == 0) {
        state->board[r][c] = attack_value; // 빈칸에 생성
    } else {
        state->board[r][c] += attack_value; // 합체 (2+2=4)
    }

    // 6. 하이라이트 설정
    state->highlight_r = r;
    state->highlight_c = c;
}

bool game_is_over(GameState *state) {
    // 1. 빈 칸이 있으면 false
    if (can_spawn(state)) return false;
    
    // 2. 인접한 같은 숫자 있으면 false
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            int current = state->board[i][j];
            // 오른쪽 확인
            if (j < 3 && current == state->board[i][j+1]) return false;
            // 아래쪽 확인
            if (i < 3 && current == state->board[i+1][j]) return false;
        }
    }
    
    // 3. 둘 다 아니면 true
    state->game_over = true;
    return true;
}