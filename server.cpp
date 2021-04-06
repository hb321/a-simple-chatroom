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
#include <gperftools/profiler.h>
#include"server.h"
/*
高亮蓝色 \033[34;1m高亮蓝色文字\033[0m
高亮绿色 \033[32;1m高亮绿色文字\033[0m
高亮红色 \033[31;1m高亮红色文字\033[0m
高亮黄色 \033[33;1m高亮黄色文字\033[0m 
*/

typedef struct sockaddr SockAddr;

Server s;
int isStart = 0;

void closeServer(Server& s){
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

void sigint(int signum){
	closeServer(s);
}

//去掉字符串前后的空白字符 
void Trim(string & str){
    string blanks("\f\v\r\t\n ");
    str.erase(0,str.find_first_not_of(blanks));
    str.erase(str.find_last_not_of(blanks) + 1);
}


void userLogout(int fd){
	char buf[1024]={};
	//若当前sock_fd绑定了用户，则让该用户退登 
	if (s.fd_user_dict.find(fd) != s.fd_user_dict.end()){
		string u_name = s.fd_user_dict[fd];
		s.fd_user_dict.erase(fd);
		s.user_fd_dict.erase(u_name);
		s.user_dict[u_name].online = false;
		//提示用户退出
		sprintf(buf, "0\033[34;1mLogout successfully.\033[0m");
		send(fd, buf, strlen(buf)+1, 0);
		//服务端显示用户退出登录 
		sprintf(buf, "\33[34;1mUser \"%s\" logout.\33[0m", u_name.c_str());
		puts(buf);
	}
	else{
		//提示错误信息
		sprintf(buf, "0\033[31;1mYou haven't login yet!\033[0m");
		send(fd, buf, strlen(buf)+1, 0);
	}
}

//客户端关闭
void clientClose(int fd){
	char buf[1024]={};
	//若当前sock_fd绑定了用户，则需要让该用户退登 
	if (s.fd_user_dict.find(fd) != s.fd_user_dict.end()){
		string u_name = s.fd_user_dict[fd];
		s.fd_user_dict.erase(fd);
		s.user_fd_dict.erase(u_name);
		s.user_dict[u_name].online = false;
		//提示用户退出
		sprintf(buf, "2\033[34;1mLogout.\033[0m");
		send(fd, buf, strlen(buf)+1, 0);
		//服务端显示用户退出登录 
		sprintf(buf, "\33[34;1mUser \"%s\" logout.\33[0m", u_name.c_str());
		puts(buf);
	}
	//关闭连接 
	sprintf(buf, "2\033[34;1mClient has been closed.\033[0m");
	send(fd, buf, strlen(buf)+1, 0);
	close(fd);
	//退出后将位置空出来
	s.sock_fds.remove(fd);
	sprintf(buf, "\033[34;1mA connection(%d) has been closed.\033[0m", fd);
	puts(buf);
}


string helpCmds(){
	string str("0\033[34;1m");
	str += "-h                help, print usage of commands\n";
	str += "-lg u_name key    user u_name logins with key\n";
	str += "-rg u_name key    user u_name registers with key\n";
	str += "-lo               user logout\n";
	str += "-cg g_name        create group g_name\n";
	str += "-jg g_name        join group g_name\n";
	str += "-qg g_name        quit group g_name\n";
	str += "-sg g_name msg    send message msg to all online members in group g_name\n";
	str += "-su u_name msg    send message msg to online user u_name\n";
	str += "-q                close the client\n";
	str += "\033[0m";
	return str;
}

void invalidCmd(int fd){
//	cout << fd << endl;
	char buf[1024]={};
	sprintf(buf, "0\033[31;1mInvalid command!\033[0m");
	send(fd, buf, strlen(buf)+1, 0);
	sprintf(buf, helpCmds().c_str());
	send(fd, buf, strlen(buf)+1, 0);
}


bool listSearch(list<string>& l, const string& ele){
	list<string>::iterator iter = l.begin();
	while(iter != l.end()){
		if (*iter == ele)	return true;
		iter++;
	}
	return false;
}


bool userRegister(int fd, const string& u_name, const string& key){
	if (s.fd_user_dict.find(fd) != s.fd_user_dict.end()){
		string err_msg = "0\033[31;1mPlease logout before registering!\033[0m";
		send(fd, err_msg.c_str(), err_msg.size()+1, 0);
		return false;
	}
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
	string err_msg;
	if (s.fd_user_dict.find(fd) != s.fd_user_dict.end()){
		err_msg = "0\033[31;1mPlease logout before logining!\033[0m";
		send(fd, err_msg.c_str(), err_msg.size()+1, 0);
		return false;
	}
	if (s.user_dict.find(u_name) != s.user_dict.end()){
		if (key == s.user_dict[u_name].key){
			if (s.user_dict[u_name].online){//当前账号在异地登录，抢占 
				int other_fd = s.user_fd_dict[u_name];
				userLogout(other_fd);
				err_msg = "0\033[31;1mYour account logins at another client!\033[0m";
				send(other_fd, err_msg.c_str(), err_msg.size()+1, 0);
			}
			s.fd_user_dict[fd] = u_name;
			s.user_fd_dict[u_name] = fd;
			s.user_dict[u_name].online = true;
			string msg = "0\033[34;1mYou("+u_name+") successfully Login!\033[0m";
		    send(fd, msg.c_str(), msg.size()+1, 0);
			return true;
		}
		else	err_msg = "0\033[31;1mError password!\033[0m";
	}
	else	err_msg = "0\033[31;1mThe account "+u_name+" doesn't' exist!\033[0m";

	send(fd, err_msg.c_str(), err_msg.size()+1, 0);
	return false;
}

//fd对应的用户建群 
bool createGroup(int fd, const string& g_name){
	string err_msg, msg;
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
				msg = "0\033[34;1mThe group "+g_name+" is created successfully!\033[0m";
	    		send(fd, msg.c_str(), msg.size()+1, 0);
	    		return true;
			}
			else	err_msg = "0\033[31;1mThe group "+g_name+" already exists!\033[0m";			
		}
		else	err_msg = "0\033[31;1mThe groups you have created achieve maximum!\033[0m";
	}
	else	err_msg = "0\033[31;1mPlease login first!\033[0m";
	send(fd, err_msg.c_str(), err_msg.size()+1, 0);
	return false;
}


