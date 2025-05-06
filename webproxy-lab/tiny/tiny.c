#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void sigchld_handler(int sig) {
  while (waitpid(-1, NULL, WNOHANG) > 0);
}
// main()
// 1. 리스닝 소켓 생성
// 2. 무한 루프로 클라이언트 연결 수락
// 3. 연결된 소켓에 대해 doit()으로 요청 처리
// 4. 요청 후 소켓 닫기
int main(int argc, char **argv)  // argc : 인자 개수, argv : 인자 배열
{
  Signal(SIGCHLD, sigchld_handler); // SIGCHLD 시그널 핸들러 등록 (자식 프로세스 종료 시 부모 프로세스가 wait() 호출) // -> 11.8 숙제 문제
  int listenfd, connfd; // 리스닝 소켓, 연결 소켓
  char hostname[MAXLINE], port[MAXLINE]; // 주소, 포트 문자열
  socklen_t clientlen; // 주소 구조체의 크기
  struct sockaddr_storage clientaddr; // 연결한 클라이언트의 주소 정보

  if (argc != 2) // 인자가 2개가 아니면 오류 출력 (./tiny | 8080)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); // 포트 번호로 리스닝 소켓 생성 [soket(), bind(), listen() 생성]

  while (1) // 무한 루프 : 클라이언트 요청 계속 받음
  {
    clientlen = sizeof(clientaddr); // Accept() 호출 전에 주소 구조체 크기 지정
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // 클라이언트 연결 수락 - 새로운 연결이 들어오면 connfd 소켓 생성
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0); // 연결된 Client의 IP와 PORT를 사람이 읽을 수 있는 문자열로 변환
    printf("Accepted connection from (%s, %s)\n", hostname, port); // 연결된 Client 정보 출력
    doit(connfd); // HTTP 요청을 처리 함수 호출
    Close(connfd); // 연결 소켓 종료
  }
}

