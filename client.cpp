#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<string.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include <pthread.h>
#include <signal.h>

int sockfd;

void sigint(int signum)
{
	
	close(sockfd);
	puts("\033[34;1mYou have left the chatroom!\033[0m");
	exit(0);
}

void* start(void* p)
{
	int fd =*(int*)p;
    while(1)
	{
        char buf[1024] = {};
        if (0 >= recv(fd, buf, sizeof(buf), 0) )
		{
            return NULL;
        }
        printf("\r%s\n",buf);
    }
}

int main()
{
	system("clear");
	signal(SIGINT,sigint);
	signal(SIGQUIT,sigint);
	//创建socket对象
	int sockfd = socket(AF_INET,SOCK_STREAM,0);
	if(0>sockfd)
	{
		perror("socket");
		return -1;
	}
	//准备通信地址
	struct sockaddr_in addr ={};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(1710);//端口号
	addr.sin_addr.s_addr = inet_addr("172.18.167.136");//本地ip

	//连接
	if(0>connect(sockfd,(struct sockaddr*)&addr,sizeof(addr)))
	{
		perror("connect");
		return -1;
	}

	pthread_t pid;
    pthread_create(&pid, 0, start, &sockfd);
	//发送数据
	while(1)
	{
		char buf[1024] ={};
		char srt[1000]={};
		fgets(srt, 800, stdin);
		sprintf(buf, srt);
		send(sockfd, buf, strlen(buf)+1, 0);
	}

}