bool joinGroup(int fd, const string& g_name){
	string err_msg, msg;
	if (s.fd_user_dict.find(fd) != s.fd_user_dict.end()){
		string u_name = s.fd_user_dict[fd];
		if (s.user_dict[u_name].join_groups.size()<s.max_join_g_num){
			if (s.group_dict.find(g_name) != s.group_dict.end()){
				if (!listSearch(s.group_dict[g_name].members, u_name)){
					if (s.group_dict[g_name].members.size() < s.max_g_size){
						s.group_dict[g_name].members.push_back(u_name);
						s.user_dict[u_name].join_groups.push_back(g_name);
						
						msg = "0\033[34;1mYou have joined group "+g_name+" successfully!\033[0m";
			    		send(fd, msg.c_str(), msg.size()+1, 0);
			    		
			    		//通知其他在线群成员 
			    		list<string>::iterator iter = s.group_dict[g_name].members.begin();
	                    for (; iter != s.group_dict[g_name].members.end(); iter++){
	                    	if (*iter == u_name)	continue;
	                    	if (s.user_dict[*iter].online){//给在线用户发通知 
	                    		int u_fd = s.user_fd_dict[*iter];
	                    		msg = "0\033[34;1mFrom group \""+g_name+"\": User \""+
									u_name+"\" join group\033[0m";
	                    		send(u_fd, msg.c_str(), msg.size()+1, 0);
							}
						}
			    		return true;
					}
					else	err_msg = "0\033[31;1mThe group is full!\033[0m";
				}
				else	err_msg = "0\033[31;1mYou are already in the group!\033[0m";
			}
			else	err_msg = "0\033[31;1mThe group "+g_name+" doesn't exist!\033[0m";
		}
		else	err_msg = "0\033[31;1mThe groups you have joined achieve maximum!\033[0m";
	}
	else	err_msg = "0\033[31;1mPlease login first!\033[0m";
	
	send(fd, err_msg.c_str(), err_msg.size()+1, 0);
	return false;
}

