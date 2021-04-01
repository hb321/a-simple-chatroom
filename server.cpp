#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

/*
高亮蓝色 \033[34;1m高亮蓝色文字\033[0m
高亮绿色 \033[32;1m高亮绿色文字\033[0m
高亮红色 \033[31;1m高亮红色文字\033[0m
高亮黄色 \033[33;1m高亮黄色文字\033[0m 

*/


typedef struct sockaddr SockAddr;
int fds[20];
char ip[20][20];
int size = 20;


void sigint(int signum){
	char buf[1024] = {};
	sprintf(buf, "\33[34;1mServer has closed!\33[0m");
	for(int i=0; i<size; i++){
		send(fds[i], buf, strlen(buf)+1, 0);
		close(fds[i]);
	}
	puts("\33[34;1mServer has closed!\33[0m");
	exit(0);
}

void* start(void* p){
	//向所有聊天室成员 提示新用户来了
	int fd =*(int*)p;
	char buf3[1024] = {}, buf4[1024] = {};
	int index = 0; //当前socket的下标 
	for(int i=0; i<size; i++){
		if(fds[i] == fd){
			index = i;
			break;
		}
	}
	sprintf(buf3, "\033[34;1mClient_%d has joined the chatroom.\033[0m", index+1);
	for (int i=0; i<size; i++){
        if (fds[i]!=0 && fds[i]!=fd){
//           	printf("send to client_%d\n",fds[i]);
           	send(fds[i], buf3, strlen(buf3)+1, 0);
        }
   	}
   	//向新来的客户端发消息表示成功进入聊天室
	sprintf(buf4, "\033[34;1mWelcome to our chatroom! You are client_%d.\033[0m", 
		index+1); 
   	send(fds[index], buf4, strlen(buf4)+1, 0);
	// 收发数据
	while(1){
		char buf1[1024] = {};
		char buf2[1024] = {};

		//接受信息小于等于零，则用户退出
 		if (0 >= recv(fd, buf1, sizeof(buf1), 0)){
			//退出后将位置空出来
			int index;
            for (index=0; index<size; index++){
                if (fd == fds[index]){
                    fds[index] = 0;
                    break;
                }
            }
			//提示用户退出
			sprintf(buf2, "\33[34;1mClient_%d has left the chatroom!\33[0m", index+1);
			puts(buf2);
			int i;
			for (i=0; i<size; i++){
		    	if (fds[i]!=0 && fds[i]!=fd)
		        	send(fds[i], buf2, strlen(buf2)+1, 0);
			}
			//send(fds[i], buf2, strlen(buf2)+1, 0);
			//结束线程
           	pthread_exit((void*)("Done")); //此处尚有疑问 
		}

		//向所有用户发送信息
		printf("\33[34;1mClient_%d:\33[0m %s", index+1, buf1);
		sprintf(buf2, "\33[34;1mClient_%d:\33[0m %s", index+1, buf1);
		for (int i=0; i<size; i++){
        	if (fds[i]!=0 && fds[i]!=fd){
            	send(fds[i], buf2, strlen(buf2)+1, 0);
        	}
    	}
	}	

}

int main(){
	signal(SIGINT, sigint);
	signal(SIGQUIT, sigint);
	// 创建socket对象
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(0 > sockfd){
		perror("socket");
		return -1;
	}

	// 准备通信地址
	struct sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(1710);
	addr.sin_addr.s_addr = inet_addr("172.18.167.136");

	// 绑定socket对象与通信地址
	socklen_t len = sizeof(addr);
	if(0 > bind(sockfd,(SockAddr*)&addr,len)){
		perror("bind");
		return -1;
	}

	// 设置监听socket对象
	listen(sockfd, size);
	printf("\33[34;1mServer starts!\33[0m\n");
	// 等待连接
	while(1){
		struct sockaddr_in from_addr;
		int fd = accept(sockfd, (SockAddr*)&from_addr, &len);
		if(0 > fd){
			printf("\33[31;1mError while connecting to the client...\33[0m\n");
			continue;
		}
		int i;
        for (i=0; i<size; i++){
            if (fds[i] == 0){
                //记录客户端的socket
                fds[i] = fd;
      			
                //有客户端连接之后，启动线程给此客户服务
                pthread_t pid;
				pthread_create(&pid, NULL, start, &fd);
				strcpy(ip[i], inet_ntoa(from_addr.sin_addr));
				printf("\33[34;1mClient %s is successfully connected!\33[0m\n",ip[i]);
                break;
			}
        }

		if (size == i){
            //发送给客户端说聊天室满了
            char* str = "\33[31;1mSorry, the chatroom is full now!\33[0m\n";
            send(fd, str, strlen(str), 0); 
            close(fd);
        }
    }
}
