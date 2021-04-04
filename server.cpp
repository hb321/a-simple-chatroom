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
	sprintf(buf, "1\33[34;1mServer has been closed!\nYou have log out!\33[0m");
	
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

//去掉字符串前后的空白字符 
void Trim(string & str){
    string blanks("\f\v\r\t\n ");
    str.erase(0,str.find_first_not_of(blanks));
    str.erase(str.find_last_not_of(blanks) + 1);
}

//用户退出
void clientLogout(int fd){
	char buf[1024]={};
	sprintf(buf, "2\033[34;1mLogout.\033[0m");
	send(fd, buf, strlen(buf)+1, 0);
	close(fd);
	//退出后将位置空出来
	s.sock_fds.remove(fd);
	//若当前sock_fd绑定了用户，则需要让该用户退登 
	if (s.fd_user_dict.find(fd) != s.fd_user_dict.end()){
		string u_name = s.fd_user_dict[fd];
		s.fd_user_dict.erase(fd);
		s.user_fd_dict.erase(u_name);
		s.user_dict[u_name].online = false;
		//提示用户退出
		sprintf(buf, "\33[34;1mA user(%s) logout.\33[0m", u_name.c_str());
		puts(buf);
	}
	else{
		//提示用户退出
		sprintf(buf, "\33[34;1mA client(%d) logout.\33[0m", fd);
		puts(buf);
	}
}

string helpCmds(){
	string str("0");
	str += "-h                help, print usage of commands\n";
	str += "-lg u_name key    user u_name logins with key\n";
	str += "-rg u_name key    user u_name registers with key\n";
	str += "-cg g_name        create group g_name\n";
	str += "-jg g_name        join group g_name\n";
	str += "-qg g_name        quit group g_name\n";
	str += "-sg g_name msg    send message msg to all online members in group g_name\n";
	str += "-q                close the client\n";
	return str;
}

void invalidCmd(int fd){
	cout << fd << endl;
	char buf[1024]={};
	sprintf(buf, "0\033[31;1mInvalid command!\033[0m");
	send(fd, buf, strlen(buf)+1, 0);
	sprintf(buf, helpCmds().c_str());
	send(fd, buf, strlen(buf)+1, 0);
}

bool parseName(string& name, string& err_msg){
	//暂不实现具体错误对应的err_msg 
	Trim(name);
	if (name.size() <= s.max_str_len && name.size() >= 1){
		for (int i = 0; i < name.size(); i++){
			if (!(name[i] >= 'a' && name[i] <= 'z') &&
				!(name[i] >= 'A' && name[i] <= 'Z') &&
				!(name[i] >= '0' && name[i] <= '9') &&
				name[i] != '_')	return false;
		}
		return true;
	}
	else	return false;
}

//fd对应的用户建群 
bool createGroup(int fd, const string& g_name){
	if (s.fd_user_dict.find(fd) != s.fd_user_dict.end()){
		string u_name = s.fd_user_dict[fd];
		if (s.user_dict[u_name].create_groups.size()<s.max_crea_g_num){
			if (s.group_dict.find(g_name) == s.group_dict.end()){
				Group g;
				g.name = g_name;
				g.owner = u_name;
				g.members.push_back(u_name);
				s.user_dict[u_name].create_groups.push_back(u_name);
				s.group_dict[g_name] = g;
				string msg = "0\033[34;1mThe group "+g_name+" is created successfully!\033[0m";
	    		send(fd, msg.c_str(), msg.size()+1, 0);
	    		return true;
			}
			else{
				string err_msg = "0\033[31;1mThe group "+g_name+" already exists!\033[0m";
	    		send(fd, err_msg.c_str(), err_msg.size()+1, 0);
			}
		}
		else{
			string err_msg = "0\033[31;1mThe groups you have created achieve maximum!\033[0m";
	    	send(fd, err_msg.c_str(), err_msg.size()+1, 0);
		}
	}
	else{
		string err_msg = "0\033[31;1mPlease login first!\033[0m";
	    send(fd, err_msg.c_str(), err_msg.size()+1, 0);
	}
	return false;
}

bool parseNameAndKey(const string& cmd, string& u_name, string& key, string& err_msg){
	//暂不实现具体错误对应的err_msg
	string cmd_cpy(cmd);
	Trim(cmd_cpy);
	int space_idx = cmd_cpy.find_first_of(string("\f\v\r\t\n "));
	u_name = cmd_cpy.substr(0, space_idx);
	key = cmd_cpy.substr(space_idx+1);
	Trim(u_name);
	Trim(key);
	if (u_name.size() <= s.max_str_len && u_name.size() >= 1 && 
		key.size() <= s.max_str_len && key.size() >= s.min_str_len){
		for (int i = 0; i < u_name.size(); i++){
			if (!(u_name[i] >= 'a' && u_name[i] <= 'z') &&
				!(u_name[i] >= 'A' && u_name[i] <= 'Z') &&
				!(u_name[i] >= '0' && u_name[i] <= '9') &&
				u_name[i] != '_')	return false;
		}
		for (int i = 0; i < key.size(); i++){
			if (!(key[i] >= 'a' && key[i] <= 'z') &&
				!(key[i] >= 'A' && key[i] <= 'Z') &&
				!(key[i] >= '0' && key[i] <= '9') &&
				key[i] != '_')	return false;
		}
		return true;
	}
	else	return false;
}

