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
#include <stdio.h>


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



SOCKET_STATUS_t 
bind_socket(int sockfd, 
               struct sockaddr_in* p_serv_addr, 
               const char* server_address, 
               const int port_no, int *p_errsv){
    SOCKET_STATUS_t ret=SOC_SUCCESS;
    int errsv=0;
    /*This function to for improving tuning socket for bind call */
    memset(p_serv_addr, 0, sizeof(struct sockaddr_in)); 
    p_serv_addr->sin_family = AF_INET;
    p_serv_addr->sin_port = port_no;	
    p_serv_addr->sin_addr.s_addr = INADDR_ANY;
    if ( bind(sockfd, (struct sockaddr*)p_serv_addr, sizeof(struct sockaddr_in)) == -1 ) {
        errsv=errno;
        EASYNETTOOL_LOG(LOG_CRITICAL,"Bind failed :%s",strerror(errsv));
        ret=SOC_ERROR;	
		*p_errsv=errsv;
    }
end:
    return ret;	
}

SOCKET_STATUS_t 
listen_socket(int sockfd,int *p_errsv){
    SOCKET_STATUS_t ret=SOC_SUCCESS;
    int errsv=0;	
    /*This function to for improving tuning socket for listen  call */
    if(listen(sockfd, SOCKET_BACKLOG) == -1){
        errsv=errno;
        EASYNETTOOL_LOG(LOG_CRITICAL,"listen failed :%s",strerror(errsv));
        ret=SOC_ERROR;	
		*p_errsv=errsv;
    }
}
SOCKET_STATUS_t 
rcv_from_socket(int sock_fd,
                    int file_fd,
                    int total_data_size,
                    void *buff,
                    int receive_buff_size,
                    int *p_errsv){
    /*receive data buffer of  mentioned size on socket .
      A dynamic CPU utilization window-managent is implemented 
      to avoid the receiver thread from hogging CPU i.e there is optimum
      sleep is introduced based on data-transfer rate over socket.
      Above CPU util optimization is very helpful to avoid CPU-hogging during big
      data transfer
      Received data is written to file_fd*/

    void  *recv_data_buffer = NULL; /*Better to keep it as TLS variable*/

    SOCKET_STATUS_t ret=SOC_SUCCESS;
    	


    ssize_t bytes, bytes_in_pipe;
    ssize_t bytes_received=0;
    ssize_t total_bytes_received = 0;
    ssize_t bytes_written =0;

    ssize_t last_bytes_received = 0;
    unsigned long long sleep_multiplier=0;
    struct timeval timer_start;
    struct timeval timer_end;
    struct timeval sleep_time;
    memset(&timer_start, 0, sizeof(struct timeval));
    memset(&timer_end, 0, sizeof(struct timeval));	


    if(!receive_buff_size)
        receive_buff_size = RECV_BUFF_LENGTH;
    if(!recv_data_buffer) {
        if(!(recv_data_buffer = malloc(RECV_BUFF_LENGTH))) {
            *p_errsv = errno;
            EASYNETTOOL_LOG(LOG_CRITICAL, "Failed to allocate memory for" 
                    "recv_data_buffer size %l %s " ,RECV_BUFF_LENGTH,
                    strerror(p_errsv));				
            ret = SOC_IO_ERROR;
            goto end;
        }
    }    
    do{ 
        if(bytes_received ){
            /*bytes_received >0 ,data is not fetched  in first loop*/    
            /*To avoid  threads hogging the CPU,we should sleep */    
            /*timer_start = 0 for first chunk fetch*/

            if(!(TIME_IS_ZERO(timer_start))){
                gettimeofday(&timer_end,NULL);		
                sleep_time = time_diff(timer_start,timer_end);

                /*Ensure minimum sleep time is 4 ms*/
                if(TIME_IS_ZERO(sleep_time))
                    sleep_time.tv_usec = 4000;

                /*Max data to be received for socket buffer = receive_buff_size
                  But we recived "bytes_received" in above sleep_time.
                  To receive "receive_buff_size" data we should increase 
                  sleep time by factor for size_of_buffer/size_of_received_data */
                sleep_multiplier = 
                    ((sleep_multiplier=(receive_buff_size/bytes_received))>1)?
                    sleep_multiplier:1;
                TIME_MULTIPLY(sleep_time,sleep_multiplier)	
                    if(MAX_SLEEP_IN_RECV <= sleep_time.tv_sec )
                        sleep_time.tv_sec = MAX_SLEEP_IN_RECV;
                select(0,NULL,NULL,NULL,&sleep_time);
            }
            /*We need to sleep in fetch call ,to calculate sleep time start timer*/
            gettimeofday(&timer_start,NULL);
        }
        *p_errsv = 0;
        bytes_received = recv(sock_fd,(char *)recv_data_buffer,receive_buff_size,0);
        if(bytes_received <= 0){					
            EASYNETTOOL_LOG(LOG_CRITICAL,"Error while receving file on socket"
                    "access denied\n");
            *p_errsv = errno;

            if (bytes_received && (*p_errsv == EINTR || *p_errsv == EAGAIN)) {   
                *p_errsv = 0;
                continue;
            }		
            ret = SOC_STALE_SOCKET_ERROR ;
            goto end;

        }
        bytes_written = pwrite(file_fd,(char *)recv_data_buffer,bytes_received,
                total_bytes_received);
        if(bytes_written !=bytes_received) {
            EASYNETTOOL_LOG(LOG_CRITICAL,"Unable to write data to file fd");
            *p_errsv = errno;
            ret = SOC_IO_ERROR;
            goto end;		
        }		
        total_bytes_received += bytes_received;	
    }while ((total_bytes_received < total_data_size));	
end:
    return ret;
}

