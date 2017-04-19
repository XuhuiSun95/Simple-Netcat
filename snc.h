#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>

void create_socket();
void *read_thread_tcp(void *void_ptr);
void *write_thread_tcp(void *void_ptr);
void *read_thread_udp(void *void_ptr);
void *write_thread_udp(void *void_ptr);

int server_mode = 0;
int udp_mode = 0;
char *source_ip_address;
char *hostname;
int port;
int sockfd;
int bsize = 1024;
int close_threads;
pthread_t read_t;
pthread_t write_t;
struct in_addr ip_address,src_address;
struct sockaddr_in addr;