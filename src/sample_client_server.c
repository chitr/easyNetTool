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
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <time.h> 
#include <assert.h>
#include <stdio.h>
#include <sys/stat.h>
#include <netinet/tcp.h>
#include <sys/statfs.h>
#include <utime.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "easyNettool_socketop.h"
#include "log_helper.h"

#define BUFFSIZE 1024 /*4K*/
#define CLIENT 0
#define SERVER 1
#define WAIT_TIME_FOR_CLIENT 300
char buffer[BUFFSIZE*4]={ [0 ... BUFFSIZE-1] = '1'};/*4K max buff*/

typedef enum state_machine {
	READ=0,
	WRITE,
	IDLE
}state_machine;

typedef enum io_request {
	READ_REQ=0,
	WRITE_REQ,
}io_request_t;

typedef struct io_rq_data{
	uint32_t io_type;
	uint32_t buffsize;
}io_rq_data_t;



/*   profiling function */
struct timeval time_diff(struct timeval time_from, struct timeval time_to) {
        struct timeval result;

        if(time_to.tv_usec < time_from.tv_usec)
        {
            result.tv_sec = time_to.tv_sec - time_from.tv_sec - 1;
            result.tv_usec = 1000000 + time_to.tv_usec - time_from.tv_usec;
        }
        else
        {
                result.tv_sec = time_to.tv_sec - time_from.tv_sec;
                result.tv_usec = time_to.tv_usec - time_from.tv_usec;
        }

        return result;
}


void *
server_client_handler(void *p_client_data){
    SOCKET_STATUS_t ret =SOC_SUCCESS;
    int *p_client=(int *)(p_client_data);
    int client_fd=p_client[0];
    int buffersize=p_client[1];
    int errsv=0;

    state_machine state=IDLE;
    int once=1;

    while(once--){

        /*Data transfer protocol */	

        /*check  for read request*/
        /*Data transfer protocol */    
        int data_read=0;
        int data_chunk=0; 
        int data_written=0;
        int read_data_size=0;
        int write_data_size=0;	
        unsigned char * p_data=NULL;


        buffersize=buffersize*BUFFSIZE;
        /*------------------------------------------------------------------------*/
        /*check read request from client i.e get write request: recv*/
        state=READ;
        io_rq_data_t write_request={0,0};	  
        p_data = (char *)&write_request;
        read_data_size=sizeof(io_rq_data_t);

        while(data_read!=read_data_size){	   
            if((data_chunk=recv(client_fd,(char *)(p_data+data_read),read_data_size-data_read,0))<=0){
                errsv=errno;
                ret=SOC_ERROR;
                EASYNETTOOL_LOG(LOG_CRITICAL,"ERROR server:while getting  request from client,%s",
                        strerror(errsv));
                goto end;
            }
            data_read+=data_chunk;
            data_chunk=0;
        }	
        if(buffersize!=write_request.buffsize){
            ret=SOC_ERROR;
            EASYNETTOOL_LOG(LOG_CRITICAL,"ERROR server :Server client protocol failed mismatch buffersize %d, %d",
                    buffersize,write_request.buffsize);
            goto end;	
        }
        /*------------------------------------------------------------------------*/
        /*Reset counters */
         write_data_size=data_chunk=read_data_size=write_data_size=data_written=data_read=0;
        /*Confirm ready to write request,send */
        state=WRITE;
        write_request.io_type=WRITE_REQ;
        write_request.buffsize=buffersize;/*Requesting for buffsize transfer*/
        p_data = (char *)&write_request; 
        write_data_size=sizeof(io_rq_data_t);

        while(data_written!=write_data_size){	   
            if((data_chunk=send(client_fd,(char *)(p_data+data_written),write_data_size-data_written,0))<=0){
                errsv=errno;
                ret=SOC_ERROR;
                EASYNETTOOL_LOG(LOG_CRITICAL,"ERROR server: while sending read request,%s",strerror(errsv));
                goto end;
            }
            data_written+=data_chunk;
            data_chunk=0;
        }	

        /*------------------------------------------------------------------------*/
        /*Reset counters */
        write_data_size=data_chunk=read_data_size=write_data_size=data_written=data_read=0;
        /*Write  buffer size*/
        state=WRITE;

        p_data = buffer; 
        write_data_size=buffersize;

        while(data_written!=write_data_size){	   
            if((data_chunk=send(client_fd,(char *)(p_data+data_written),write_data_size-data_written,0))<0){
                errsv=errno;
                ret=SOC_ERROR;
                EASYNETTOOL_LOG(LOG_CRITICAL,"ERROR server: while sending read request,%s",strerror(errsv));
                goto end;
            }
            data_written+=data_chunk;
            data_chunk=0;
        }	
       sleep(WAIT_TIME_FOR_CLIENT);

    }	
end:
	close(client_fd);
    ;
    /*No returns*/
}





