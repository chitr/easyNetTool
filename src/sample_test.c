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
#include <stdlib.h>
#include <stdio.h>
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

#include <sys/stat.h>
#include <netinet/tcp.h>
#include <sys/statfs.h>
#include <utime.h>

#include <lib_socketop.h>

int main(int argc ,char **argv){
    if(argc < 3){
        printf("\n usage: %s <ip> <port> <fd_limit>\n",argv[0]);
        exit(1);
    }
    int errsv=0;
    char *server_ip=argv[1];
    int porti=atoi(argv[2]);
    int fd_limit=atoi(argv[3]);
    int count=0;
    struct sockaddr_in serv_addr;
    int port=htons(porti);
    int fail_count=0;
    int success_count=0;
    while(count++ <fd_limit){
        /*consume fds*/
        int sock_fd=-1;
        if(create_socket(&sock_fd,&errsv)){
            printf("\n Error create %s",strerror(errsv));
        }

    }
    count=0;
    while(count++  < fd_limit){
        int sock_fd=-1;
        struct sockaddr_in serv_addr;
        char buf[10]="buftest\n";
        printf("\n Socket FD:%d",sock_fd);
        if(create_socket(&sock_fd,&errsv)){
            printf("\n Error create %s",strerror(errsv));
            exit(1);
        }
        printf("\n Socket FD:%d",sock_fd);
        if(connect_socket(sock_fd,&serv_addr,server_ip,port,&errsv)){

            printf("\n Connect fail:fail count %d %s",fail_count++,strerror(errsv));
        }else{
            printf("\n Connect successful count  %d",success_count++);
            if(sizeof(buf)!=write(sock_fd,buf,sizeof(buf)))
                printf("\n write error %d %s",sock_fd,strerror(errno));
            else
                printf("\n write success %d %s",sock_fd,strerror(errno));

        }

    }

}
