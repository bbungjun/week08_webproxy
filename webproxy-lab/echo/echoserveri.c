#include "csapp.h" // robust I/O 함수와 네트워크 함수 래퍼가 포함된 헤더 파일

int main(int argc, char **argv) {
    int listenfd, connfd;                       // 서버 리슨용 소켓과 클라이언트 연결용 소켓
    socklen_t clientlen;                        // 클라이언트 주소 구조체의 크기
    struct sockaddr_storage clientaddr;         // 클라이언트 주소 정보를 저장할 구조체
    char client_hostname[MAXLINE];              // 클라이언트의 호스트 이름 문자열
    char client_port[MAXLINE];                  // 클라이언트의 포트 번호 문자열

    // 명령줄 인자가 2개가 아니면 사용법 출력 후 종료
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]); // 실행 방법 안내
        exit(0);
    }

    // 포트 번호를 인자로 받아 서버 리슨 소켓 열기
    listenfd = Open_listenfd(argv[1]);

    while (1) { // 반복형 서버: 클라이언트가 연결 올 때마다 반복
        clientlen = sizeof(struct sockaddr_storage); // 주소 구조체 크기 초기화

        // 클라이언트의 연결 요청 수락 → 새로운 소켓 connfd 생성
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        // 클라이언트 주소를 사람이 읽을 수 있는 호스트/포트 문자열로 변환
        Getnameinfo((SA *)&clientaddr, clientlen,
                    client_hostname, MAXLINE,
                    client_port, MAXLINE, 0);

        // 접속한 클라이언트의 정보 출력
        printf("connected to (%s, %s)\n", client_hostname, client_port);

        // 연결된 소켓을 echo 처리 함수로 넘겨 응답 처리
        echo(connfd);

        // 클라이언트와의 연결 종료
        Close(connfd);
    }
}