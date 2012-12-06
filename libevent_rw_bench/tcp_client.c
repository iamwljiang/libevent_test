#include <event2/event.h>
#include <event2/util.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>
static char server_ip[16] = "127.0.0.1";
static int connected = 0;
static unsigned int thread_id;
static long rbytes;
//static long wbytes;
static int  rhandles;
static struct event* time_event = NULL;
/*
send connection id | ask | time_string
server calc 'ask' * 'connection_id' % time(NULL) 
*/
typedef struct {
	unsigned int conn_id;
	unsigned int ask;
	unsigned int max;
}conn_rec;

void make_string(void *data,char *out)
{
	conn_rec *conn = data;
	if(conn->ask > conn->max){
		conn->ask = conn->max >> 10;
	}
	time_t now = time(NULL);
	sprintf(out,"%u|%u|%ld",conn->conn_id,conn->ask,now);
}

void event_cb(struct bufferevent *bev,short what,void *data)
{
	conn_rec *conn = data;
	if(what & BEV_EVENT_CONNECTED){
		connected += 1;
		conn->conn_id = thread_id%10000 + connected;
		conn->ask = rand() % conn->conn_id ;
		char out_buf [256] = "";
		make_string(data,out_buf);
		evbuffer_add(bufferevent_get_output(bev),out_buf,strlen(out_buf));
		return;
	}

	if(what & BEV_EVENT_ERROR){
		printf("conntect error:%s\n",evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
	}
	
	if(what & (BEV_EVENT_ERROR|BEV_EVENT_EOF)){
		connected -=1;
		bufferevent_free(bev);
		free(conn);
	}
}

void write_cb(struct bufferevent *bev,void *data)
{
	printf("call write callback:%ld\n",evbuffer_get_length(bufferevent_get_output(bev)));
	//char out_buf [256] = "";
	//make_string(data,out_buf);
	//wbytes += 
	//evbuffer_add(bufferevent_get_output(bev),out_buf,strlen(out_buf));
}

void read_cb(struct bufferevent *bev,void *data)
{
	rhandles += 1;
	conn_rec *conn = data;
	struct evbuffer * in_evbuf = bufferevent_get_input(bev);
	int len = evbuffer_get_length(in_evbuf);
	rbytes += len;

	char *data_buf = malloc(len+1);
	evbuffer_remove(in_evbuf,data_buf,len);
	data_buf[len] = '\0';
	//printf("recv server response:%s\n",data_buf);
	
	conn->ask = atol(data_buf);
	char out_buf [256] = "";
	make_string(data,out_buf);
	evbuffer_add(bufferevent_get_output(bev),out_buf,strlen(out_buf));
	free(data_buf);
}

int launch_connect(void *data)
{	
	int ret = 0;
	evutil_socket_t sock_fd = socket(AF_INET,SOCK_STREAM,0);
	if(sock_fd < 0){
		printf("create socket error\n");
		return -1;
	}

	ret = evutil_make_socket_nonblocking(sock_fd) ;
	if(ret < 0){
		printf("make socket nonblocking error\n");
		return -2;
	}

	struct event_base* base = data;
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port   = htons(3333);
	if(inet_pton(AF_INET,server_ip,&server_addr.sin_addr.s_addr) != 1)
		return -1;	
	conn_rec *conn = malloc(sizeof(conn_rec));
	conn->max = 2 << 30;
	struct bufferevent* bev = bufferevent_socket_new(base,sock_fd,BEV_OPT_CLOSE_ON_FREE);
	bufferevent_setcb(bev,read_cb,NULL,event_cb,conn);

	bufferevent_enable(bev, EV_READ|EV_WRITE);
	if(bufferevent_socket_connect(bev,(struct sockaddr*)&server_addr,sizeof(server_addr)) != 0){
		printf("connect failed\n");
		bufferevent_free(bev);
		free(conn);
		return -3;
	}

	return 0;
}

void time_cb(evutil_socket_t fd,short what,void *arg)
{
	struct timeval tv = {5,0};
	float per_conn_speed = 0;
	if(connected != 0){
		per_conn_speed = rhandles*1.0 / connected /tv.tv_sec;
	}

	float read_speed = 0;
	read_speed = rbytes * 1.0 / 1024 / 1024;
	printf("connected:%d,read:%0.2f m/s,taotal handles:%0.2f c/s,per conn handles:speed:%0.2fc/s\n",
			connected,read_speed,rhandles*1.0/tv.tv_sec,per_conn_speed);
	rbytes=0;
	rhandles = 0;
	evtimer_add(time_event,&tv);
}

int set_timer(void *data)
{
	struct event_base * base = data;
	struct event * tev = evtimer_new(base,time_cb,NULL);
	time_event = tev;
	
	struct timeval tv = {5,0};
	evtimer_add(time_event,&tv);
	return 0;
}

int main(int argc,char** argv)
{
	thread_id = pthread_self();
	signal(SIGPIPE,SIG_IGN);
	int max_conn = 10;
	if(argc >= 2){
		max_conn = atoi(argv[1]);
	}
	struct event_base* base = event_base_new();
	int i;

	for(i = 0; i < max_conn; ++i){
		launch_connect(base);
	}
	srand(time(NULL));
	set_timer(base);

	event_base_dispatch(base);
	event_free(time_event);
	event_base_free(base);

	return 0;
}

