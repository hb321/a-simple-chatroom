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


typedef struct sockaddr SockAddr;
int fds[20];
char ip[20][20];
int size = 20;


void sigint(int signum){
	char buf[1024] = {};
	sprintf(buf, "\33[;45m%s \33[0m", "Server has closed!");
	for(int i=0; i<size; i++){
		send(fds[i], buf, strlen(buf)+1, 0);
		close(fds[i]);
	}
	puts("Server has closed!");
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
	sprintf(buf3, "\33[;45m%s client_%d: %s\33[0m ", "Welcome", index+1, ip[index]);
	for (int i=0; i<size; i++){
        if (fds[i]!=0 && fds[i]!=fd){
           	printf("send to client_%d\n",fds[i]);
           	send(fds[i], buf3, strlen(buf3)+1, 0);
        }
   	}
   	//�������Ŀͻ��˷���Ϣ��ʾ�ɹ�����������
	sprintf(buf4, "\33[;45m%s to our chatroom! You are client_%d: %s\33[0m ", "Welcome", index+1, ip[index]); 
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
			sprintf(buf2, "client_%d: %s \33[;31m%s\33[0m ", index+1, 
				ip[index], "has left the chatroom!");
			puts(buf2);
			int i;
			for (i=0; i<size; i++){
		    	if (fds[i]!=0 && fds[i]!=fd)
		        	send(fds[i], buf2, strlen(buf2)+1, 0);
			}
			//send(fds[i], buf2, strlen(buf2)+1, 0);
			//�����߳�
           	pthread_exit((void*)index); //�˴��������� 
		}

		//�������û�������Ϣ
		printf("%s:%s\n", ip[index], buf1);	
		sprintf(buf2, "\33[;42m%s\33[0m client_%d[%s]:%s", "recv",
			index+1, ip[index], buf1);
		for (int i=0; i<size; i++){
        	if (fds[i]!=0 && fds[i]!=fd){
//            	printf("send to client_%d\n", i+1);
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
	printf("Server starts!\n");
	// �ȴ�����
	while(1){
		struct sockaddr_in from_addr;
		int fd = accept(sockfd, (SockAddr*)&from_addr, &len);
		if(0 > fd){
			printf("Error while connecting to the client...\n");
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
				printf("Client %s is successfully connected!\n",ip[i]);
                break;
			}
        }

		if (size == i){
            //���͸��ͻ���˵����������
            char* str = "Sorry, the chatroom is full now!";
            send(fd, str, strlen(str), 0); 
            close(fd);
        }
    }
}