bool quitGroup(int fd, const string& g_name){
	string err_msg, msg;
	if (s.fd_user_dict.find(fd) != s.fd_user_dict.end()){
		string u_name = s.fd_user_dict[fd];
		if (s.group_dict.find(g_name) != s.group_dict.end()){
			if (listSearch(s.group_dict[g_name].members, u_name)){
                if (s.group_dict[g_name].owner == u_name){
                    list<string>::iterator iter = s.group_dict[g_name].members.begin();
                    for (; iter != s.group_dict[g_name].members.end(); iter++){
                    	if (*iter == u_name)	continue;
                    	s.user_dict[*iter].join_groups.remove(g_name);//群成员退群
                    	if (s.user_dict[*iter].online){//给在线用户发通知 
                    		int u_fd = s.user_fd_dict[*iter];
                    		msg = "0\033[34;1mYou left group \""+g_name+
								"\" because it has been disbanded.\033[0m";
                    		send(u_fd, msg.c_str(), msg.size()+1, 0);
						}
					}
                    s.user_dict[u_name].create_groups.remove(g_name);//群主退群
                    s.group_dict.erase(g_name);//删除群聊 
                    msg = "0\033[34;1mGroup "+g_name+" is disbanded successfully!\033[0m";
                    send(fd, msg.c_str(), msg.size()+1, 0);
                    return true;
                }
                else{
                	list<string>::iterator iter = s.group_dict[g_name].members.begin();
                    for (; iter != s.group_dict[g_name].members.end(); iter++){
                    	if (*iter == u_name)	continue;
                    	if (s.user_dict[*iter].online){//给在线用户发通知 
                    		int u_fd = s.user_fd_dict[*iter];
                    		msg = "0\033[34;1mFrom group \""+g_name+"\": User \""+
								u_name+"\" left group\033[0m";
                    		send(u_fd, msg.c_str(), msg.size()+1, 0);
						}
					}
                	
                	s.group_dict[g_name].members.remove(u_name);
                    s.user_dict[u_name].join_groups.remove(g_name);//普通用户退群 
                    
                    msg = "0\033[34;1mYou left group \""+g_name+"\" successfully!\033[0m";
                    send(fd, msg.c_str(), msg.size()+1, 0);
                    return true;
				}
            }
            else    err_msg = "0\033[31;1mYou are not in the group!\033[0m";
        }
        else    err_msg = "0\033[31;1mThe group \""+g_name+"\" doesn't exist!\033[0m";
	}
	else	err_msg = "0\033[31;1mPlease login first!\033[0m";
	
	send(fd, err_msg.c_str(), err_msg.size()+1, 0);
	return false;
}


bool sendGroupMsg(int fd, const string& g_name, const string& msg){
	string err_msg;
	if (s.fd_user_dict.find(fd) != s.fd_user_dict.end()){
		string u_name = s.fd_user_dict[fd];
		if (s.group_dict.find(g_name) != s.group_dict.end()){
			if (listSearch(s.group_dict[g_name].members, u_name)){
                list<string>::iterator iter = s.group_dict[g_name].members.begin();
                for (; iter != s.group_dict[g_name].members.end(); iter++){
                   	if (*iter == u_name)	continue;
                   	if (s.user_dict[*iter].online){//给在线用户发消息 
                   		int u_fd = s.user_fd_dict[*iter];
                   		string g_msg;
						g_msg += "0\033[34;1mFrom \""+*iter+"\"(in group \""+g_name+"\"):\033[0m";
						g_msg += msg;
                   		send(u_fd, g_msg.c_str(), g_msg.size()+1, 0);
					}
					//后续可以实现离线消息 
				}
				return true;
            }
            else    err_msg = "0\033[31;1mYou are not in the group!\033[0m";
        }
        else    err_msg = "0\033[31;1mThe group \""+g_name+"\" doesn't exist!\033[0m";
	}
	else	err_msg = "0\033[31;1mPlease login first!\033[0m";
	
	send(fd, err_msg.c_str(), err_msg.size()+1, 0);
	return false;
}

bool sendUserMsg(int fd, const string& d_name, const string& msg){
	string err_msg;
	if (s.fd_user_dict.find(fd) != s.fd_user_dict.end()){
		string s_name = s.fd_user_dict[fd];
		if (s.user_dict.find(d_name) != s.user_dict.end()){
			if (s_name != d_name){
				if (s.user_dict[d_name].online){//若对方用户在线，向其发消息 
					int u_fd = s.user_fd_dict[d_name];
                   	string u_msg;
					u_msg += "0\033[34;1mFrom \""+s_name+"\":\033[0m";
					u_msg += msg;
                   	send(u_fd, u_msg.c_str(), u_msg.size()+1, 0);
				}
				//后续可以实现离线消息 
				return true;
			}
			else	err_msg = "0\033[31;1mCannot send message to yourself!\033[0m";
		}
		else	err_msg = "0\033[31;1mUser \""+d_name+"\" doesn't exist'!\033[0m";
	}
	else	err_msg = "0\033[31;1mPlease login first!\033[0m";
	
	send(fd, err_msg.c_str(), err_msg.size()+1, 0);
	return false;
}

