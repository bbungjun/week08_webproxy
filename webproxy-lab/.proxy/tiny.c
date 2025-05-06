/* $begin tinymain */
/*
 * tiny.c - 간단한 반복적 HTTP/1.0 웹 서버로 GET 메소드를 사용하여
 *     정적 및 동적 콘텐츠를 제공합니다.
 *
 * 2019년 11월 업데이트 (droh)
 *   - serve_static() 및 clienterror()에서 sprintf() 별칭 문제 수정
 */
#include "csapp.h" // CS:APP 교재의 라이브러리 포함

// 함수 프로토타입 선언
void doit(int fd); // HTTP 요청/응답 트랜잭션 처리
void read_requesthdrs(rio_t *rp); // HTTP 요청 헤더 읽기
int parse_uri(char *uri, char *filename, char *cgiargs); // URI 파싱
void serve_static(int fd, char *filename, int filesize); // 정적 콘텐츠 제공
void get_filetype(char *filename, char *filetype); // 파일 타입 결정
void serve_dynamic(int fd, char *filename, char *cgiargs); // 동적 콘텐츠 제공
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg); // 클라이언트에게 오류 메시지 반환

int main(int argc, char **argv)
{
  int listenfd, connfd; // 리스닝 소켓과 연결 소켓 디스크립터
  char hostname[MAXLINE], port[MAXLINE]; // 클라이언트 호스트명과 포트 저장 버퍼
  socklen_t clientlen; // 클라이언트 주소 구조체 크기
  struct sockaddr_storage clientaddr; // 클라이언트 주소 구조체

  /* 명령줄 인자 확인 */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]); // 사용법 출력
    exit(1); // 오류 코드 1로 종료
  }

  listenfd = Open_listenfd(argv[1]); // 지정된 포트에서 리스닝 소켓 생성
  while (1) // 무한 루프로 계속 연결 대기
  {
    clientlen = sizeof(clientaddr); // 클라이언트 주소 구조체 크기 설정
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen); // 클라이언트 연결 수락
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0); // 클라이언트 주소를 호스트명과 포트번호로 변환
    printf("Accepted connection from (%s, %s)\n", hostname, port); // 연결 수락 메시지 출력
    doit(connfd);  // HTTP 트랜잭션 처리
    Close(connfd); // 연결 종료
  }
}

/*
 * doit - 하나의 HTTP 요청/응답 트랜잭션 처리
 */
void doit(int fd)
{
  int is_static; // 정적 콘텐츠인지 여부 (1=정적, 0=동적)
  struct stat sbuf; // 파일 상태 정보 구조체
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; // 요청 라인 파싱용 버퍼
  char filename[MAXLINE], cgiargs[MAXLINE]; // 파일명과 CGI 인자 저장 버퍼
  rio_t rio; // 견고한 I/O 패키지 구조체

  /* 요청 라인과 헤더 읽기 */
  Rio_readinitb(&rio, fd); // rio 버퍼 초기화
  if (!Rio_readlineb(&rio, buf, MAXLINE)) // 요청 라인 읽기 실패 시
    return; // 함수 종료
  printf("%s", buf); // 요청 라인 출력
  sscanf(buf, "%s %s %s", method, uri, version); // 요청 라인 파싱
  if (strcasecmp(method, "GET")) // GET 메소드가 아닌 경우
  {
    clienterror(fd, method, "501", "Not Implemented",
                "Tiny does not implement this method"); // 에러 응답 반환
    return; // 함수 종료
  }
  read_requesthdrs(&rio); // HTTP 요청 헤더 읽기

  /* GET 요청에서 URI 파싱 */
  is_static = parse_uri(uri, filename, cgiargs); // URI 파싱하여 파일명과 CGI 인자 추출
  if (stat(filename, &sbuf) < 0) // 파일 정보 획득 실패 시
  {
    clienterror(fd, filename, "404", "Not found",
                "Tiny couldn't find this file"); // 404 에러 응답
    return; // 함수 종료
  }

  if (is_static) // 정적 콘텐츠인 경우
  { /* 정적 콘텐츠 제공 */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) // 정규 파일이 아니거나 읽기 권한이 없는 경우
    {
      clienterror(fd, filename, "403", "Forbidden",
                  "Tiny couldn't read the file"); // 403 에러 응답
      return; // 함수 종료
    }
    serve_static(fd, filename, sbuf.st_size); // 정적 콘텐츠 제공
  }
  else // 동적 콘텐츠인 경우
  { /* 동적 콘텐츠 제공 */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) // 정규 파일이 아니거나 실행 권한이 없는 경우
    {
      clienterror(fd, filename, "403", "Forbidden",
                  "Tiny couldn't run the CGI program"); // 403 에러 응답
      return; // 함수 종료
    }
    serve_dynamic(fd, filename, cgiargs); // 동적 콘텐츠 제공
  }
}

