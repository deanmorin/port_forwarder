#include "network.hpp"
#include <arpa/inet.h>
#include <errno.h>
#include <iostream>
#include <netdb.h>
#include <stdlib.h>
#include <sys/socket.h>

int connectSocket(const char* host, const int port)
{
    int sock;
    struct sockaddr_in server;
    struct hostent* hp;

    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        sockError("socket()", 0);
        return -1;
    }
    int arg = 1;
    // set so port can be resused imemediately after ctrl-c
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &arg, sizeof(arg)) == -1) 
    {
        sockError("setsockopt()", 0);
        return -1;
    }

    // set up address structure
    memset((char*) &server, 0, sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    if (!(hp = gethostbyname(host)))
    {
        fprintf(stderr, "Error: unknown server address\n");
        exit(1);
    }
    memcpy((char*) &server.sin_addr, hp->h_addr, hp->h_length);

    // connect
    if (connect(sock, (struct sockaddr*) &server, sizeof(server)))
    {
        sockError("connect()", 0);
        return -1;
    }

    return sock;
}

int clearSocket(int fd, char* buf, int bufsize)
{
    int	read = 0;
    int bytesToRead = bufsize;
    char* bp = buf;

    while ((read = recv(fd, bp, bytesToRead, 0)) < bytesToRead)
    {
        if (read == -1)
        {
            if (errno != EAGAIN)
            {
                sockError("recv()", 0);
                return -1;
            }
#ifdef DEBUG
            sockError("recv()", 0);
#endif
            break;
        } 
        else if (!read)
        {
            return -1; 
        }
        bp += read;
        bytesToRead -= read;
    }
    return bufsize - bytesToRead;
}


int sockError(const char* msg, int err)
{
    if (!err)
    {
        err = errno;
        perror(msg);
    }
    else
    {
        printf("%s", msg);
    }
    fprintf(stderr, "error type: ");

    switch (err)
    {
        case EAGAIN:        fprintf(stderr, "EAGAIN\n");        break;
        case EACCES:        fprintf(stderr, "EACCES\n");        break;
        case EADDRINUSE:    fprintf(stderr, "EADDRINUSE\n");    break;
        case EADDRNOTAVAIL: fprintf(stderr, "EADDRNOTAVAIL\n"); break;
        case EAFNOSUPPORT:  fprintf(stderr, "EAFNOSUPPORT\n");  break;
        case EALREADY:      fprintf(stderr, "EALREADY\n");      break;
        case EBADF:         fprintf(stderr, "EBADF\n");         break;
        case ECONNREFUSED:  fprintf(stderr, "ECONNREFUSED\n");  break;
        case EFAULT:        fprintf(stderr, "EFAULT\n");        break;
        case EHOSTUNREACH:  fprintf(stderr, "EHOSTUNREACH\n");  break;
        case EINPROGRESS:   fprintf(stderr, "EINPROGRESS\n");   break;
        case EINTR:         fprintf(stderr, "EINTR\n");         break;
        case EINVAL:        fprintf(stderr, "EINVAL\n");        break;
        case EISCONN:       fprintf(stderr, "EISCONN\n");       break;
        case ENETDOWN:      fprintf(stderr, "ENETDOWN\n");      break;
        case ENETUNREACH:   fprintf(stderr, "ENETUNREACH\n");   break;
        case ENOBUFS:       fprintf(stderr, "ENOBUFS\n");       break;
        case ENOTSOCK:      fprintf(stderr, "ENOTSOCK\n");      break;
        case EOPNOTSUPP:    fprintf(stderr, "EOPNOTSUPP\n");    break;
        case EPROTOTYPE:    fprintf(stderr, "EPROTOTYPE\n");    break;
        case ETIMEDOUT:     fprintf(stderr, "ETIMEDOUT\n");     break;
        case ECONNRESET:    fprintf(stderr, "ECONNRESET\n");    break;
        default:            fprintf(stderr, "%d (unknown)\n", err); 
    }
    return err;
}