void doit(int fd)
{
  int is_static; // 요청이 정적(1), 동적(0)인지 구분
  struct stat sbuf; // stat() 호출 결과를 담는 파일 정보 구조체
  char buf[MAXLINE]; // 요청 한 줄 읽을 임시 저장소
  char method[MAXLINE], uri[MAXLINE], version[MAXLINE]; // 요청 라인 파싱 결과 저장
  char filename[MAXLINE], cgiargs[MAXLINE]; 
  // filename : 요청된 실제 파일 경로, cgiargs : CGI 인자 
  rio_t rio; // robust I/O용 구조체

  Rio_readinitb(&rio, fd); // rio를 소켓 fd에 연결
  Rio_readlineb(&rio, buf, MAXLINE); // 요청 라인 읽음
  printf("Request headers:\n"); // -> 11.6C 숙제 문제
  printf("%s", buf); // 로그 출력

  sscanf(buf, "%s %s %s", method, uri, version); // 요청 라인에서 메서드(GET), URI(/index.html), HTTP 버전 분리


  
  if (!(strcasecmp(method, "GET") == 0 || strcasecmp(method, "HEAD") == 0)) { // GET or HEAD 허용   
    clienterror(fd, method, "501", "Not implemented",  // 그 외는 501 Not Implemented 반환
                "Tiny does not implement this method");
    return;
  }

  read_requesthdrs(&rio); // 나머지 헤더들 모두 읽기만 하고 무시

  is_static = parse_uri(uri, filename, cgiargs); // URI 분석, cgi-bin 포함되어 있으면 동적, 아니면 정적

  if(stat(filename, &sbuf) < 0) { // 요청한 파일이 없으면 404
    clienterror(fd, filename, "404", "Not found",
                 "Tiny couldn't find this file");
    return;
  }

  if(is_static) { 
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { // 일반 파일인지(S_ISREG), 읽기 권한 있는지(S_IRUSR) 확인
      clienterror(fd, filename, "403", "Forbidden", 
                   "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size, method); 
  }
  else { 
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { // 일반 파일인지(S_ISREG), 실행 권한 있는지(S_IXUSR) 확인
      clienterror(fd, filename, "403", "Forbidden",
                   "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs); // serve_dynamic()으로 CGI 프로그램 실행
  }
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) // 클라이언트에게 에러가 발생했음을 HTML로 알려줌
{
  char buf[MAXLINE], body[MAXBUF]; // buf : HTTP 헤더, body : HTML 에러 메시지

  sprintf(body, "<html><title>Tiny Error</title>"); 
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body); 
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg); 
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause); 
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg); 
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));

  Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp) // 클라이언트가 보낸 HTTP 요청 헤더를 읽음
{
  char buf[MAXLINE];
  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n")) { 
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;
  if(!strstr(uri, "cgi-bin")) { // cgi-bin이 없으면 정적 컨텐츠 처리
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    if(uri[strlen(uri)-1] == '/') 
      strcat(filename, "home.html");
    return 1;
  }
  else { // cgi-bin이 있으면 동적 컨텐츠 처리(CGI)
    ptr = index(uri, '?');
    if(ptr){
      strcpy(cgiargs, ptr+1);
      *ptr = '\0';
    }
    else
    {
        strcpy(cgiargs, "");
    }
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

// 1. 파일 타입 추론 (MIME type)
// 2. 응답 헤더 생성 및 전송
// 3. method가 GET이면 → 파일 열고 → 내용 전송
// 4. method가 HEAD면 → 파일은 전송하지 않음
void serve_static(int fd, char *filename, int filesize, char *method)
{
    int srcfd; // 소스 파일 디스크립터
    char *srcbuf; // 파일 내용 버퍼
    char filetype[MAXLINE], buf[MAXBUF]; // MIME type, 응답 헤더

    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); // 응답 헤더 생성
    sprintf(buf + strlen(buf), "Server: Tiny Web Server\r\n");
    sprintf(buf + strlen(buf), "Connection: close\r\n");
    sprintf(buf + strlen(buf), "Content-length: %d\r\n", filesize);
    sprintf(buf + strlen(buf), "Content-type: %s\r\n\r\n", filetype);
    Rio_writen(fd, buf, strlen(buf));

    if (strcasecmp(method, "HEAD") == 0) // HEAD 요청이면 파일 내용 전송하지 않음
        return;

    // 파일 읽기 -> 클라이언트에게 보내기
    srcfd = Open(filename, O_RDONLY, 0); //파일 열기
    srcbuf = (char *)malloc(filesize);   // 파일 내용 저장할 메모리 할당    -> 11.9 숙제 문제
    Rio_readn(srcfd, srcbuf, filesize);  // 파일 내용 읽기
    Close(srcfd); // 파일 디스크립터 닫기   

    Rio_writen(fd, srcbuf, filesize); // 클라이언트에게 파일 내용 전송
    free(srcbuf); // 메모리 해제
}

void get_filetype(char *filename, char *filetype) //파일이 어떤 종류인지(Content-Type)를 판단
{
  if(strstr(filename, ".html")) 
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif")) 
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png")) 
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg")) 
    strcpy(filetype, "image/jpg");
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");
  else if (strstr(filename, ".mpg") || strstr(filename, ".mpeg")) // -> 11.7 숙제 문제
    strcpy(filetype, "video/mpeg");
  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs) // CGI 프로그램을 실행해서 그 출력 결과를 브라우저로 보내는 역할
{
  char buf[MAXLINE], *emptylist [] = { NULL };

  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if(Fork() == 0) { // 자식 프로세스 생성
    setenv("QUERY_STRING", cgiargs, 1); // 환경변수 설정 (cgiargs : CGI 인자)   // -> 11.10B 숙제 문제
    Dup2(fd, STDOUT_FILENO); // 자식 프로세스의 표준 출력(1)을 fd로 리다이렉트
    Execve(filename, emptylist, environ); // filename을 실행 (CGI 프로그램 실행)
  }
  //Wait(NULL); // 자식 프로세스 종료 대기
}
