#ifndef PROTOCOL_H
#define PROTOCOL_H

// 두 프로그램 간의 모든 네트워크 통신 규약을 정의하는 헤더 파일

// 게임 상태를 나타내는 상수
typedef enum {
    GAME_WAITING,
    GAME_PLAYING,
    GAME_WIN,
    GAME_LOSE,
    GAME_OVER_WAIT  // [NEW] 나는 끝났지만 상대방 기다리는 중
} GameStatus;

//클라이언트의 입력을 나타내는 상수
typedef enum {
    MOVE_UP,
    MOVE_DOWN,
    MOVE_LEFT,
    MOVE_RIGHT,
    QUIT // q를 눌러 종료
} ClientAction;

// C2S 패킷 구조체
typedef struct {
    ClientAction action; // 클라이언트의 행동
} C2S_Packet;

// S2C 패킷 구조체
typedef struct {
    int my_board[4][4];
    int opp_board[4][4];

    int my_score;
    int opp_score;

    GameStatus game_status;
    
    // 공격 큐 정보 (화면 표시용)
    int pending_attacks[10]; 
    int attack_count;        

    // [핵심 추가] 하이라이트 좌표 (공격받은 위치)
    // -1이면 하이라이트 없음
    int highlight_r; 
    int highlight_c;
    // 공격이벤트
    bool is_hit;

} S2C_Packet;

#endif // PROTOCOL_H