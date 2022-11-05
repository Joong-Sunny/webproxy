/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

/*  메인함수  */
int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  listenfd = Open_listenfd(argv[1]); // 듣기 소켓 오픈
  /* 무한 서버 루프 */
  while (1)
  {
    clientlen = sizeof(clientaddr);
    /* 연결 요청 접수 */
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    /* Transaction 수행 */
    printf("나 이제 doit 실행한다...????");
    doit(connfd); // line:netp:tiny:doit
    /* 서버쪽 연결 종료 */
    Close(connfd); // line:netp:tiny:close
    printf("힝... 서버 닫힘 ㅠㅠ");
  }
}

/* doit : 한 개의 HTTP transaction 처리 */
void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  /* Request Line과 헤더를 읽기 */
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  /* GET 메소드 외에는 지원하지 않음 - 다른 요청이 오면 에러 띄우고 종료 */

  if (!strcasecmp(method, "HEAD"))
  {
    read_requesthdrs(&rio);
  }
  else if (!strcasecmp(method, "GET"))
  {
    read_requesthdrs(&rio);
  }
  else
  {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }


//   if (strcasecmp(method, "GET"))
//   {
//     clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
//     return;
//   }
//  read_requesthdrs(&rio);



  /* URI를 parsing 하고, 요청받은 것이 static contents인지 판단하는 플래그 설정 */
  is_static = parse_uri(uri, filename, cgiargs);

  /* 파일이 디스크 상에 있지 않다면 - 에러 띄우고 종료 */
  if (stat(filename, &sbuf) < 0)
  {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn’t find this file");
    return;
  }
  if (is_static)
  { /* static contents를 요청받은 경우 */
    /* regualar file인지, 읽기 권한이 있는지 검사 */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn’t read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size); /* static contents 제공 */
  }
  else
  { /* dynamic contents를 요청받은 경우 */
    /* regualar file인지, 실행 권한이 있는지 검사 */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn’t run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs); /* dynamic contents 제공 */
  }
}

/* read_requesthdrs: 요청 헤더를 읽고 무시하기 */
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while (strcmp(buf, "\r\n"))
  {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

/* parse_uri: HTTP URI를 분석하기 */
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;
  /* 실행파일의 홈 디렉토리 : cgi-bin */
  /* cgi-bin을 포함하는 uri는 dynamic contents 요청임을 가정 */
  if (!strstr(uri, "cgi-bin"))
  {                      /* Static content */
    strcpy(cgiargs, ""); /* cgi argument를 지우고 */
    strcpy(filename, ".");
    strcat(filename, uri);           /* uri를 상대 리눅스 경로이름으로 변환 (e.g. ./index.html) */
    if (uri[strlen(uri) - 1] == '/') // uri가 '/'로 끝난다면
      strcat(filename, "home.html"); // 기본 파일이름 추가
    return 1;
  }
  else
  { /* Dynamic content */
    ptr = index(uri, '?');
    /* cgi argument 추출 */
    if (ptr)
    {
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
      printf("ddddddddd");
    }
    else
      strcpy(cgiargs, "");
    /* 나머지 uri를 상대 리눅스 경로이름으로 변환 */
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

/* serve_static : 서버의 local file을 body로 가진 HTTP response를 클라이언트에게 전달 */
/* Tiny는 5개의 정적 컨텐츠 파일을 지원함 : HTML, unformatted text file, GIF, PNG, JPEG */
void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  // char *srcp, filetype[MAXLINE], buf[MAXBUF];
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* 파일 타입을 결정하기 */
  get_filetype(filename, filetype);

  /* 클라이언트에 response line과 header를 보내기 */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);

  /* 클라이언트에 response body 보내기 */
  srcfd = Open(filename, O_RDONLY, 0); /* read를 위해 filename을 open하고 file descriptor를 얻어옴 */
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); /* 요청한 파일을 가상메모리 영억으로 mapping */
  // Close(srcfd);                                              /* mapping 후 파일을 닫는다 - 메모리 누수를 막기 위해 */
  // Rio_writen(fd, srcp, filesize);                             /* 파일을 클라이언트에 전달 */
  // Munmap(srcp, filesize);                                     /* mapping된 가상메모리를 free 한다 - 메모리 누수를 막기 위해 */

  srcp = (char *)malloc(filesize);
  Rio_readn(srcfd, srcp, filesize);
  Close(srcfd);
  Rio_writen(fd, srcp, filesize);
  free(srcp);
}

/*
 * get_filetype - Derive file type from filename
 */
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}

/* serve_dynamic : child process를 fork하고, CGI program을 child context에서 실행함 */
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = {NULL};

  /* 연결 성공을 알리는 response line을 보내기 */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0)
  { /* Child process를 fork 하기 */
    /* 실제 서버는 모든 CGI 환경 변수를 여기서 설정하나, Tiny에서는 생략함 */
    setenv("QUERY_STRING", cgiargs, 1);   /* URI의 CGI argument를 이용해 QUERY_STRING 환경변수 초기화 */
    Dup2(fd, STDOUT_FILENO);              /* child의 stdout을 file descriptor로 redirect */
    Execve(filename, emptylist, environ); /* CGI program을 실행시킴 */
  }
  Wait(NULL); /* Parent process는 child process의 종료를 기다림 */
}

/* clienterror : 에러 메시지를 클라이언트에게 전송 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor="
                "ffffff"
                ">\r\n",
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);
  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}