#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/util.h>

#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
static int  connected = 0;
static int  bad = 0;
static int  closed = 0;

struct timer_info{
	struct event* time_event;	
};

struct timer_info global_ti;

struct signal_info{
	struct event* sig_event;
	struct event* sig_event1;
	struct event* sig_event2;
	struct event_base* base;	
};

struct signal_info global_si;

static void set_tcp_no_delay(evutil_socket_t fd)
{
  int one = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
      &one, sizeof one);
}

static char remote_ip[16]="127.0.0.1";

//如果连接成功的,则不用关心bufferevent是否内存泄露,这只是一个测试高连接的程序
void event_cb(struct bufferevent *bev,
    short events, void *ctx)
{
	if(events & BEV_EVENT_EOF){
		bufferevent_free(bev);
		closed += 1;
		connected -=1;
	}else if(events & BEV_EVENT_CONNECTED){
		connected +=1;
		set_tcp_no_delay(bufferevent_getfd(bev));
	}else if(events & BEV_EVENT_ERROR){
		printf("connection error:%s\n",evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
		bufferevent_free(bev);
		bad +=1;
	}

}

void time_cb(evutil_socket_t fd, short what, void *data)
{
	struct timer_info *ti = data;
	printf("catch timeout,current connection number:%d,bad:%d,closed:%d\n",
		connected,bad,closed);
	
	struct timeval timeout = {5,0};
	evtimer_add(ti->time_event,&timeout);
}

int set_timer(void *data)
{
	struct event_base* base = data;	
	struct event*      time_event = evtimer_new(base,time_cb,&global_ti);
	global_ti.time_event = time_event;
	struct timeval timeout = {5,0};
	evtimer_add(time_event,&timeout);
	return 0;
}

void signal_cb(evutil_socket_t fd, short what, void *data)
{
	printf("catch singal :%d,exit...\n",fd);
	struct signal_info* si = data;	
	event_base_loopexit((struct event_base*)si->base,NULL);
}

int set_signal(void *data)
{
	struct event_base* base = data;	
	struct event* signal_event = evsignal_new(base,SIGINT,signal_cb,&global_si);
	global_si.sig_event = signal_event;
	global_si.base = data;
	
	signal_event = evsignal_new(base,SIGTERM,signal_cb,&global_si);
	global_si.sig_event1 = signal_event;
	
	signal_event = evsignal_new(base,SIGABRT,signal_cb,&global_si);
	global_si.sig_event2 = signal_event;
	
	evsignal_add(global_si.sig_event,NULL);
	evsignal_add(global_si.sig_event1,NULL);
	evsignal_add(global_si.sig_event2,NULL);
	return 0;
}

int lanch_connection(void *data)
{
	/*
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
	*/
	struct event_base* base = data;
	//如果socket fd < 0,则在之后调用bufferevent_socket_connect会自动创建一个socket
	struct bufferevent* bev = bufferevent_socket_new(base,-1,BEV_OPT_CLOSE_ON_FREE);
	if(!bev){
		printf("make bufferevent error\n");
		return -3;
	}

	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port   = htons(33333);
	if(inet_pton(AF_INET,remote_ip,&server_addr.sin_addr.s_addr) != 1){
		printf("inet pton error\n");
		return -4;
	}

	bufferevent_setcb(bev,NULL,NULL,event_cb,NULL);
	bufferevent_enable(bev, EV_READ|EV_WRITE);

	if(bufferevent_socket_connect(bev,(struct sockaddr*)&server_addr,sizeof(server_addr)) < 0){
		printf("can't connect\n");
		return -5;
	}

	return 0;
}

//发起连接,默认客户端和server是同台机器则连接数是10
//执行命令的几种方式:
//./tcp-client 连接数
//./tcp-client ip地址 连接数
int main(int argc,char** argv)
{
	struct event_base* base ;
	base = event_base_new();
	
	int connect_num = 10;
	if (argc == 2){
		connect_num = atoi(argv[1]);
	}else if(argc == 3){
		struct in_addr s_addr ;
		if(inet_pton(AF_INET,argv[1],&s_addr) != 1){
			printf("ip adress error\n");
			return -1;
		}
		strcpy(remote_ip,argv[1]);
		connect_num = atoi(argv[2]);
	}

	int i = 0;
	for(; i < connect_num; ++i){
		lanch_connection(base);
	}

	set_timer(base);
	set_signal(base);
	event_base_dispatch(base);

	evtimer_del(global_ti.time_event);
	evsignal_del(global_si.sig_event);
	evsignal_del(global_si.sig_event1);
	evsignal_del(global_si.sig_event2);
	event_base_free(base);

	printf("program exiting successful\n");
	return 0;
}
