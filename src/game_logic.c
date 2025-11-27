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

    // 하이라이트 초기화
    state->highlight_r = -1;
    state->highlight_c = -1;
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

// [핵심 수정] 공격 실행 및 하이라이트 좌표 설정
void game_execute_attack(GameState *state) {
    // 1. 하이라이트 초기화 (이번 턴에 공격 없으면 -1)
    state->highlight_r = -1;
    state->highlight_c = -1;

    // 2. 큐 확인
    if (state->attack_cnt <= 0) return;

    int attack_value = state->attack_queue[0];
    
    // 3. 공격 가능한 위치 찾기 (빈칸 0 또는 값이 2인 칸)
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

    // 공격할 공간이 전혀 없으면 공격 취소 (또는 보류)
    if (target_cnt == 0) return;

    // 4. 큐에서 꺼내기
    int attack_value = state->attack_queue[0];
    for (int i = 0; i < state->attack_cnt - 1; i++) {
        state->attack_queue[i] = state->attack_queue[i + 1];
    }
    state->attack_cnt--;

    // 5. 랜덤 위치 선정 및 공격 적용
    int idx = rand() % target_cnt;
    int r = targets[idx].r;
    int c = targets[idx].c;

    if (state->board[r][c] == 0) {
        state->board[r][c] = attack_value; // 빈칸 생성
    } else {
        state->board[r][c] += attack_value; // 2+2=4 합체!
    }

    // [핵심] 하이라이트 좌표 설정
    state->highlight_r = r;
    state->highlight_c = c;
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

// game_logic.c (추가 함수)

/**
 * @brief 상대 보드에 공격 타일(2)을 즉시 생성하고 그 위치를 반환
 * @param state 공격받는 플레이어의 게임 상태
 * @param out_r 생성된 타일의 행 인덱스 (출력용)
 * @param out_c 생성된 타일의 열 인덱스 (출력용)
 * @return 타일 생성 성공 여부 (bool)
 */
// game_logic.c (수정된 game_spawn_attack_tile 함수)

// 임시 좌표 구조체
typedef struct { int r; int c; } Coord;

/**
 * @brief 상대 보드에 공격 타일(2)을 즉시 생성/합체하고 그 위치를 반환
 * * 빈 칸(0) 또는 이미 2가 있는 칸을 찾아 타일을 배치합니다.
 * 2에 배치 시 즉시 4로 합체됩니다.
 * * @param state 공격받는 플레이어의 게임 상태
 * @param out_r 생성/합체된 타일의 행 인덱스 (출력용)
 * @param out_c 생성/합체된 타일의 열 인덱스 (출력용)
 * @return 타일 생성/합체 성공 여부 (bool)
 */
bool game_spawn_attack_tile(GameState *state, int* out_r, int* out_c) {
    // 1. 공격이 가능한 모든 위치(0 또는 2)를 찾습니다.
    Coord eligible_spots[16];
    int spot_count = 0;

    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 4; j++) {
            int current_val = state->board[i][j];
            // 빈 칸(0)이거나, 2에 덮어쓰기(합체)가 가능한 경우
            if (current_val == 0 || current_val == 2) {
                eligible_spots[spot_count].r = i;
                eligible_spots[spot_count].c = j;
                spot_count++;
            }
        }
    }

    // 2. 공격할 곳이 없으면 실패 (보드가 4 이상으로 가득 찬 경우)
    if (spot_count == 0) {
        *out_r = -1;
        *out_c = -1;
        return false;
    }

    // 3. 찾은 위치들 중 하나를 무작위로 선택
    int rand_idx = rand() % spot_count;
    int r = eligible_spots[rand_idx].r;
    int c = eligible_spots[rand_idx].c;

    // 4. 배치 및 합체 실행
    if (state->board[r][c] == 2) {
        // 이미 2가 있으면 즉시 합체 (2 + 2 = 4)
        state->board[r][c] = 4;
    } else {
        // 빈 칸(0)이면 2 생성
        state->board[r][c] = 2;
    }
    
    // 5. 결과 좌표 반환
    *out_r = r;
    *out_c = c;
    return true;
}