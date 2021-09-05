//client

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAXLINE 4096
short task = 0;
typedef struct CLIENT_SOCKET{
  int sockfd;
  int rec_len;
}cs;

pthread_t threads[MAXLINE];
pthread_mutex_t count_mutex;
pthread_cond_t count_threshold_cv;
pthread_attr_t attr;

void init(){
  /* 初始化互斥和条件变量对象 */
  pthread_mutex_init(&count_mutex, NULL);
  pthread_cond_init (&count_threshold_cv, NULL);

  /* 设置线程属性:为了可移植性，显式地在可join状态下创建线程 */
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
}

void *cli_recv(void *p){
  char buf[MAXLINE];
  cs *client_socket = (cs*)p;
  while (1)
  {
    //接受客户端传过来的数据
    if ((client_socket->rec_len = recv(client_socket->sockfd, buf, MAXLINE, 0)) == -1)
    {
      perror("recv error");
      exit(1);
    }
    if(client_socket->rec_len==0){
      continue;
    }
    buf[client_socket->rec_len] = '\0';
    printf("Received : %s ", buf);
    printf("\nsend msg to server: \n");
  }
}

int main(int argc, char **argv)
{
  cs client_socket;
  char sendline[4096];
  struct sockaddr_in servaddr;

  if (argc != 2)
  {
    printf("usage: ./client <ipaddress>\n");
    exit(0);
  }

  if ((client_socket.sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    printf("create socket error: %s(errno: %d)\n", strerror(errno), errno);
    exit(0);
  }

  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(8000);
  if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0)
  {
    printf("inet_pton error for %s\n", argv[1]);
    exit(0);
  }

  if (connect(client_socket.sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
  {
    printf("connect error: %s(errno: %d)\n", strerror(errno), errno);
    exit(0);
  }
  init();
  pthread_create(&threads[task], &attr, cli_recv, (void *)&client_socket);
  printf("send msg to server: \n");
  while (1)
  {
    fgets(sendline, 4096, stdin);
    if (send(client_socket.sockfd, sendline, strlen(sendline), 0) < 0)
    {
      printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
      //exit(0);
    }

  }
  close(client_socket.sockfd);
  exit(0);
}

