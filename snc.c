#include "snc.h"

// command line helper function
void print_help()
{
    fprintf(stderr, "invalid or missing options\nusage: snc [-l] [-u] [-s source_ip_address] [hostname] port\n");
    exit(0);
}

// stderr and exit program
void print_error()
{
    fprintf(stderr, "internal error\n");
    exit(1);
}

// DNS translate -> IP address
int str_to_ip(char * hostname, struct in_addr *address)
{
    struct hostent *he;
    struct in_addr **addr_list;

    if(strlen(hostname) < 3)
        return -1;

    if( (he = gethostbyname(hostname)) == NULL )
	    return -1;

    memcpy(address, he->h_addr_list[0], he->h_length);
        return 0;
}

// create socket for udp/tcp connection
void create_socket()
{
  if(udp_mode == 0) {
    //printf("creat tcp socket\n");
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        print_error();
  }
  if(udp_mode == 1) {
    //printf("creat udp socket\n");
    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        print_error();
  }

  int reuse_port;
  if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse_port, sizeof(reuse_port)))
        print_error();
}

// read thread for udp/tcp
void *read_thread_udp(void *void_ptr)
{  
    int recvlen;
    socklen_t fromlen = sizeof(addr);

    while(1){
        char udp_buffer[bsize+1];
        memset(udp_buffer, 0, bsize);
        recvlen = recvfrom(sockfd, udp_buffer, bsize, 0, (struct sockaddr *)&addr, &fromlen); 
        if(recvlen < 0)
           print_error();

        printf("%s\n", udp_buffer);
     }
}

void *read_thread_tcp(void *void_ptr)
{
     int fdlisten = *((int *)void_ptr);
     int bytes_recv;
     while(1){
        char tcp_buffer[bsize];
        memset(tcp_buffer, 0, bsize);
        bytes_recv = recv(fdlisten, tcp_buffer, bsize, 0);
        if(bytes_recv == 0 || tcp_buffer[0] == EOF){
           pthread_cancel(write_t);
           exit(2);
           break;
        }
        else if(bytes_recv < 0)
           print_error();
        else
           printf("%s\n", tcp_buffer);
     }
}

// write thread for udp/tcp
void *write_thread_udp(void *void_ptr)
{
     int bytes_recv;
     socklen_t addrlen = sizeof(addr);
     
     while(1){
        char send_data[bsize];
        memset(send_data, 0, bsize);

        int cur_index = 0;
        int c = getchar();
        while(c != '\n' && c != EOF){
           send_data[cur_index] = c;
           cur_index++;
           if(cur_index >= bsize-1)
              break;

           c = getchar();
        }

        if(c == EOF)
           break;

        send_data[bsize-1] = 0;

        sendto(sockfd, send_data, bsize, 0, (struct sockaddr *)&addr, addrlen);
     }
}

void *write_thread_tcp(void *void_ptr)
{
     int fd = *((int *)void_ptr);
     int message_size = bsize;
     char message[message_size];

     while(1){
        memset(message, 0, message_size);

        int cur_index = 0;
        int c = getchar();
        while(c != '\n' && (c != EOF)){
           if(c != EOF){
              message[cur_index] = c;
              cur_index++;
           }
           if(cur_index >= bsize-1)
              break;
           c = getchar();
        }

        message[bsize-1] = 0;

        if(c == EOF){
           message[0] = EOF;
           message[1] = 0;
           cur_index = 2;
        }

        if(send(fd, message, cur_index, 0) != cur_index)
           print_error();
     }
}


