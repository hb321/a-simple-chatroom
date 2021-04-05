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
#include<iostream>

using namespace std;


int sockfd;

int max_con_num = 5;
int max_join_g_num = 50;
int max_crea_g_num = 20;
int min_str_len = 4;
int max_str_len = 16;
int max_g_size = 100;


void sigint(int signum)
{
	close(sockfd);
	puts("\033[34;1mYou have log out.\033[0m");
	exit(0);
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

void invalidCmd(const string& err_msg){
	puts(err_msg.c_str()+1);
	puts(helpCmds().c_str()+1);
}

//去掉字符串前后的空白字符 
void Trim(string & str){
    string blanks("\f\v\r\t\n ");
    str.erase(0,str.find_first_not_of(blanks));
    str.erase(str.find_last_not_of(blanks) + 1);
}

bool parseNameAndKey(const string& cmd, string& u_name, string& key){
	string cmd_cpy(cmd);
	Trim(cmd_cpy);
	int space_idx = cmd_cpy.find_first_of(string("\f\v\r\t\n "));
	if (space_idx == -1)	return false;//找不到分隔符 
	u_name = cmd_cpy.substr(0, space_idx);
	key = cmd_cpy.substr(space_idx+1);
	Trim(u_name);
	Trim(key);
	if (u_name.size() <= max_str_len && u_name.size() >= 1 && 
		key.size() <= max_str_len && key.size() >= min_str_len){
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

bool parseName(string& name){
	Trim(name);
	if (name.size() <= max_str_len && name.size() >= 1){
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

bool parseNameAndMsg(const string& cmd, string& name, string& msg){
	string cmd_cpy(cmd);
	Trim(cmd_cpy);
	int space_idx = cmd_cpy.find_first_of(string("\f\v\r\t\n "));
	name = cmd_cpy.substr(0, space_idx);
	msg = cmd_cpy.substr(space_idx+1);
	Trim(name);
	Trim(msg);//发消息不以空白字符开始或结束，也不能发空白消息 
	if (name.size() <= max_str_len && name.size() >= 1){
		for (int i = 0; i < name.size(); i++){
			if (!(name[i] >= 'a' && name[i] <= 'z') &&
				!(name[i] >= 'A' && name[i] <= 'Z') &&
				!(name[i] >= '0' && name[i] <= '9') &&
				name[i] != '_')	return false;
		}
		if (msg.size() > 0)	return true;
	}
	else	return false;
}

//解析命令行，判断是否合法，并将合法命令行解析成方便服务端读取的格式 
//返回值-1表示命令行有误，err_msg为错误信息；0表示命令已处理完；
//1表示需要发送cmd_msg给服务端，2表示要发送cmd_msg给服务端，之后自行关闭客户端 
int parseCmd(const string& cmd, string& cmd_msg, string& err_msg){
	cmd_msg = "";
	err_msg = "";
//	puts(cmd.c_str());
//	cout << cmd.size() << endl;
    if (cmd.size() >= 2 && cmd[0] == '-'){
    	//显示帮助信息
//    	cout << (cmd[1] == 'h') << " " << (cmd.size() == 2) << endl;
//    	cout << (cmd[1] == 'q') << " " << (cmd.size() == 2) << endl;
        if (cmd[1] == 'h' && cmd.size() == 2){
        	puts(helpCmds().c_str()+1);
        	return 0;
		}
        //关闭客户端 
        else if (cmd[1] == 'q' && cmd.size() == 2){
        	cmd_msg = "1 q";
        	return 2;
		}
        //退出登录
        else if (cmd[1] == 'l' && cmd[2]=='o' && cmd.size() == 3)	cmd_msg = "1 lo";
        else{
            if (cmd.size() > 4){
                if (cmd[1]=='r'&&cmd[2]=='g'){//注册账号 
                    string u_name, key;
                    bool ret = parseNameAndKey(cmd.substr(3), u_name, key);
                    if (ret)	cmd_msg = "3 rg "+u_name+" "+key;
                    else	err_msg = "0\033[31;1mInvalid user name or key!\033[0m";
                }
                else if (cmd[1]=='l'&&cmd[2]=='g'){//登录账号 
                    string u_name, key;
                    bool ret = parseNameAndKey(cmd.substr(3), u_name, key);
                    if (ret)	cmd_msg = "3 lg "+u_name+" "+key;
                    else	err_msg = "0\033[31;1mInvalid user name or key!\033[0m";
                }
                else if (cmd[1]=='c'&&cmd[2]=='g'){//建群 
                    string g_name(cmd.substr(3));
                    bool ret = parseName(g_name);
                    if (ret)	cmd_msg = "2 cg "+g_name;
                    else	err_msg = "0\033[31;1mInvalid name!\033[0m";
                }
                else if (cmd[1]=='j'&&cmd[2]=='g'){//加群 
                    string g_name(cmd.substr(3));
                    bool ret = parseName(g_name);
                    if (ret)	cmd_msg = "2 jg "+g_name;
                    else	err_msg = "0\033[31;1mInvalid name!\033[0m";
                }
                else if (cmd[1]=='q'&&cmd[2]=='g'){//退群 
                    string g_name(cmd.substr(3));
                    bool ret = parseName(g_name);
                    if (ret)	cmd_msg = "2 qg "+g_name;
                    else	err_msg = "0\033[31;1mInvalid name!\033[0m";
                }
                else if (cmd[1]=='s'&&cmd[2]=='g'){//发群消息 
                    string g_name, msg, err_msg;
                    bool ret = parseNameAndMsg(cmd.substr(3), g_name, msg);
                    if (ret)	cmd_msg = "3 sg "+g_name+" "+msg;
                }
                else if (cmd[1]=='s'&&cmd[2]=='u'){//私聊消息 
                    string u_name, msg;
                    bool ret = parseNameAndMsg(cmd.substr(3), u_name, msg);
                    if (ret)	cmd_msg = "3 su "+u_name+" "+msg;
                }
            }
        } 
    }
    if (cmd_msg.size() > 0)	return 1;
    else{
    	if (err_msg.size() == 0)	err_msg = "0\033[31;1mInvalid command!\033[0m";
    	return -1;
	}
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
        if (buf[0] == '0')
        	printf("\r%s\n", buf+1);
		else{
			printf("\r%s\n", buf+1);
			close(sockfd);
			exit(0);
		}
    }
}

int main()
{
	system("clear");
	signal(SIGINT, sigint);
	signal(SIGQUIT, sigint);
	//创建socket对象
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
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
	if(0>connect(sockfd, (struct sockaddr*)&addr, sizeof(addr))){
		perror("connect");
		return -1;
	}

	pthread_t pid;
    pthread_create(&pid, 0, start, &sockfd);
	//发送数据
	while(1){
		char buf[1024] ={};
		char srt[1000]={};
		fgets(srt, 800, stdin);
		sprintf(buf, srt);
		string cmd(buf), cmd_msg, err_msg;
		Trim(cmd);
		int ret = parseCmd(cmd, cmd_msg, err_msg);
		if (ret == 1)	send(sockfd, cmd_msg.c_str(), cmd_msg.size()+1, 0);
		else if (ret == -1)	invalidCmd(err_msg);
		else if (ret == 2){//向服务端发送消息后退出客户端 
			send(sockfd, cmd_msg.c_str(), cmd_msg.size()+1, 0);
			close(sockfd);
			puts("\033[34;1mYou have log out.\033[0m");
			exit(0);
		}
	}

}
