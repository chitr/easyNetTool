/*
 * vim:expandtab:shiftwidth=4:tabstop=4:
 *
 * Copyright   (2013)      Contributors
 * Contributor : chitr   chitr.prayatan@gmail.com
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * ---------------------------------------
 */
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <time.h> 
#include <assert.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <netinet/tcp.h>
#include <sys/statfs.h>
#include <utime.h>


#include "easyNettool_socketop.h"
#include "log_helper.h"


SOCKET_STATUS_t 
sock_wait_epoll(int socketfd,int timeout,int *p_errsv){
    SOCKET_STATUS_t ret=SOC_SUCCESS;

    struct epoll_event event_on_socketfd;
    int epoll_fd = ERROR;
    struct epoll_event processableEvents;
    int numEvents = ERROR;
    int timeout_msec=timeout*SEC2MSEC;
    int so_error=ERROR;
    unsigned so_len=sizeof(so_error);

    ERR_RESET
        if (ERROR==(epoll_fd = epoll_create (EPOLL_ARG1))){	  
            EASYNETTOOL_LOG(LOG_CRITICAL,"Could not create the epoll FD list %s",
                    strerror(*p_errsv));
            *p_errsv=errno;
            ret=SOC_ERROR;
            goto end;
        }     

    event_on_socketfd.data.fd = socketfd;
    event_on_socketfd.events = EPOLLOUT | EPOLLIN | EPOLLERR;

    ERR_RESET
        if(ERROR==epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socketfd, &event_on_socketfd)){
            *p_errsv=errno;	
            EASYNETTOOL_LOG(LOG_CRITICAL,"Unable to add socket fd to epoll event list:%s");      	   
            ret=SOC_ERROR;
            goto end;
        }
    ERR_RESET
        numEvents = epoll_wait(epoll_fd, &processableEvents, 
                MAX_EPOLL_EVENT,timeout_msec);

    if (numEvents <= 0){
        *p_errsv=errno;	
        EASYNETTOOL_LOG(LOG_CRITICAL,"epoll Error: epoll_wait :%s",strerror(*p_errsv));
        ret = (numEvents)? SOC_ERROR:SOC_TIMEOUT;
        goto end;
    }
    ERR_RESET
        if (getsockopt(socketfd, SOL_SOCKET,SO_ERROR, &so_error, &so_len) < 0){

            ret=SOC_ERROR;
            *p_errsv = errno;
            EASYNETTOOL_LOG(LOG_INFO,"Socket not ready %s",strerror(*p_errsv));
            goto end;

        }
    if (so_error != 0){  
        ret=SOC_ERROR;
        *p_errsv = so_error;
        EASYNETTOOL_LOG(LOG_INFO,"Socket not ready %s",strerror(*p_errsv));
        goto end;
    }
end:
    printf("\n Epoll fd %d",epoll_fd);
    if(epoll_fd > RESERVED_FD_COUNT){
        if(close(epoll_fd)){
            int err=errno;
            printf("%s epoll fd %s",strerror(errno));}

    }
    return ret;
}

/*wait with select implementataion works only with socket fds < 1024 */
SOCKET_STATUS_t 
sock_wait_select(int socketfd,
	long timeout,
	int rd_fdset,
    int wr_fdset,int *p_errsv){

    SOCKET_STATUS_t ret=SOC_SUCCESS;

    struct timeval tv = {0,0};
    fd_set fdset;
    fd_set *rfds, *wfds;
    int n, so_error;
    unsigned so_len;

    FD_ZERO (&fdset);
    FD_SET  (socketfd, &fdset);
    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    if(p_errsv)
        *p_errsv=0;
    EASYNETTOOL_LOG(LOG_INFO,"wait for connection to get established={%ld,%ld}",
            tv.tv_sec, tv.tv_usec);

    if (rd_fdset) rfds = &fdset; else rfds = NULL;
    if (wr_fdset) wfds = &fdset; else wfds = NULL;

    n = select (socketfd+1, rfds, wfds, NULL, &tv);
    if(p_errsv)
        *p_errsv=errno;
    switch (n) {
        case 0:
            EASYNETTOOL_LOG(LOG_INFO,"This wait timed out");			
            ret=SOC_TIMEOUT;
            break;
        case -1:
            EASYNETTOOL_LOG(LOG_INFO,"This is error during wait");
            ret=SOC_ERROR;
            break;
        default:
            /* select tell us that sock is ready, test it*/
            so_len = sizeof(so_error);
            so_error = 0;
            getsockopt (socketfd, SOL_SOCKET, SO_ERROR, &so_error, &so_len);
            if (so_error != 0){  
				ret=SOC_ERROR;
                *p_errsv = so_error;
                EASYNETTOOL_LOG(LOG_INFO,"wait failed");
            }
    }
end:
    return ret;
}