SOCKET_STATUS_t
sample_server(int buffsize,char *ip,int port){
    SOCKET_STATUS_t ret = SOC_SUCCESS;

    int errsv=0;

    /*Socket APIs */	
    struct sockaddr_in serv_addr;
    int sock_fd=-1,client_fd=-1;

    if(create_socket(&sock_fd,&errsv)){
        EASYNETTOOL_LOG(LOG_CRITICAL,"\n Error create %s",strerror(errsv));
        ret=SOC_ERROR;
        goto end;
    }
    if(bind_socket(sock_fd,&serv_addr,ip,port,&errsv)){
        EASYNETTOOL_LOG(LOG_CRITICAL,"\n Error bind %s",strerror(errsv));
        ret=SOC_ERROR;
        goto end;
    }

    if (listen_socket(sock_fd, &errsv)){
        EASYNETTOOL_LOG(LOG_CRITICAL,"\n Error bind %s",strerror(errsv));
        ret=SOC_ERROR;
        goto end;
    }
    socket_setoption(sock_fd);

    /*Client thread API */

    pthread_attr_t attr_thr;
    pthread_attr_init(&attr_thr);
    pthread_attr_setscope(&attr_thr, PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setdetachstate(&attr_thr, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setstacksize(&attr_thr, THREAD_STACK_SIZE );

    while(TRUE) {
        client_fd = accept(sock_fd, (struct sockaddr*)NULL, NULL);
        if(client_fd < 0) {
            errsv = errno;
            EASYNETTOOL_LOG(LOG_CRITICAL,"Unsuccessful client connection %s", 
                    strerror(errsv));
            continue;
        }
        socket_setoption(client_fd);
        pthread_t client_handler;
        int client_data[2]={client_fd,buffsize};
        if(pthread_create(&client_handler,&attr_thr,server_client_handler,(void *)client_data)){
            errsv=errno;
            EASYNETTOOL_LOG(LOG_CRITICAL,"Error while creating client thread :%s",strerror(errsv));
            close(client_fd);
        }
        if(pthread_detach(client_handler)){
            errsv=errno;
            EASYNETTOOL_LOG(LOG_CRITICAL,"Error with detaching thread %s",strerror(errsv));
            close(client_fd);
        }
        usleep(100);
    }


end:
    return ret;
}


SOCKET_STATUS_t
sample_client(int buffsize,char *ip,int port){
    SOCKET_STATUS_t ret = SOC_SUCCESS;

    struct timeval start={0,0},end={0,0},diff={0,0};

    int sock_fd=-1;
    struct sockaddr_in serv_addr;
    int errsv=0;

    state_machine state=IDLE;

    /*Socket APIs */		
    if(create_socket(&sock_fd,&errsv)){
        EASYNETTOOL_LOG(LOG_CRITICAL,"\n ERROR Client: create %s",strerror(errsv));
        ret=SOC_ERROR;
        goto end;
    }
    if(connect_socket(sock_fd,&serv_addr,ip,port,&errsv)){
        EASYNETTOOL_LOG(LOG_CRITICAL,"ERROR Client: Connect fail: %s",strerror(errsv));
        ret=SOC_ERROR;
    }
    socket_setoption(sock_fd);

    /*Data transfer protocol */	   
    int data_read=0;
    int data_chunk=0; 
    int data_written=0;
    int read_data_size=0;
    int write_data_size=0;	

    buffsize=buffsize*BUFFSIZE;
	/*------------------------------------------------------------------------*/
    /*Send read request :sending 64 byte data as read request */
    state=WRITE;
    io_rq_data_t read_request={READ_REQ,buffsize};/*Requesting for buffsize transfer*/
    unsigned char * p_data = (unsigned char *)&read_request; 
    write_data_size=sizeof(io_rq_data_t);

    while(data_written!=write_data_size){	   
        if((data_chunk=send(sock_fd,(char *)(p_data+data_written),write_data_size-data_written,0))<0){
            errsv=errno;
            ret=SOC_ERROR;
            EASYNETTOOL_LOG(LOG_CRITICAL,"ERROR Client: while sending read request,%s",strerror(errsv));
            goto end;
        }
        data_written+=data_chunk;
        data_chunk=0;
    }	
    /*------------------------------------------------------------------------*/  
    /*Reset counters */
     write_data_size=data_chunk=read_data_size=write_data_size=data_written=data_read=0;

    /*Expecting ready to read  confirmation/write request from server */
    state=READ;
    io_rq_data_t write_request={0,0};	  
    p_data = (unsigned char*)&write_request;
    read_data_size=sizeof(io_rq_data_t);

    while(data_read!=read_data_size){	   
        if((data_chunk=recv(sock_fd,(char *)(p_data+data_read),read_data_size-data_read,0))<0){
            errsv=errno;
            ret=SOC_ERROR;
            EASYNETTOOL_LOG(LOG_CRITICAL,"ERROR Client: while getting  request,%s",strerror(errsv));
            goto end;
        }
        data_read+=data_chunk;
        data_chunk=0;
    }	
	/*------------------------------------------------------------------------*/
    /*Server is supposed to send same buffsize as requested*/
    if(buffsize!=write_request.buffsize){
        ret=SOC_ERROR;
        EASYNETTOOL_LOG(LOG_CRITICAL,"ERROR Client:buffsize mismatch from server-client");
        goto end;
    }
    /*------------------------------------------------------------------------*/
     write_data_size=data_chunk=read_data_size=write_data_size=data_written=data_read=0;

    /*Receiveing buffsize data from server */
    
    state=READ;
    gettimeofday(&start,NULL);
    p_data = (unsigned char*)buffer;
    read_data_size=buffsize;

    while(data_read!=read_data_size){	
		int data_chunk=0;
        if((data_chunk=recv(sock_fd,(char *)(p_data+data_read),read_data_size-data_read,0))<0){
            errsv=errno;
            ret=SOC_ERROR;
            EASYNETTOOL_LOG(LOG_CRITICAL,"ERROR Client: while getting  request,%s",strerror(errsv));
            printf("ERROR Client: while getting  request,%s",strerror(errsv));
            goto end;
        }
        data_read+=data_chunk;
        data_chunk=0;
		
    }   
	/*------------------------------------------------------------------------*/
    gettimeofday(&end,NULL);
    diff = time_diff(start,end);
    printf ("\n Time:DIFF::%llu.%.6llu \n",(unsigned long long)diff.tv_sec, (unsigned long long)diff.tv_usec);		   
end:
    return ret;
}


#define ARG_COUNT 5
int 
main(int argc,char **argv){    
    if(argc < ARG_COUNT){
        printf("\nUsage:%s Type.client/server<0 / 1>" 
                "buffsize in KB <buffsize> <IP> <port>",argv[0]);
        return ;

    }

    int type=atoi(argv[1]);
    int buffsize=atoi(argv[2]);
    char *ip=argv[3];
    int port=atoi(argv[4]);

    port=htons(port);


    struct timeval start={0,0},end={0,0};

    int sock_fd=-1;
    struct sockaddr_in serv_addr;
    int errsv=0;

    state_machine state=IDLE;
    if((CLIENT==type) && sample_client(buffsize,ip,port))
        printf("\n ERROR while starting client ");

    if((SERVER==type) && sample_server(buffsize,ip,port))		
        printf("\n ERROR while starting server ");
    return 0;
}
