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
������ɫ \033[34;1m������ɫ����\033[0m
������ɫ \033[32;1m������ɫ����\033[0m
������ɫ \033[31;1m������ɫ����\033[0m
������ɫ \033[33;1m������ɫ����\033[0m 

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
	//�����������ҳ�Ա ��ʾ���û�����
	int fd =*(int*)p;
	char buf3[1024] = {}, buf4[1024] = {};
	int index = 0; //��ǰsocket���±� 
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
   	//�������Ŀͻ��˷���Ϣ��ʾ�ɹ�����������
	sprintf(buf4, "\033[34;1mWelcome to our chatroom! You are client_%d.\033[0m", 
		index+1); 
   	send(fds[index], buf4, strlen(buf4)+1, 0);
	// �շ�����
	while(1){
		char buf1[1024] = {};
		char buf2[1024] = {};

		//������ϢС�ڵ����㣬���û��˳�
 		if (0 >= recv(fd, buf1, sizeof(buf1), 0)){
			//�˳���λ�ÿճ���
			int index;
            for (index=0; index<size; index++){
                if (fd == fds[index]){
                    fds[index] = 0;
                    break;
                }
            }
			//��ʾ�û��˳�
			sprintf(buf2, "\33[34;1mClient_%d has left the chatroom!\33[0m", index+1);
			puts(buf2);
			int i;
			for (i=0; i<size; i++){
		    	if (fds[i]!=0 && fds[i]!=fd)
		        	send(fds[i], buf2, strlen(buf2)+1, 0);
			}
			//send(fds[i], buf2, strlen(buf2)+1, 0);
			//�����߳�
           	pthread_exit((void*)("Done")); //�˴��������� 
		}

		//�������û�������Ϣ
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
	// ����socket����
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(0 > sockfd){
		perror("socket");
		return -1;
	}

	// ׼��ͨ�ŵ�ַ
	struct sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(1710);
	addr.sin_addr.s_addr = inet_addr("172.18.167.136");

	// ��socket������ͨ�ŵ�ַ
	socklen_t len = sizeof(addr);
	if(0 > bind(sockfd,(SockAddr*)&addr,len)){
		perror("bind");
		return -1;
	}

	// ���ü���socket����
	listen(sockfd, size);
	printf("\33[34;1mServer starts!\33[0m\n");
	// �ȴ�����
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
                //��¼�ͻ��˵�socket
                fds[i] = fd;
      			
                //�пͻ�������֮�������̸߳��˿ͻ�����
                pthread_t pid;
				pthread_create(&pid, NULL, start, &fd);
				strcpy(ip[i], inet_ntoa(from_addr.sin_addr));
				printf("\33[34;1mClient %s is successfully connected!\33[0m\n",ip[i]);
                break;
			}
        }

		if (size == i){
            //���͸��ͻ���˵����������
            char* str = "\33[31;1mSorry, the chatroom is full now!\33[0m\n";
            send(fd, str, strlen(str), 0); 
            close(fd);
        }
    }
}
