#include<iostream>
#include<map>
#include<list>
#include"user.h"
#include"group.h"
using namespace std;
struct Server{
	public:
    int sockfd;
    map<string, User> user_dict;
    list<int> sock_fds;
    map<int, string> fd_user_dict;
    map<string, int> user_fd_dict;
    map<string, Group> group_dict;
    bool close_flag;
    
    int max_con_num;
    int max_join_g_num;
    int max_crea_g_num;
    int min_str_len;
    int max_str_len;
    
    Server();
    void saveData(); //将数据保存到文件中 
//    void closeSocket(int sock_fd);
//	void sigint(int signum);
};
Server::Server(){
	close_flag = false;
	max_con_num = 2;
	max_join_g_num = 50;
	max_crea_g_num = 20;
	min_str_len = 4;
	max_str_len = 16;
}
void Server::saveData(){
	//pass
}
