# C-based Real-time PvP 2048

리눅스 터미널 환경에서 `Socket`, `Pthread`, `ncurses`를 사용하여 구현한 **실시간 1:1 대전 2048**



## 빌드 방법 (How to Build)

**Ubuntu 24.04** 환경에서 개발되었으며, `ncurses` 라이브러리가 필요

### 1. 필수 라이브러리 설치
컴파일 전에 `ncurses` 라이브러리를 설치
```bash
sudo apt update
sudo apt install libncurses5-dev
```

### 2. 컴파일
`Makefile`을 통해 서버와 클라이언트를 한 번에 빌드 가능
```bash
make
```
* 빌드가 완료되면 `bin/`폴더에 `server`와 `client` 실행 파일이 생성
* 빌드 파일을 삭제하려면 `make clean`을 입력



## 실행 방법 (How to Run)

### 1. 서버 실행 (Server)
포트 번호를 지정하여 서버 열기 (미입력 시 기본 포트 8080으로 지정)
```bash
./bin/server 8080
```

### 2. 클라이언트 실행 (Client)
* IP 미입력시 로컬 테스트용 IP인 **127.0.0.1**으로 지정되며 포트 미입력시 기본 포트 **8080**으로 지정
* 서버와 연결이 되지 않을 시 멀티플레이 모드는 불가능하며 싱글플레이 모드만 가능
```bash
# IP와 포트 모두 지정
./bin/client 127.0.0.1 8080

# IP만 지정 (포트는 8080)
./bin/client 127.0.0.1

# 기본값 사용 (127.0.0.1:8080)
./bin/client
```



## 사용 방법 및 규칙 (Usage & Rules)

### 메인메뉴 (MainMenu)
![MainMenu](https://github.com/riverjune/Mult2048/blob/main/doc/MainMenu.png)

### 조작법 (Controls)
* **이동(move)**: `w`(Up), `a`(Left), `s`(Down), `d`(Right) or Arrow
* **종료(quit)**: `q` or `Q`

### 게임 규칙 (Rules)
1. 기본 룰: 2048 게임과 동일하게 타일을 합쳐 점수를 획득
2. 공격 : 한 번의 이동으로 128점 이상 획득 시, 상대방에게 방해 타일을 전송 (생성한 타일에 따라 최대 4개의 방해 타일 전송)
3. 방어 : 5초 동안 입력이 없거나 다음 조작을 진행할 시 대기 중인 방해 타일이 내 보드에 강제로 생성
4. 승리 조건: 둘 다 게임이 끝났을 때 점수가 더 높은 사람이 승리



## 팀원 정보 (Team)
* **[강유준] (2022112103)**:
  * 서버 아키텍처 및 게임 로직 구현
  * 공격 알고리즘 및 타임아웃(select) 로직 구현
  * 제안서 제작 및 발표
* **[박찬원] (2022110026)**:
  * 클라이언트 UI(ncurses) 디자인 및 구현
  * 비동기 통신 스레드 설계
  * 최종보고서 제작 및 발표
