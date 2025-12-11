#ifndef GAME_H
#define GAME_H

#include <stdbool.h> // for bool type

// 2048 게임의 순수 로직 정의
// server.c는 이 헤더를 include 하여 게임 로직 함수를 호출
// game_logic.c는 이 헤더를 include 하여 함수들을 구현

// 게임 보드와 상태를 담는 구조체, 서버 내부 로직용이므로 protocol.hdml S2C_Packet는 별개
typedef struct {
    int board[4][4]; // 4x4 게임 보드
    int score;       // 현재 점수
    bool game_over;  // 게임 오버 여부
    bool moved;      // 마지막 이동에서 타일이 움직였는지 여부
    int attack_queue[10]; // 최대 10개의 공격 대기열
    int attack_cnt;       // 현재 대기열에 있는 공격 수

    // 이번 턴에 공격받은 위치 저장
    int highlight_r; 
    int highlight_c;
} GameState;

// 방향 상수 
typedef enum {
    UP,
    DOWN,
    LEFT,
    RIGHT
} Direction;

// 핵심 함수 선언부, 실제 구현은 game_logic.c에 있음

/**
 * @brief 게임 상태를(보드, 점수 등) 초기화
 * 보통 2개의 '2'타일을 무작위 위치에 배치
 * @param state 게임 상태 구조체의 포인터
 */
void game_init(GameState* state);

/**
 * @brief 사용자의 입력에 따라 보드의 타일을 이동하고 합침
 * 프로젝트의 뇌에 해당하는 가장 복잡한 함수
 * @param state 게임 상태 구조체의 포인터
 * @param dir 이동 방향 (UP, DOWN, LEFT, RIGHT)
 * @return 타일이 합쳐져서 발생한 점수 (int)
 */
int game_move(GameState* state, Direction dir);

/**
 * @brief 보드의 빈 칸에 새로운 타일 (2 또는 4)를 무작위로 추가
 * @param state 게임 상태 구조체의 포인터
 */
void game_spawn_tile(GameState* state);

// 큐에 공격 추가
void game_queue_attack(GameState *state, int value);
// 큐에 있는 공격 실행 (위치 정보 업데이트 포함)
void game_execute_attack(GameState *state);

/**
 * @brief 게임이 끝났는지 (더 이상 이동할 수 없는지) 확인
 * @param state 게임 상태 구조체의 포인터
 * @return 게임 오버 여부 (bool)
 */
bool game_is_over(GameState* state);  

#endif // GAME_H