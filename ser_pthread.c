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
#include <sys/wait.h>
#include <arpa/inet.h> 
#include <openssl/ssl.h>
#include <openssl/err.h> 

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


void *ser_recv(void *t)
{
  char buff[MAXLINE];
  int mytid = task;
  int n;
  sc *fd = NULL;
  fd = (sc*)t;
  int client_socket = fd->c_fd;
  SSL_CTX *ctx;
  SSL *ssl;

  /* SSL 库初始化 */
  SSL_library_init();
  /* 载入所有 SSL 算法 */
  OpenSSL_add_all_algorithms();
  /* 载入所有 SSL 错误消息 */
  SSL_load_error_strings();
  /* 用 TLSv1_2_server_method() 单独表示 V2 标准 */
  ctx = SSL_CTX_new(TLSv1_2_server_method());
  /* SSLv23_client_method 可以以 SSL V2 和 V3 标准兼容方式产生一个 SSL_CTX ，即 SSL Content Text */
  if (ctx == NULL) {
    ERR_print_errors_fp(stdout);
    exit(1);
  }

  // 设置信任根证书
  if (SSL_CTX_load_verify_locations(ctx, "ca.crt",NULL)<=0){
    ERR_print_errors_fp(stdout);
    exit(1);
  }
  /* 载入用户的数字证书， 此证书用来发送给客户端。 证书里包含有公钥 */
  if (SSL_CTX_use_certificate_file(ctx, "server.crt", SSL_FILETYPE_PEM) <= 0) {
    ERR_print_errors_fp(stdout);
    exit(1);
  }
  /* 载入用户私钥 */
  if (SSL_CTX_use_PrivateKey_file(ctx, "server_rsa_private.pem.unsecure", SSL_FILETYPE_PEM) <= 0) {
    ERR_print_errors_fp(stdout);
    exit(1);
  }
  /* 检查用户私钥是否正确 */
  if (!SSL_CTX_check_private_key(ctx)) {
    ERR_print_errors_fp(stdout);
    exit(1);
  }

  /* 基于 ctx 产生一个新的 SSL */
  ssl = SSL_new(ctx);
  /* 将连接用户的 socket 加入到 SSL */
  SSL_set_fd(ssl, client_socket);
  /* 建立 SSL 连接 */
  if (SSL_accept(ssl) == -1) {
    perror("accept");
    close(client_socket);
    exit(1);
  }

  while (1)
  {
    //接受客户端传过来的数据
    //n = recv(client_socket, buff, MAXLINE, 0);
    n = SSL_read(ssl,buff,MAXLINE);
    if(n != -1){
      buff[n] = '\0';
      printf("client %d recv msg from client: %s\n", mytid-1, buff);
      //发消息给客户端
      //if (send(client_socket, buff , n, 0) == -1){
      if (SSL_write(ssl, buff, strlen(buff)) <= 0){
        perror("send error");
        break;
      }
    }
    else{
      printf("client %s:%d:", clientlist[mytid-1].ip, clientlist[mytid-1].port);
      printf("closed %d\n", n);
      close(client_socket);
      task--;
      mytid = 0;
      pthread_exit(NULL);
      break;
    }
  }
  /* 关闭 SSL 连接 */
  SSL_shutdown(ssl);
  /* 释放 SSL */
  SSL_free(ssl);
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
  //建立ocket
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

