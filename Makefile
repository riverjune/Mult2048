// How to use this Makefile:
// 1. To build both server and client, simply run:
//      make
// 2. To clean up generated files, run:
//      make clean

#1. 컴파일러 및 플래그 정의
CC = gcc
# CFLAGS = 컴파일 옵션
# -Wall: 모든 경고 메시지 출력
# -Wextra: 추가 경고 메시지 출력
# -std=c23: C23 표준 사용
# -g: 디버깅 정보 포함
# -Iinclude: "include" 폴더를 헤더 파일(#include <...>) 검색 경로에 추가
CFLAGS = -Wall -Wextra -std=c23 -g -Iinclude

#2. 라이브러리 정의 (Libraries)
# LDFLAGS = 링킹 옵션
# -lncurses: ncurses 라이브러리
# -lpthread: pthread (스레드) 라이브러리
# -lm: math 라이브러리 (필요할 수도?)
CLIENT_LIBS = -lncurses -lpthread
SERVER_LIBS = -lpthread -lm

# 3. 디렉토리 정의 (Directories)
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# 4. 소스 파일 및 오브젝트 파일 정의 (Sources & Objects)
# 서버 소스 및 오브젝트
SERVER_SRC = $(SRC_DIR)/server.c $(SRC_DIR)/game_logic.c
SERVER_OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SERVER_SRC))

# 클라이언트 소스 및 오브젝트
CLIENT_SRC = $(SRC_DIR)/client.c
CLIENT_OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(CLIENT_SRC))

# 5. 실행 파일 정의 (Executables)
SERVER_EXEC = $(BIN_DIR)/server
CLIENT_EXEC = $(BIN_DIR)/client

# 6. '가짜' 타겟 정의 (.PHONY)
# clean, all처럼 실제 파일 이름이 아닌 '명령'을 정의합니다.
.PHONY: all clean

# 7. 핵심 규칙 (Rules)

# "make" 또는 "make all"을 입력하면 실행되는 기본 규칙
all: $(BIN_DIR) $(OBJ_DIR) $(SERVER_EXEC) $(CLIENT_EXEC)

# 서버 실행 파일을 만드는 규칙
$(SERVER_EXEC): $(SERVER_OBJ)
	@echo "Linking Server..."
	@$(CC) $(CFLAGS) -o $@ $^ $(SERVER_LIBS)
	@echo "Server build complete!"

# 클라이언트 실행 파일을 만드는 규칙
$(CLIENT_EXEC): $(CLIENT_OBJ)
	@echo "Linking Client..."
	@$(CC) $(CFLAGS) -o $@ $^ $(CLIENT_LIBS)
	@echo "Client build complete!"

# 오브젝트 파일(.o)을 만드는 '패턴 규칙' (가장 중요)
# "obj/%.o" 파일을 만들기 위해 "src/%.c" 파일을 찾습니다.
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

# 필요한 디렉토리가 없으면 생성하는 규칙
$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

# "make clean"을 입력하면 실행되는 정리 규칙
clean:
	@echo "Cleaning up..."
	@rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "Done."