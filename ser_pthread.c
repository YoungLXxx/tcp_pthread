//server

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
#include <arpa/inet.h>  

#define DEFAULT_PORT 8000
#define MAXLINE 4096

typedef struct SOCKETCON{
  int s_fd;
  int c_fd;
  struct sockaddr_in servaddr;
}sc;
typedef struct CLIENTLIST{
  int c_fd;
  unsigned short port;
  char *ip;
}cl;
cl clientlist[MAXLINE];
short task = 0;
char buff[MAXLINE];
char sendline[MAXLINE];
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

static void wait(void)
{
  time_t start_time = time(NULL);
  printf("%s\n", ctime(&start_time));
  while (time(NULL) == start_time)
  {
    /* do nothing except chew CPU slices for up to one second */
  }
}

void *ser_recv(void *t)
{
  char buff[MAXLINE];
  int mytid = task;
  int n;
  sc *fd = NULL;
  fd = (sc*)t;
  int client_socket = fd->c_fd;
  while (1)
  {
    //接受客户端传过来的数据
    n = recv(client_socket, buff, MAXLINE, 0);
    if(n != -1){
      buff[n] = '\0';
      printf("client %d recv msg from client: %s\n", mytid-1, buff);
      //send
      if (send(client_socket, buff , n, 0) == -1)
        perror("send error");
    }
    else{
      printf("client %s:%d:", clientlist[mytid-1].ip, clientlist[mytid-1].port);
      printf("closed %d\n", n);
      close(client_socket);
      task--;
      mytid = 0;
      pthread_exit(NULL);
    }
  }

}

int main(int argc, char **argv)
{
  int i,on = 1;
  sc fd;
  sc* pfd;
  pfd = &fd;
  char delims[] = ":";
  char *result = NULL;
  int tid = 0;
  int sin_size = sizeof(struct sockaddr_in);
  struct sockaddr_in remote_addr;
  //初始化Socket
  if ((fd.s_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    printf("create socket error: %s(errno: %d)\n", strerror(errno), errno);
    exit(0);
  }
  //初始化
  memset(&fd.servaddr, 0, sizeof(fd.servaddr));
  fd.servaddr.sin_family = AF_INET;
  fd.servaddr.sin_addr.s_addr = htonl(INADDR_ANY); //IP地址设置成INADDR_ANY,让系统自动获取本机的IP地址。
  fd.servaddr.sin_port = htons(DEFAULT_PORT);      //设置的端口为DEFAULT_PORT
  setsockopt(fd.s_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)); //设置端口重用
  //将本地地址绑定到所创建的套接字上
  if (bind(fd.s_fd, (struct sockaddr *)&fd.servaddr, sizeof(fd.servaddr)) == -1)
  {
    printf("bind socket error: %s(errno: %d)\n", strerror(errno), errno);
    exit(0);
  }
  //开始监听是否有客户端连接
  if (listen(fd.s_fd, 10) == -1)
  {
    printf("listen socket error: %s(errno: %d)\n", strerror(errno), errno);
    exit(0);
  }
  printf("======waiting for client's request======\n");
  init();
  
  
  while(1){
    //阻塞直到有客户端连接，不然多浪费CPU资源。
    if ((fd.c_fd = accept(fd.s_fd, (struct sockaddr *)&remote_addr, &sin_size)) == -1)
    {
      printf("accept socket error: %s(errno: %d)", strerror(errno), errno);
      continue;
    }
    clientlist[task].port = remote_addr.sin_port;
    clientlist[task].ip = inet_ntoa(remote_addr.sin_addr);
    clientlist[task].c_fd = fd.c_fd;
    printf("client %s:%d:\n", clientlist[task].ip, clientlist[task].port);
    pthread_create(&threads[task], &attr, ser_recv, (void *)&fd);
    task++;
  }
  pthread_exit(NULL);

  task++;
  
  for(i = task;i>=0;i--){
    pthread_join(threads[i], NULL);
  }
  printf("Main(): Waited and joined with %d threads. Done.\n", 
          task);
  /* Clean up and exit */
  pthread_attr_destroy(&attr);
  pthread_mutex_destroy(&count_mutex);
  pthread_cond_destroy(&count_threshold_cv);
  pthread_exit (NULL);
  close(fd.s_fd);
}

