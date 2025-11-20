//네트워크 연결 없이 로직 확인용 함수
//gcc -o test_game src/test_game.c src/game_logic.c -Iinclude
#include <stdio.h>
#include "game.h"

// 보드를 예쁘게 출력하는 도우미 함수
void print_board(GameState *state) {
    printf("-----------------------------\n");
    printf("Score: %d\n", state->score);
    printf("-----------------------------\n");
    for (int i = 0; i < 4; i++) {
        printf("|");
        for (int j = 0; j < 4; j++) {
            if (state->board[i][j] == 0)
                printf("      |"); // 빈칸
            else
                printf(" %4d |", state->board[i][j]); // 숫자
        }
        printf("\n-----------------------------\n");
    }
    printf("\n");
}

int main() {
    GameState state;

    printf("=== 1. 게임 초기화 테스트 ===\n");
    game_init(&state);
    print_board(&state);

    printf("=== 2. 위(UP)로 이동 테스트 ===\n");
    // 강제로 테스트하기 쉬운 상황 설정
    state.board[0][0] = 2;
    state.board[1][0] = 2;
    state.board[2][0] = 4;
    state.board[3][0] = 8;
    printf("[이동 전]\n");
    print_board(&state);

    game_move(&state, 0); // 0: UP (game.h의 enum이나 define에 따라 다름)
    // game_logic.c의 UP 케이스가 잘 작동한다면:
    // [0][0]은 4가 되고, [1][0]은 4, [2][0]은 8, [3][0]은 0(또는 랜덤생성)이 되어야 함

    printf("[이동 후 (UP)]\n");
    print_board(&state);
    
    if (state.moved) printf(">> 이동 감지됨 (OK)\n");
    else printf(">> 이동 감지 안됨 (FAIL)\n");

    game_spawn_tile(&state);
    printf("[랜덤 타일 생성 후]\n");
    print_board(&state);

    return 0;
}