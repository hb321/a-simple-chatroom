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
������ɫ \033[34;1m������ɫ����\033[0m
������ɫ \033[32;1m������ɫ����\033[0m
������ɫ \033[31;1m������ɫ����\033[0m
������ɫ \033[33;1m������ɫ����\033[0m 
*/

typedef struct sockaddr SockAddr;

Server s;
//int fds[20];
//char ip[20][20];
//int size = 20;

void closeServer(Server& s){
	s.close_flag = true;//����˼����ر�
	//��ͻ��˷���ǿ�ƹر�֪ͨ����߹ر�ȫ�����ӣ��ͻ�����Ҫ�Լ��رճ��� 
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

//ȥ���ַ���ǰ��Ŀհ��ַ� 
void Trim(string & str){
    string blanks("\f\v\r\t\n ");
    str.erase(0,str.find_first_not_of(blanks));
    str.erase(str.find_last_not_of(blanks) + 1);
}


void userLogout(int fd){
	char buf[1024]={};
	//����ǰsock_fd�����û������ø��û��˵� 
	if (s.fd_user_dict.find(fd) != s.fd_user_dict.end()){
		string u_name = s.fd_user_dict[fd];
		s.fd_user_dict.erase(fd);
		s.user_fd_dict.erase(u_name);
		s.user_dict[u_name].online = false;
		//��ʾ�û��˳�
		sprintf(buf, "0\033[34;1mLogout successfully.\033[0m");
		send(fd, buf, strlen(buf)+1, 0);
		//�������ʾ�û��˳���¼ 
		sprintf(buf, "\33[34;1mA user(%s) logout.\33[0m", u_name.c_str());
		puts(buf);
	}
	else{
		//��ʾ������Ϣ
		sprintf(buf, "0\033[31;1mYou haven't login yet!\033[0m");
		send(fd, buf, strlen(buf)+1, 0);
	}
}

//�رտͻ��� 
void clientClose(int fd){
	char buf[1024]={};
	//����ǰsock_fd�����û�������Ҫ�ø��û��˵� 
	if (s.fd_user_dict.find(fd) != s.fd_user_dict.end()){
		string u_name = s.fd_user_dict[fd];
		s.fd_user_dict.erase(fd);
		s.user_fd_dict.erase(u_name);
		s.user_dict[u_name].online = false;
		//��ʾ�û��˳�
		sprintf(buf, "2\033[34;1mLogout.\033[0m");
		send(fd, buf, strlen(buf)+1, 0);
		//�������ʾ�û��˳���¼ 
		sprintf(buf, "\33[34;1mA user(%s) logout.\33[0m", u_name.c_str());
		puts(buf);
	}
	//�ر����� 
	sprintf(buf, "2\033[34;1mClient has been closed.\033[0m");
	send(fd, buf, strlen(buf)+1, 0);
	close(fd);
	//�˳���λ�ÿճ���
	s.sock_fds.remove(fd);
	sprintf(buf, "\033[34;1mA connection(%d) has been closed.\033[0m", fd);
	puts(buf);
}


string helpCmds(){
	string str("0");
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

bool parseName(string& name, string& err_msg){
	//�ݲ�ʵ�־�������Ӧ��err_msg 
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

bool parseNameAndKey(const string& cmd, string& u_name, string& key, string& err_msg){
	//�ݲ�ʵ�־�������Ӧ��err_msg
	string cmd_cpy(cmd);
	Trim(cmd_cpy);
	int space_idx = cmd_cpy.find_first_of(string("\f\v\r\t\n "));
	if (space_idx == -1)	return false;//�Ҳ����ָ��� 
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
//		cout << "*" << u_name << "**" << key << "*" << endl; 
		return true;
	}
	else	return false;
}

bool parseNameAndMsg(const string& cmd, string& name, string& msg, string& err_msg){
	//�ݲ�ʵ�־�������Ӧ��err_msg
	string cmd_cpy(cmd);
	Trim(cmd_cpy);
	int space_idx = cmd_cpy.find_first_of(string("\f\v\r\t\n "));
	name = cmd_cpy.substr(0, space_idx);
	msg = cmd_cpy.substr(space_idx+1);
	Trim(name);
	Trim(msg);//����Ϣ���Կհ��ַ���ʼ�������Ҳ���ܷ��հ���Ϣ 
	if (name.size() <= s.max_str_len && name.size() >= 1){
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

//fd��Ӧ���û���Ⱥ 
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
                    	s.user_dict[*iter].join_groups.remove(g_name);//Ⱥ��Ա��Ⱥ
                    	if (s.user_dict[*iter].online){//�������û���֪ͨ 
                    		int u_fd = s.user_fd_dict[*iter];
                    		msg = "0\033[34;1mYou left group \""+g_name+
								"\" because it has been disbanded.\033[0m";
                    		send(u_fd, msg.c_str(), msg.size()+1, 0);
						}
					}
                    s.user_dict[u_name].create_groups.remove(g_name);//Ⱥ����Ⱥ
                    s.group_dict.erase(g_name);//ɾ��Ⱥ�� 
                    msg = "0\033[34;1mGroup "+g_name+" is disbanded successfully!\033[0m";
                    send(fd, msg.c_str(), msg.size()+1, 0);
                    return true;
                }
                else{
                	s.group_dict[g_name].members.remove(u_name);
                    s.user_dict[u_name].join_groups.remove(g_name);//��ͨ�û���Ⱥ 
                    
                    msg = "0\033[34;1mYou left group "+g_name+" successfully!\033[0m";
                    send(fd, msg.c_str(), msg.size()+1, 0);
                    return true;
				}
            }
            else    err_msg = "0\033[31;1mYou are not in the group!\033[0m";
        }
        else    err_msg = "0\033[31;1mThe group "+g_name+" doesn't exist!\033[0m";
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
                   	if (s.user_dict[*iter].online){//�������û�����Ϣ 
                   		int u_fd = s.user_fd_dict[*iter];
                   		string g_msg;
						g_msg += "0\033[34;1mFrom "+*iter+"(in "+g_name+"):\033[0m";
						g_msg += msg;
                   		send(u_fd, g_msg.c_str(), g_msg.size()+1, 0);
					}
					//��������ʵ��������Ϣ 
				}
				return true;
            }
            else    err_msg = "0\033[31;1mYou are not in the group!\033[0m";
        }
        else    err_msg = "0\033[31;1mThe group "+g_name+" doesn't exist!\033[0m";
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
				if (s.user_dict[d_name].online){//���Է��û����ߣ����䷢��Ϣ 
					int u_fd = s.user_fd_dict[d_name];
                   	string u_msg;
					u_msg += "0\033[34;1mFrom "+s_name+":\033[0m";
					u_msg += msg;
                   	send(u_fd, u_msg.c_str(), u_msg.size()+1, 0);
				}
				//��������ʵ��������Ϣ 
				return true;
			}
			else	err_msg = "0\033[31;1mCannot send message to yourself!\033[0m";
		}
		else	err_msg = "0\033[31;1mUser "+d_name+" doesn't exist'!\033[0m";
	}
	else	err_msg = "0\033[31;1mPlease login first!\033[0m";
	
	send(fd, err_msg.c_str(), err_msg.size()+1, 0);
	return false;
}

int parseCmd(int fd, const string& cmd, bool c_flag){
	//c_flagΪtrue��ʾ�������Կͻ��ˣ��������Է���� 
	char buf[1024]={};
	if (c_flag){
		if (cmd.size() < 2)	invalidCmd(fd);
		if (cmd[0] == '-'){
		    if (cmd[1] == 'h' && cmd.size() == 2){
		        sprintf(buf, helpCmds().c_str());
		        send(fd, buf, strlen(buf)+1, 0);
		    }
		    else if (cmd[1] == 'q' && cmd.size() == 2){
				clientClose(fd);
				return 1;
			}
			else if (cmd[1] == 'l' && cmd[2]=='o' && cmd.size() == 3){
				userLogout(fd);
			}
		    else{
		    	if (cmd.size() > 4){
					if (cmd[1]=='r'&&cmd[2]=='g'){//ע���˺� 
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
					else if (cmd[1]=='l'&&cmd[2]=='g'){//��¼�˺� 
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
					else if (cmd[1]=='c'&&cmd[2]=='g'){//��Ⱥ 
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
					else if (cmd[1]=='j'&&cmd[2]=='g'){//��Ⱥ 
						string g_name(cmd.substr(3)), err_msg;
		    			bool ret = parseName(g_name, err_msg);
		    			if (ret){
		    				if (joinGroup(fd, g_name))	return 0;
						}
		    			else{
		    				err_msg = "0\033[31;1mInvalid name!\033[0m";
		    				send(fd, err_msg.c_str(), err_msg.size()+1, 0);
						}
					}
					else if (cmd[1]=='q'&&cmd[2]=='g'){//��Ⱥ 
						string g_name(cmd.substr(3)), err_msg;
		    			bool ret = parseName(g_name, err_msg);
		    			if (ret){
		    				if (quitGroup(fd, g_name))	return 0;
						}
		    			else{
		    				err_msg = "0\033[31;1mInvalid name!\033[0m";
		    				send(fd, err_msg.c_str(), err_msg.size()+1, 0);
						}
					}
					else if (cmd[1]=='s'&&cmd[2]=='g'){//��Ⱥ��Ϣ 
		    			string g_name, msg, err_msg;
						bool ret = parseNameAndMsg(cmd.substr(3), g_name, msg, err_msg);
						if (ret){
							if (sendGroupMsg(fd, g_name, msg))	return 0;
						}
						else	invalidCmd(fd);
					}
					else if (cmd[1]=='s'&&cmd[2]=='u'){//˽����Ϣ 
						string u_name, msg, err_msg;
						bool ret = parseNameAndMsg(cmd.substr(3), u_name, msg, err_msg);
						if (ret){
							if (sendUserMsg(fd, u_name, msg))	return 0;
						}
						else	invalidCmd(fd);
					}
					else{
						//��������ʶ�������ݲ�ʵ�֣���Ϊ�Ƿ� 
						 invalidCmd(fd);
					}
				}
				else	invalidCmd(fd);
			} 
		}
		else	invalidCmd(fd);
	}
	else{
		if (cmd[0] == '-' && cmd[1] == 'q' && cmd.size()==2){
			closeServer(s);
		}
	}
	return 2;
}



void* start(void* p){
	int fd =*(int*)p;
	char buf1[1024] = {}, buf2[1024] = {};

//	sprintf(buf1, "0\033[34;1mA new client has joined the chatroom.\033[0m");
//	list<int>::iterator iter;
//	for (iter = s.sock_fds.begin(); iter != s.sock_fds.end(); iter++){
//		if (*iter != fd){
//			send(*iter, buf1, strlen(buf1)+1, 0);
//		}
//	}

   	//�������Ŀͻ��˷���Ϣ��ʾ�ɹ�����������
	sprintf(buf2, "0\033[34;1mSuccessfully connect to the server.\033[0m"); 
   	send(fd, buf2, strlen(buf2)+1, 0);
   	
	// �շ�����
	while(1){
		char buf3[1024] = {};
		char buf4[1024] = {};

		//������ϢС�ڵ�������߷����׼���رգ����û��˳�
 		if (0 >= recv(fd, buf3, sizeof(buf3), 0) || s.close_flag){
 			
 			clientClose(fd); //�û��˳���¼ 
			//�����߳�
           	pthread_exit((void*)("Done")); //�˴��������� 
		}

		//�����û������������� 
		string cmd(buf3);
		Trim(cmd);
		int ret = parseCmd(fd, cmd, true);
		if (ret == 1)	pthread_exit((void*)("Done"));	//�û��˳�ϵͳ���˳��߳� 
	}	

}

void* start_server(void* p)
{
	int fd =*(int*)p;
    while(1)
	{
        //�������˵����� 
		char buf[1024] ={}, srt[1024]={};
		fgets(srt, 1000, stdin);
		sprintf(buf, srt);
		string cmd(buf);
		Trim(cmd);
		parseCmd(fd, cmd, false);//�����fdʵ�������ò�����
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
	s.sockfd = sockfd;
	
	// ׼��ͨ�ŵ�ַ
	struct sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(1710);
	addr.sin_addr.s_addr = inet_addr("172.18.167.136");
	// ��socket������ͨ�ŵ�ַ
	socklen_t len = sizeof(addr);
	if(0 > bind(sockfd, (SockAddr*)&addr, len)){
		perror("bind");
		return -1;
	}
	// ���ü���socket����
	listen(sockfd, s.max_con_num);
	printf("\33[34;1mServer starts!\33[0m\n");
	
	pthread_t pid;
    pthread_create(&pid, 0, start_server, &sockfd);
    
	// �ȴ�����
	while(1){
		//����ͻ��˵����� 
		if (!s.close_flag){
			struct sockaddr_in from_addr;
			int fd = accept(sockfd, (SockAddr*)&from_addr, &len);
			if(0 > fd){
				printf("\33[31;1mError while connecting to the client...\33[0m\n");
				continue;
			}
			if (s.sock_fds.size() < s.max_con_num){
				s.sock_fds.push_back(fd); //socket������ӵ�ǰ�ͻ��� 
				//�пͻ�������֮�������̸߳��˿ͻ�����
	            pthread_t pid;
				pthread_create(&pid, NULL, start, &fd);
	//			strcpy(ip[i], inet_ntoa(from_addr.sin_addr));
				printf("\33[34;1mClient %s:%d is successfully connected!\33[0m\n", 
					inet_ntoa(from_addr.sin_addr), from_addr.sin_port);
			}
			else{
				//���͸��ͻ���˵����������
	            char* str = "1\33[31;1mSorry, the server is busy now!\33[0m";
	            send(fd, str, strlen(str), 0); 
	            close(fd);
			}    
		}
		
    }
}
