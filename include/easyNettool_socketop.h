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
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef ERROR
#define ERROR -1
#endif
#ifndef SUCCESS
#define SUCCESS 0
#endif
#ifndef ULIMIT_MAX
#define ULIMIT_MAX 65535
#endif
#ifndef THREAD_STACK_SIZE
#define THREAD_STACK_SIZE 2116488
#endif

#define TIME_IS_ZERO(start_time) \
	!(start_time.tv_sec || start_time.tv_usec)

#define TIME_MULTIPLY(time,factor)  { \
time.tv_sec = ((factor * time.tv_sec)+ ((factor*time.tv_usec)/1000000));  \
time.tv_usec = (factor*time.tv_usec)%(1000000);\
}


/*****************Socket helper ***********************************************/

#ifndef SOCKET_BACKLOG
#define SOCKET_BACKLOG 100
#endif 

#ifndef RESERVED_FD_COUNT 
#define RESERVED_FD_COUNT 3
#endif

#ifndef SET_SOCKETOPTION
#define SET_SOCKETOPTION TRUE
#endif

#ifndef SOCKET_CONNECT_TIMEOUT
#define SOCKET_CONNECT_TIMEOUT 15 /*Timeout for connect()*/
#endif	

typedef enum SOCKET_STATUS {
SOC_SUCCESS=0,
SOC_IO_ERROR,
SOC_RETRY,
SOC_STALE_SOCKET_ERROR,
SOC_ARG_ERROR,
SOC_ERROR,
SOC_TIMEOUT
}SOCKET_STATUS_t;


#define MAX_SLEEP_IN_RECV 1
#define RECV_BUFF_LENGTH    1048576 /*1 MB buffer for fetch data*/


#define SOCKET_CONNECT_TIMEOUT 15 /*Timeout for connect()*/


#define OFF   0
#define ON    1

/******* EPOLL  data **********************************************************/
#ifndef EPOLL_ARG1
#define EPOLL_ARG1 1
#endif
#ifndef MAX_EPOLL_EVENT
#define MAX_EPOLL_EVENT 1
#endif
#ifndef SEC2MSEC
#define SEC2MSEC 1000 /*convert from sec to msec*/
#endif
#ifndef ERR_RESET
#define ERR_RESET  \
	errno=0;
#endif