void
socket_setoption(int socketFd) {
    unsigned int SbMax = (1 << 30);       /* 1GB */

    while(SbMax > 1048576) {
        if((setsockopt(socketFd, SOL_SOCKET, SO_SNDBUF, (char *)&SbMax, sizeof(SbMax)) < 0)
                || (setsockopt(socketFd, SOL_SOCKET, SO_RCVBUF, (char *)&SbMax, sizeof(SbMax)) <
                    0)) {
            SbMax >>= 1;          /* SbMax = SbMax/2 */
            continue;
        }

        break;
    }
    int flag = 1;
    int result = setsockopt(socketFd, IPPROTO_TCP,TCP_NODELAY,
            (char *) &flag,sizeof(int));    
    if(result < 0) 
        EASYNETTOOL_LOG(LOG_CRITICAL, "Could not set tcpnodelay on socket: %d",socketFd);

    struct timeval tv={0,0};

    tv.tv_sec = 300;  /* 300 Secs Timeout */

    if(setsockopt(socketFd, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv,sizeof(struct timeval))< 0)
        EASYNETTOOL_LOG(LOG_CRITICAL, "Could not set timeout for rcv on socket: %d",socketFd);

    if(setsockopt(socketFd, SOL_SOCKET, SO_SNDTIMEO,(struct timeval *)&tv,sizeof(struct timeval))< 0)
        EASYNETTOOL_LOG(LOG_CRITICAL, "Could not set timeout for send on socket: %d",socketFd);


    return;
} 

SOCKET_STATUS_t 
create_socket(int *p_socket_fd,int *p_errsv) {
    int ret = SOC_SUCCESS;
    if((*p_socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        *p_errsv = errno;
        EASYNETTOOL_LOG(LOG_CRITICAL,"\n Error : Could not create socket %s",
                strerror(errno));
        *p_socket_fd = 0;	
        ret = SOC_RETRY;
    } 
    return ret;
}

SOCKET_STATUS_t 
connect_socket(int sockfd, 
               struct sockaddr_in* serv_addr, 
               const char* server_address, 
               const int port_no, int *p_errsv) {
    SOCKET_STATUS_t ret = SOC_SUCCESS;	
    int timeout=SOCKET_CONNECT_TIMEOUT ;
    SOCKET_STATUS_t  wait_on_socket=SOC_SUCCESS;
    int opts=0;

    memset(serv_addr, 0, sizeof(struct sockaddr_in)); 
    serv_addr->sin_family = AF_INET;
    serv_addr->sin_port = port_no;

    if(inet_pton(AF_INET, server_address, &(serv_addr->sin_addr))<=0) {
        *p_errsv = errno;
        EASYNETTOOL_LOG(LOG_CRITICAL,"\n inet_pton error occured\n");
        ret = SOC_ARG_ERROR;
        goto end;
    } 
    /*Set socket fd to non-blocking mode */
    if ((fcntl(sockfd, F_SETFL, O_NONBLOCK) < 0)){		
        EASYNETTOOL_LOG(LOG_CRITICAL,"error on setting non-block socket flags.");
        ret=SOC_RETRY;
        goto end;
    }

    /*Errors will be handle in wait for socket call */
    int socket_ret=ERROR;
    *p_errsv=errno=0;
    /*Since socket is non-blocking ,it would return EINPROGRESS/SUCCESS or ERROR */ 
    if((SUCCESS!=(socket_ret=connect(sockfd, 
                        (struct sockaddr *)serv_addr, sizeof(struct sockaddr))))
            &&(*p_errsv=errno)){
        if(EINPROGRESS==*p_errsv)
            if(wait_on_socket=sock_wait_epoll(sockfd,timeout,p_errsv)){
                if((EAGAIN == *p_errsv) ||(SOC_TIMEOUT == wait_on_socket) )
                    ret = SOC_RETRY;
                else
                    ret =  SOC_IO_ERROR;
                if(!*p_errsv)
                    *p_errsv=ETIMEDOUT;
                goto end;
            }
    }	
    opts = fcntl(sockfd,F_GETFL);
    if (opts < 0) {
        EASYNETTOOL_LOG(LOG_CRITICAL,"fcntl(F_GETFL) failed");
        ret = SOC_RETRY;
        goto end;
    }
    /*Reset non blocking flag on socket fd So that it blocks for recv/send */
    opts^=O_NONBLOCK;
    if (fcntl(sockfd,F_SETFL,opts) < 0) {
        EASYNETTOOL_LOG(LOG_CRITICAL,"fcntl(F_SETFL) unset non block failed");
        ret = SOC_RETRY;
        goto end;
    }	   		
#if  SET_SOCKETOPTION	
    socket_setoption(sockfd);
#endif
end:
    return ret ;
}