bool userRegister(int fd, const string& u_name, const string& key){
	if (s.user_dict.find(u_name) == s.user_dict.end()){
		User u;
		u.name = u_name;
		u.key = key;
		u.online = true;
		s.user_dict[u_name] = u;
		
		string msg = "0\033[34;1mThe account "+u_name+" is successfully registered!\033[0m";
	    send(fd, msg.c_str(), msg.size()+1, 0);
	    return true;
	}
	else{
		string err_msg = "0\033[31;1mThe user name "+u_name+" already exists!\033[0m";
		send(fd, err_msg.c_str(), err_msg.size()+1, 0);
	}
	return false;
}

bool userLogin(int fd, const string& u_name, const string& key){
	if (s.user_dict.find(u_name) != s.user_dict.end()){
		if (key == s.user_dict[u_name].key){
			s.fd_user_dict[fd] = u_name;
			s.user_fd_dict[u_name] = fd;
			s.user_dict[u_name].online = true;
			string msg = "0\033[34;1mYou("+u_name+") successfully Login!\033[0m";
	    	send(fd, msg.c_str(), msg.size()+1, 0);
			return true;
		}
		else{
			string err_msg = "0\033[31;1mError password!\033[0m";
			send(fd, err_msg.c_str(), err_msg.size()+1, 0);
		}
	}
	else{
		string err_msg = "0\033[31;1mThe account "+u_name+" doesn't' exist!\033[0m";
		send(fd, err_msg.c_str(), err_msg.size()+1, 0);
	}
	return false;
}

int parseCmd(int fd, const string& cmd){
//	puts(cmd.c_str());
	char buf[1024]={};
	if (cmd.size() < 2)	invalidCmd(fd);
	if (cmd[0] == '-'){
	    if (cmd[1] == 'h' && cmd.size() == 2){
	        sprintf(buf, helpCmds().c_str());
	        send(fd, buf, strlen(buf)+1, 0);
	    }
	    else if (cmd[1] == 'q' && cmd.size() == 2){
			clientLogout(fd);
			return 1;
		}
	    else{
	    	if (cmd.size() > 4){
	    		if (cmd[1]=='c'&&cmd[2]=='g'){//建群 
	    			string g_name(cmd.substr(3)), err_msg;
	    			bool ret = parseName(g_name, err_msg);
	    			if (ret){
	    				if (createGroup(fd, g_name))	return 0;
					}	
	    			else{
	    				err_msg = "0\033[31;1mInvalid name!\033[0m";
	    				send(fd, err_msg.c_str(), err_msg.size()+1, 0);
					}
				}
				else if (cmd[1]=='r'&&cmd[2]=='g'){//注册账号 
					string u_name, key, err_msg;
					bool ret = parseNameAndKey(cmd.substr(3), u_name, key, err_msg);
					if (ret){
						if (userRegister(fd, u_name, key))	return 0;
					}
					else{
						err_msg = "0\033[31;1mInvalid user name or key!\033[0m";
	    				send(fd, err_msg.c_str(), err_msg.size()+1, 0);
					}
				}
				else if (cmd[1]=='l'&&cmd[2]=='g'){//登录账号 
					string u_name, key, err_msg;
					bool ret = parseNameAndKey(cmd.substr(3), u_name, key, err_msg);
					if (ret){
						if (userLogin(fd, u_name, key))	return 0;
					}
					else{
						err_msg = "0\033[31;1mInvalid user name or key!\033[0m";
	    				send(fd, err_msg.c_str(), err_msg.size()+1, 0);
					}
				}
				else{
					//其他命令识别区，暂不实现，视为非法 
					 invalidCmd(fd);
				}
			}
			else	invalidCmd(fd);
		} 
	}
	else	invalidCmd(fd);
	return 2;
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
		int(s.sock_fds.size())); 
   	send(fd, buf2, strlen(buf2)+1, 0);
   	
	// 收发数据
	while(1){
		char buf3[1024] = {};
		char buf4[1024] = {};

		//接受信息小于等于零或者服务端准备关闭，则用户退出
 		if (0 >= recv(fd, buf3, sizeof(buf3), 0) || s.close_flag){
 			clientLogout(fd); //用户退出登录 
			//结束线程
           	pthread_exit((void*)("Done")); //此处尚有疑问 
		}

		printf("\33[34;1mFrom client:\33[0m %s", buf3);
		//处理用户发来的命令行 
		string cmd(buf3);
		Trim(cmd);
		int ret = parseCmd(fd, cmd);
		if (ret == 1)	pthread_exit((void*)("Done"));	//用户退出系统，退出线程 
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
	            char* str = "1\33[31;1mSorry, the server is busy now!\33[0m";
	            send(fd, str, strlen(str), 0); 
	            close(fd);
			}      
		}
		  
    }
}
