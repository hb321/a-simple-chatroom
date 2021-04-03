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
#include"server.h"
/*
高亮蓝色 \033[34;1m高亮蓝色文字\033[0m
高亮绿色 \033[32;1m高亮绿色文字\033[0m
高亮红色 \033[31;1m高亮红色文字\033[0m
高亮黄色 \033[33;1m高亮黄色文字\033[0m 
*/

typedef struct sockaddr SockAddr;

Server s;
//int fds[20];
//char ip[20][20];
//int size = 20;

void sigint(int signum){
	s.close_flag = true;//服务端即将关闭
	//向客户端发起强制关闭通知，这边关闭全部连接，客户端需要自己关闭程序 
	char buf[1024] = {};
	sprintf(buf, "1\33[34;1mServer has been closed!\33[0m");
	
	list<int>::iterator iter;
	for (iter = s.sock_fds.begin(); iter != s.sock_fds.end(); iter++){
		send(*iter, buf, strlen(buf)+1, 0);
		close(*iter);
	}
	s.sock_fds.clear();
	s.fd_user_dict.clear();
	s.user_fd_dict.clear();
	s.saveData();
	puts("\33[34;1mServer has been closed!\33[0m");
	exit(0);
}

void* start(void* p){
	//向所有聊天室成员 提示新用户来了
	int fd =*(int*)p;
	char buf1[1024] = {}, buf2[1024] = {};

	sprintf(buf1, "0\033[34;1mA new client has joined the chatroom.\033[0m");
	list<int>::iterator iter;
	for (iter = s.sock_fds.begin(); iter != s.sock_fds.end(); iter++){
		if (*iter != fd){
			send(*iter, buf1, strlen(buf1)+1, 0);
		}
	}

   	//向新来的客户端发消息表示成功进入聊天室
	sprintf(buf2, "0\033[34;1mWelcome to our chatroom! You are the %dth client.\033[0m", 
		s.sock_fds.size()); 
   	send(fd, buf2, strlen(buf2)+1, 0);
   	
	// 收发数据
	while(1){
		char buf3[1024] = {};
		char buf4[1024] = {};

		//接受信息小于等于零或者服务端准备关闭，则用户退出
 		if (0 >= recv(fd, buf3, sizeof(buf3), 0) || s.close_flag){
			//退出后将位置空出来
			s.sock_fds.remove(fd);
			//若当前sock_fd绑定了用户，则需要让该用户退登 
			if (s.fd_user_dict.find(fd) != s.fd_user_dict.end()){
				//pass
			}
			
			//提示用户退出
			puts("\33[34;1mA client has left the chatroom!\33[0m");

			sprintf(buf4, "2\33[34;1mYou have log out!\33[0m");
			send(fd, buf4, strlen(buf4)+1, 0);
			//结束线程
           	pthread_exit((void*)("Done")); //此处尚有疑问 
		}

		printf("\33[34;1mFrom client:\33[0m %s", buf3);
		//向所有用户发送信息
		sprintf(buf4, "\33[34;1mClient:\33[0m %s", buf3);
    	list<int>::iterator iter;
		for (iter = s.sock_fds.begin(); iter != s.sock_fds.end(); iter++){
			if (*iter != fd){
				send(*iter, buf4, strlen(buf4)+1, 0);
			}
		}
	}	

}

int main(){
//	initializeServer(s);
	signal(SIGINT, sigint);
	signal(SIGQUIT, sigint);
	// 创建socket对象
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(0 > sockfd){
		perror("socket");
		return -1;
	}
	s.sockfd = sockfd;
	
	// 准备通信地址
	struct sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(1710);
	addr.sin_addr.s_addr = inet_addr("172.18.167.136");

	// 绑定socket对象与通信地址
	socklen_t len = sizeof(addr);
	if(0 > bind(sockfd, (SockAddr*)&addr, len)){
		perror("bind");
		return -1;
	}

	// 设置监听socket对象
	listen(sockfd, s.max_con_num);
	printf("\33[34;1mServer starts!\33[0m\n");
	// 等待连接
	while(1){
		if (!s.close_flag){
			struct sockaddr_in from_addr;
			int fd = accept(sockfd, (SockAddr*)&from_addr, &len);
			if(0 > fd){
				printf("\33[31;1mError while connecting to the client...\33[0m\n");
				continue;
			}
			if (s.sock_fds.size() < s.max_con_num){
				s.sock_fds.push_back(fd); //socket队列添加当前客户端 
				//有客户端连接之后，启动线程给此客户服务
	            pthread_t pid;
				pthread_create(&pid, NULL, start, &fd);
	//			strcpy(ip[i], inet_ntoa(from_addr.sin_addr));
				printf("\33[34;1mClient %s:%d is successfully connected!\33[0m\n", 
					inet_ntoa(from_addr.sin_addr), from_addr.sin_port);
			}
			else{
				//发送给客户端说服务器满了
	            char* str = "1\33[31;1mSorry, the server is busy now!\33[0m\n";
	            send(fd, str, strlen(str), 0); 
	            close(fd);
			}      
		}
		  
    }
}