/*
 * read_requesthdrs - HTTP 요청 헤더 읽기
 */
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE]; // 헤더 라인 저장 버퍼

  Rio_readlineb(rp, buf, MAXLINE); // 첫 번째 헤더 라인 읽기
  printf("%s", buf); // 헤더 라인 출력
  while (strcmp(buf, "\r\n")) // 빈 줄(헤더 종료 표시)이 아닌 동안
  {
    Rio_readlineb(rp, buf, MAXLINE); // 다음 헤더 라인 읽기
    printf("%s", buf); // 헤더 라인 출력
  }
  return; // 함수 종료
}

/*
 * parse_uri - URI를 파일명과 CGI 인자로 파싱
 *             동적 콘텐츠면 0, 정적 콘텐츠면 1 반환
 */
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr; // 문자열 포인터

  if (!strstr(uri, "cgi-bin")) // URI에 "cgi-bin"이 없으면 정적 콘텐츠
  { /* 정적 콘텐츠 */
    strcpy(cgiargs, ""); // CGI 인자 비움
    strcpy(filename, "."); // 현재 디렉토리 시작
    strcat(filename, uri); // URI를 파일명에 추가
    if (uri[strlen(uri) - 1] == '/') // URI가 '/'로 끝나면
      strcat(filename, "home.html"); // 기본 파일명 추가
    return 1; // 정적 콘텐츠 표시
  }
  else // URI에 "cgi-bin"이 있으면 동적 콘텐츠
  { /* 동적 콘텐츠 */
    ptr = index(uri, '?'); // '?' 문자 위치 찾기
    if (ptr) // '?' 문자가 있으면
    {
      strcpy(cgiargs, ptr + 1); // '?' 이후 문자열을 CGI 인자로 복사
      *ptr = '\0'; // URI에서 '?' 이후 부분 제거
    }
    else // '?' 문자가 없으면
      strcpy(cgiargs, ""); // CGI 인자 비움
    strcpy(filename, "."); // 현재 디렉토리 시작
    strcat(filename, uri); // URI를 파일명에 추가
    return 0; // 동적 콘텐츠 표시
  }
}

/*
 * serve_static - 파일을 클라이언트에게 전송
 */
void serve_static(int fd, char *filename, int filesize)
{
  int srcfd; // 소스 파일 디스크립터
  char *srcp, filetype[MAXLINE]; // 메모리 매핑 포인터와 파일 타입 버퍼

  char buf[MAXBUF]; // 응답 헤더 버퍼
  char *p = buf; // 버퍼 포인터
  int n; // 쓰기 바이트 수
  int remaining = sizeof(buf); // 남은 버퍼 크기

  /* 파일 타입 결정 */
  get_filetype(filename, filetype); // 파일명으로부터 MIME 타입 결정

  /* HTTP 응답 헤더를 올바르게 구성 - 별도 버퍼 사용 또는 추가 */
  n = snprintf(p, remaining, "HTTP/1.0 200 OK\r\n"); // 상태 라인 추가
  p += n; // 버퍼 포인터 이동
  remaining -= n; // 남은 버퍼 크기 업데이트

  n = snprintf(p, remaining, "Server: Tiny Web Server\r\n"); // 서버 헤더 추가
  p += n; // 버퍼 포인터 이동
  remaining -= n; // 남은 버퍼 크기 업데이트

  n = snprintf(p, remaining, "Connection: close\r\n"); // 연결 헤더 추가
  p += n; // 버퍼 포인터 이동
  remaining -= n; // 남은 버퍼 크기 업데이트

  n = snprintf(p, remaining, "Content-length: %d\r\n", filesize); // 콘텐츠 길이 헤더 추가
  p += n; // 버퍼 포인터 이동
  remaining -= n; // 남은 버퍼 크기 업데이트

  n = snprintf(p, remaining, "Content-type: %s\r\n\r\n", filetype); // 콘텐츠 타입 헤더와 빈 줄 추가
  p += n; // 버퍼 포인터 이동
  remaining -= n; // 남은 버퍼 크기 업데이트

  Rio_writen(fd, buf, strlen(buf)); // 응답 헤더 전송
  printf("Response headers:\n"); // 응답 헤더 출력 메시지
  printf("%s", buf); // 응답 헤더 내용 출력

  /* 응답 본문 전송 */
  srcfd = Open(filename, O_RDONLY, 0); // 요청 파일 열기
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 파일을 메모리에 매핑
  Close(srcfd); // 파일 디스크립터 닫기
  Rio_writen(fd, srcp, filesize); // 메모리 매핑된 파일 내용 전송
  Munmap(srcp, filesize); // 메모리 매핑 해제
}