int main(int argc, char ** argv)
{
    // init variables
    char optchar;
    int argcl;
    
    // option switch
    opterr = 0;
    while((optchar = getopt(argc, argv, "lus:")) != -1)
        switch (optchar) {
            case 'l':
                if(server_mode == 1)
                    print_help();
                server_mode = 1;
                break;
            case 'u':
                if(udp_mode == 1)
                    print_help();
                udp_mode = 1;
                break;
            case 's':
                if(server_mode == 1)
                    print_help();
                source_ip_address = optarg;
                if(str_to_ip(source_ip_address, &src_address) == -1)
                    print_error();
                source_ip_address = inet_ntoa(src_address);
                //printf("%s\n", source_ip_address);
                break;
            default: /* '?' */
                print_help();
                break;
        }
    
    // pramaters setting
    argcl = argc - optind;
    if(argcl != 2 && argcl != 1)
        print_help();
    else if(argc-optind == 2) {
        hostname = argv[optind];
        if(str_to_ip(hostname, &ip_address) == -1)
            print_error();
        hostname = inet_ntoa(ip_address);
        port = atoi(argv[optind+1]);
        //printf("%s, %i\n", hostname, port);
    } else {
        hostname = NULL;
        port = atoi(argv[optind]);
        //printf("%s, %i\n", hostname, port);

    }
    
    // create socket for udp/tcp connection
    create_socket();

    // server mode
    if(server_mode == 1) {

        // udp connection
        if(udp_mode == 1) {

            struct sockaddr_in server_addr;
            memset((char *)&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(port);
            if(hostname == NULL)
                server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
            else
                server_addr.sin_addr = ip_address;

            if(bind(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) < 0)
                print_error();

            if(pthread_create(&read_t, NULL, read_thread_udp, NULL))
                print_error();

            if(pthread_create(&write_t, NULL, write_thread_udp, NULL))
                print_error();
            
            if(pthread_join(read_t, NULL))
                print_error();

            close(sockfd);
        }
        
        // tcp connection
        if(udp_mode == 0) {

            int connfd;

            struct sockaddr_in address_iface;
            address_iface.sin_family = AF_INET;
            address_iface.sin_addr.s_addr = INADDR_ANY;
            address_iface.sin_port = htons(port);
            if(hostname != NULL)
                address_iface.sin_addr = ip_address;

            if((bind(sockfd, (struct sockaddr *)&address_iface, sizeof(address_iface))) < 0)
                print_error();

            if((listen(sockfd, 1)) < 0)
                print_error();

            int addrlen = sizeof(struct sockaddr_in);
            if((connfd = accept(sockfd, (struct sockaddr *)&address_iface, &addrlen)) < 0)
                print_error();

            if(pthread_create(&read_t, NULL, read_thread_tcp, &connfd))
                print_error();

            if(pthread_create(&write_t, NULL, write_thread_tcp, &connfd))
                print_error();

            if(pthread_join(write_t, NULL))
                print_error();

            if(close(sockfd) < 0)
                print_error();
        }
    }
    
    // client mode
    if(server_mode == 0) {
        
        // udp connection
        if(udp_mode == 1) {
        
            struct sockaddr_in server_addr;
            memset((char *)&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(port);
            server_addr.sin_addr = ip_address;

            if(source_ip_address != NULL)
                inet_aton(source_ip_address, &server_addr.sin_addr);
                
            memcpy(&addr, &server_addr, sizeof(struct sockaddr_in));

            if(pthread_create(&write_t, NULL, write_thread_udp, NULL))
                print_error();

            if(pthread_create(&read_t, NULL, read_thread_udp, NULL))
                print_error();

            if(pthread_join(read_t, NULL))
                print_error();

            close(sockfd);
        }
        
        // tcp connection
        if(udp_mode == 0) {

            struct sockaddr_in address_iface;
            //connect to a server 
            memcpy(&address_iface, hostname, 4);
            address_iface.sin_family = AF_INET;
            address_iface.sin_port = htons(port);
            if(source_ip_address != NULL)
                inet_aton(source_ip_address, &address_iface.sin_addr);

            if(connect(sockfd, (struct sockaddr *)&address_iface, sizeof(address_iface)) < 0)
                print_error();
            
            if(pthread_create(&read_t, NULL, read_thread_tcp, &sockfd))
                print_error();

            if(pthread_create(&write_t, NULL, write_thread_tcp, &sockfd))
                print_error();

            if(pthread_join(write_t, NULL))
                print_error();

            if(close(sockfd) < 0)
                print_error();
        }
    }
    return 0;
}