//返回值为1表示客户端关闭，返回值0表示命令处理成功，返回值2表示命令有误，处理完毕 
int parseCmdMsg(int fd, const string& cmd_msg, bool c_flag){
	//c_flag为true表示命令来自客户端，否则来自服务端 
	char buf[1024]={};
	if (c_flag){
		string content = string(cmd_msg.substr(2));
		if (cmd_msg[0] == '1'){
			if (content == string("q")){//客户端关闭 
				clientClose(fd);
				return 1;
			}
			else if (content == string("lo")){
				userLogout(fd);
				return 0; 
			}
			else	invalidCmd(fd);
		}
		else if (cmd_msg[0] == '2'){
			int space_idx = content.find_first_of(' ');
			string cmd = content.substr(0, space_idx);
			string name = content.substr(space_idx+1);
			if (cmd == string("cg")){
				if (createGroup(fd, name))	return 0;
			}
			else if (cmd == string("jg")){
				if (joinGroup(fd, name))	return 0;
			}
			else if (cmd == string("qg")){
				if (quitGroup(fd, name))	return 0;
			}
			else	invalidCmd(fd);
		}
		else if (cmd_msg[0] == '3'){
			int space_idx1 = content.find_first_of(' ');
			string cmd = string(content.substr(0, space_idx1));
			
			content = content.substr(space_idx1+1);
			int space_idx2 = content.find_first_of(' ');
			string text1 = content.substr(0, space_idx2);;
			string text2 = content.substr(space_idx2+1);
			if (cmd == string("rg")){//注册账号 
				if (userRegister(fd, text1, text2))	return 0;
			}
			else if (cmd == string("lg")){//登录账号 
				if (userLogin(fd, text1, text2))	return 0;
			}
			else if (cmd == string("sg")){//发群消息 
				if (sendGroupMsg(fd, text1, text2))	return 0;
			}
			else if (cmd == string("su")){//私聊消息 
				if (sendUserMsg(fd, text1, text2))	return 0;
			}
			else	invalidCmd(fd);
		}
		else	invalidCmd(fd);
	}
	else{
		if (cmd_msg == string("-q")){
			closeServer(s);
		}
		else if (cmd_msg == string("-lsg")){
			string msg;
			map<string, Group>::iterator iter = s.group_dict.begin();
			for (; iter != s.group_dict.end(); iter++){
				msg += iter->first + "\t";
			}	
			if (msg.size() == 0){
				msg = "NULL";
			}
			msg = "\033[34;1m"+msg+"\033[0m";
			puts(msg.c_str());
		}
		else if (cmd_msg == string("start")){
			isStart = 1;
		}
		else if (cmd_msg == string("end")){
			isStart = 2;
		}
	}
	return 2;
}

void* start(void* p){
	int fd =*(int*)p;
	char buf1[1024] = {}, buf2[1024] = {};
   	//向新来的客户端发消息表示成功连接服务端 
	sprintf(buf2, "0\033[34;1mSuccessfully connect to the server.\033[0m"); 
   	send(fd, buf2, strlen(buf2)+1, 0);
	// 收发数据
	while(1){
		char buf3[1024] = {};
		char buf4[1024] = {};
		//接受信息小于等于零或者服务端准备关闭，则用户退出
 		if (0 >= recv(fd, buf3, sizeof(buf3), 0) || s.close_flag){
 			clientClose(fd); //用户退出登录 
			//结束线程
           	pthread_exit((void*)("Done")); //此处尚有疑问 
		}
		//处理用户发来的命令行 
		string cmd(buf3);
		int ret = parseCmdMsg(fd, cmd, true);
		if (ret == 1)	pthread_exit((void*)("Done"));	//用户退出系统，退出线程 
	}	
}

void* start_server(void* p)
{
	int fd =*(int*)p;
    while(1)
	{
		if (isStart==1){
			ProfilerStart("result.prof");
			isStart = 0;
		}
		else if (isStart == 2){
			ProfilerStop();
			isStart = 0;
		}
		else{
			//处理服务端的命令 
			char buf[1024] ={}, srt[1024]={};
			fgets(srt, 1000, stdin);
			sprintf(buf, srt);
			string cmd(buf);
			Trim(cmd);
			parseCmdMsg(fd, cmd, false);//这里的fd实际上是用不到的，返回值也是无效的
		}
    }
}

int main(){
	system("clear");
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
	
	pthread_t pid;
    pthread_create(&pid, 0, start_server, &sockfd);
    
	// 等待连接
	while(1){
		//处理客户端的连接 
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
