### 项目简介
这是一个实现简单聊天室的小项目。需要实现的功能如下：
* 两个在线用户之间的私聊对话
* 用户在聊天室公共频道上发布公开消息，其他聊天室内的成员可以接收到。

### 更新记录

#### 4/1

1. 功能介绍

   ​		基于[这篇博客](https://blog.csdn.net/weixin_38663899/article/details/87884938?utm_term=socket%E7%BC%96%E7%A8%8BC%E8%AF%AD%E8%A8%80%E5%AE%9E%E7%8E%B0%E8%81%8A%E5%A4%A9%E7%A8%8B%E5%BA%8F&utm_medium=distribute.pc_aggpage_search_result.none-task-blog-2~all~sobaiduweb~default-4-87884938&spm=3001.4430)的代码开发本项目，目前代码实现的功能如下：代码分为服务端和客户端，所有客户端组成一个聊天室（最大容量为`size`）。服务器启动时聊天室就创建，客户端连接服务器则加入了聊天室（也就是说，聊天室的容量等于服务器的连接量），客户端断开连接处于离线状态则退出了聊天室。用户可以在聊天频道里对所有成员发消息。

2. 暂不支持的功能以及存在的问题

   ​		暂未实现用户之间的私聊对话以及服务器主动向客户端发消息。

3. 后续开发思路：

   * 点对点的通信需要对客户端进行命名，服务端需要能提供在线用户名单
   * 客户端和服务端需要支持命令行，以支持服务端对客户端发消息（系统消息），客户端对客户端发消息（私聊消息）
   * 聊天室独立出来，由服务器系统管理，聊天室限制最大容量，服务器限制最大连接量