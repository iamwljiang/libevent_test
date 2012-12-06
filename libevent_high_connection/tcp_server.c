#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/util.h>
#include <event2/listener.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
int connected = 0;
int bad = 0;
int closed = 0;


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
void time_cb(evutil_socket_t fd, short what, void *data)
{
	struct timer_info *ti = data;
	printf("catch timeout,connected:%d,bad:%d,closed:%d\n",
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

static void set_tcp_no_delay(evutil_socket_t fd)
{
  int one = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
      &one, sizeof one);
}

//在server端,进入到event callback就表示这个连接有问题
void event_cb(struct bufferevent *bev, short events, void *ctx)
{
    if (events & BEV_EVENT_ERROR){
        printf("connection error:%s\n",evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
        bad += 1;
        connected -= 1;
    }else if (events & BEV_EVENT_EOF) {
    	closed += 1;	
    	connected -=1;
    }  

    bufferevent_free(bev);
}

//接收到连接
void accept_conn_cb(struct evconnlistener *lev, evutil_socket_t fd, 
	struct sockaddr *addr, int socklen, void *data)
{
	connected += 1;
	struct event_base* base = evconnlistener_get_base(lev);
	set_tcp_no_delay(fd);
	struct bufferevent* bev = bufferevent_socket_new(base,fd,BEV_OPT_CLOSE_ON_FREE);
	bufferevent_setcb(bev,NULL,NULL,event_cb,NULL);
	bufferevent_enable(bev,EV_READ|EV_WRITE);
}

//listener出问题了
void accept_error_cb(struct evconnlistener *lev, void *data)
{
	struct event_base* base = evconnlistener_get_base(lev);
	printf("listener have error:%s\n",evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
	event_base_loopbreak(base);
}

int main(int argc, char** argv)
{
	struct event_base* base = event_base_new();

	struct sockaddr_in local_addr;
	local_addr.sin_family = AF_INET;
	local_addr.sin_addr.s_addr = htonl(0);
	local_addr.sin_port = htons(33333);

	set_timer(base);
	set_signal(base);

	struct evconnlistener* lev;
	//backlog 大小对client非同台机器测试会有影响
	lev = evconnlistener_new_bind(base,accept_conn_cb,NULL,LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE,
		-1,(struct sockaddr*)&local_addr,sizeof(local_addr));
	if(lev == NULL){
		printf("new listener error\n");
		return -1;
	}
	evconnlistener_set_error_cb(lev, accept_error_cb);

	event_base_dispatch(base);

	evtimer_del(global_ti.time_event);
	evsignal_del(global_si.sig_event);
	evsignal_del(global_si.sig_event1);
	evsignal_del(global_si.sig_event2);

	evconnlistener_free(lev);
	event_base_free(base);
	printf("program exiting successful\n");
	return 0;
}