/*
 * get_filetype - 파일명으로부터 파일 타입 유추
 */
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html")) // HTML 파일인 경우
    strcpy(filetype, "text/html"); // HTML MIME 타입
  else if (strstr(filename, ".gif")) // GIF 이미지인 경우
    strcpy(filetype, "image/gif"); // GIF MIME 타입
  else if (strstr(filename, ".png")) // PNG 이미지인 경우
    strcpy(filetype, "image/png"); // PNG MIME 타입
  else if (strstr(filename, ".jpg")) // JPEG 이미지인 경우
    strcpy(filetype, "image/jpeg"); // JPEG MIME 타입
  else // 기타 파일 타입
    strcpy(filetype, "text/plain"); // 일반 텍스트 MIME 타입
}

/*
 * serve_dynamic - 클라이언트를 대신하여 CGI 프로그램 실행
 */
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = {NULL}; // 버퍼와 빈 인자 리스트
  pid_t pid; // 프로세스 ID

  /* HTTP 응답의 첫 부분 반환 */
  sprintf(buf, "HTTP/1.0 200 OK\r\n"); // 상태 라인 생성
  Rio_writen(fd, buf, strlen(buf)); // 상태 라인 전송
  sprintf(buf, "Server: Tiny Web Server\r\n"); // 서버 헤더 생성
  Rio_writen(fd, buf, strlen(buf)); // 서버 헤더 전송

  /* CGI 프로그램을 처리할 자식 프로세스 생성 */
  if ((pid = Fork()) < 0) // 포크 실패 시
  { /* 포크 실패 */
    perror("Fork failed"); // 오류 메시지 출력
    return; // 함수 종료
  }

  if (pid == 0) // 자식 프로세스인 경우
  { /* 자식 프로세스 */
    /* 실제 서버는 여기서 모든 CGI 변수를 설정 */
    setenv("QUERY_STRING", cgiargs, 1); // QUERY_STRING 환경 변수 설정

    /* 표준 출력을 클라이언트로 리다이렉션 */
    if (Dup2(fd, STDOUT_FILENO) < 0) // 표준 출력 리다이렉션 실패 시
    {
      perror("Dup2 error"); // 오류 메시지 출력
      exit(1); // 오류 코드 1로 종료
    }
    Close(fd); // 원본 파일 디스크립터 닫기

    /* CGI 프로그램 실행 */
    Execve(filename, emptylist, environ); // CGI 프로그램 실행

    /* 여기에 도달하면 Execve 실패 */
    perror("Execve error"); // 오류 메시지 출력
    exit(1); // 오류 코드 1로 종료
  }
  else // 부모 프로세스인 경우
  { /* 부모 프로세스 */
    /* 부모는 자식이 종료될 때까지 대기 */
    int status; // 자식 프로세스 종료 상태
    if (waitpid(pid, &status, 0) < 0) // 자식 프로세스 대기 실패 시
    {
      perror("Wait error"); // 오류 메시지 출력
    }

    printf("Child process %d terminated with status %d\n", pid, status); // 자식 프로세스 종료 정보 출력
    /* 부모는 정상적으로 계속 진행 - doit()으로 복귀 */
  }
  /* 여기서 반환하면 doit()이 연결을 닫음 */
}

/*
 * clienterror - 클라이언트에게 오류 메시지 반환
 */
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF]; // 응답 헤더와 본문 버퍼

  /* HTTP 응답 본문 구성 */
  sprintf(body, "<html><title>Tiny Error</title>"); // HTML 시작과 제목
  sprintf(body, "%s<body bgcolor="
                "ffffff"
                ">\r\n",
          body); // 본문 시작과 배경색 설정
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg); // 오류 번호와 간단한 메시지
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause); // 상세 오류 메시지와 원인
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body); // 구분선과 서버 서명

  /* HTTP 응답 출력 */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg); // 상태 라인
  Rio_writen(fd, buf, strlen(buf)); // 상태 라인 전송
  sprintf(buf, "Content-type: text/html\r\n"); // 콘텐츠 타입 헤더
  Rio_writen(fd, buf, strlen(buf)); // 콘텐츠 타입 헤더 전송
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body)); // 콘텐츠 길이 헤더와 빈 줄
  Rio_writen(fd, buf, strlen(buf)); // 콘텐츠 길이 헤더 전송
  Rio_writen(fd, body, strlen(body)); // 응답 본문 전송
}