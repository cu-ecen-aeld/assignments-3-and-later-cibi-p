#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#include <syslog.h>

#define SERVER_FILE "/var/tmp/asedsocket"
#define PORT "9000"  // the port users will be connecting to

#define BACKLOG 10   // how many pending connections queue will hold

volatile sig_atomic_t close_app = false;

void sigchld_handler(int s)
{

    if (s == SIGINT || s == SIGTERM) {
      close_app = true;
    }

    // waitpid() might overwrite errno, so we save and restore it:
    if (s == SIGCHLD) {
      int saved_errno = errno;
      while(waitpid(-1, NULL, WNOHANG) > 0);
      errno = saved_errno;
    }
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
    // listen on sock_fd, new connection on new_fd
    int sockfd, new_fd;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address info
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    char buf[100];
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if (argc > 2) {
        fprintf(stderr,"usage: ./aesdsocket -d for daemon mode or ./aesdsocket\n");
        exit(1);
    }
    if (argc == 2)
      if (!strcmp(argv[1], "-d"))
        if (fork())
          exit(0);
    openlog(NULL, LOG_PID | LOG_PERROR, LOG_USER);
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    printf("server: waiting for connections...\n");

    while(1) {  // main accept() loop
        if (close_app == true)
        {
         break; 
        }
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr,
            &sin_size);
        if (new_fd == -1) {
            if (errno != EINTR)
              perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        printf("server: got connection from %s\n", s);
        syslog(LOG_INFO,"Accepted connection from %s\n", s);

        if (!fork()) { // this is the child process
            FILE *fptr = fopen(SERVER_FILE, "a+");
            close(sockfd); // child doesn't need the listener
            int recbytes = 0;
            while (1) {
              if (close_app == true)
                break;
              recbytes = recv(new_fd, buf, 99, 0);
              if (recbytes < 0 && errno == EAGAIN) {
                continue;
              }
              buf[recbytes] = '\0';
              fprintf(fptr,"%s",buf);
              if (buf[recbytes -1] == '\n') {
                break;
              }
            }
            if (close_app == true)
              goto clean;
            buf[recbytes] = '\0';
            fseek(fptr, 0, SEEK_SET);
            while (1) {
              if(!(recbytes = fread(buf, 1, 99, fptr)))
                break;
              buf[recbytes] = '\0';
              if (send(new_fd, buf, recbytes, 0) == -1)
                  perror("send");
            }
clean:
            fclose(fptr);
            close(new_fd);
            exit(0);
        }
        close(new_fd);  // parent doesn't need this
    }
    closelog();
    remove(SERVER_FILE);

    return 0;
}

