#include "csapp.h" // robust I/O 함수들과 네트워크 래퍼 함수들이 정의된 헤더

int main(int argc, char **argv) {
    int clientfd;               // 서버와 연결된 클라이언트 소켓 파일 디스크립터
    char *host, *port;          // 명령줄 인자로 받은 호스트 이름과 포트 번호
    char buf[MAXLINE];          // 메시지를 저장할 버퍼
    rio_t rio;                  // robust I/O용 입력 버퍼 구조체

    // 인자 개수 체크: 실행 파일명 + host + port → 총 3개여야 함
    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]); // 사용법 안내
        exit(0);
    }

    // 명령줄 인자에서 호스트와 포트 정보를 가져옴
    host = argv[1];
    port = argv[2];

    // 서버에 연결하는 소켓을 생성하고, 연결 시도
    clientfd = Open_clientfd(host, port);

    // robust I/O를 사용하기 위해 rio 구조체를 초기화
    Rio_readinitb(&rio, clientfd);

    // 사용자로부터 한 줄씩 입력을 받고 서버와 통신하는 반복 루프
    while (Fgets(buf, MAXLINE, stdin) != NULL) {
        // 입력한 내용을 서버로 전송
        Rio_writen(clientfd, buf, strlen(buf));

        // 서버로부터 응답을 줄 단위로 수신
        Rio_readlineb(&rio, buf, MAXLINE);

        // 받은 응답을 화면에 출력
        Fputs(buf, stdout);
    }

    // 서버와의 연결 종료
    Close(clientfd);
    return 0;
}