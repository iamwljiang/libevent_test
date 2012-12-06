#include <event2/event.h>
#include <event2/util.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>

static int connected = 0;
static struct event* time_event;
static unsigned int  rbytes;
static int rhandles = 0;
void read_cb(struct bufferevent* bev,void *data)
{
	rhandles += 1;
	struct evbuffer * in_evbuf = bufferevent_get_input(bev);
	int len = evbuffer_get_length(in_evbuf);
	rbytes+= len;
	char data_buf[256]="";
	evbuffer_remove(in_evbuf,data_buf,len);
	//printf("recv :%s\n",data_buf);
	char *str = data_buf;
	char *tok;
	unsigned int conn_id;
	unsigned int ask;
	long sec ;
	tok = strtok(str,"|");
	conn_id = atoi(tok);
	tok = strtok(NULL,"|");
	ask = atoi(tok);
	tok = strtok(NULL,"|");
	sec = atol(tok);

	ask = ask * conn_id % sec;
	memset(data_buf,0,256);
	sprintf(data_buf,"%d",ask);
	evbuffer_add(bufferevent_get_output(bev),data_buf,strlen(data_buf));
}

void event_cb(struct bufferevent* bev,short what,void *data)
{
	if(what & BEV_EVENT_ERROR){
		printf("socket error:%s\n",evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
	}
	connected -= 1;
	bufferevent_free(bev);
}

void listener_error_cb(struct evconnlistener *lev,void *data)
{
	printf("listener error:%s\n",evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
	event_base_loopbreak(evconnlistener_get_base(lev));
}

void accept_conn_cb(struct evconnlistener *lev, evutil_socket_t fd, \
	struct sockaddr *cli_addr, int socklen, void *data)
{
	struct bufferevent* bev = bufferevent_socket_new(\
		evconnlistener_get_base(lev),fd,BEV_OPT_CLOSE_ON_FREE);
	bufferevent_setcb(bev,read_cb,NULL,event_cb,NULL);
	bufferevent_enable(bev,EV_READ|EV_WRITE);
	connected += 1;
}

void time_cb(evutil_socket_t fd,short what,void *data)\
{
	struct timeval tval={5,0};
	evtimer_add(time_event,&tval);
	printf("conntected:%d,read speed:%0.2f c/s,read bytes speed:%0.2f m/s\n",\
			connected,rhandles/5.0,rbytes/(1.0*5*1024*1024));
	rbytes = 0;
	rhandles = 0;
}

void set_timer(void *data)
{
	struct event_base * base = data;
	struct event * tev = evtimer_new(base,time_cb,NULL);
	time_event = tev;
	struct timeval tval={5,0};
	evtimer_add(tev,&tval);
}

int main(int argc,char **argv)
{
	struct event_base * base = event_base_new();
	struct sockaddr_in addr ;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(0);
	addr.sin_port = htons(3333);
	set_timer(base);
	struct evconnlistener * lev ;
	lev = evconnlistener_new_bind(base,accept_conn_cb,NULL,
		LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE,
		-1,(struct sockaddr*)&addr,sizeof(addr));

	evconnlistener_set_error_cb(lev,listener_error_cb);

	event_base_dispatch(base);
	evconnlistener_free(lev);
	event_base_free(base);
	return 0;
}
