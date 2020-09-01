#ifndef __MYUTILS_H__
#define __MYUTILS_H__

const int LISTENQ = 1024; // 监听队列长度，系统默认为SOMAXCONN,Ubuntu18.04LTS中的值为128

namespace swings
{

    namespace myUtils
    {
    int createListenFd(int port); // 创建监听描述符
    int setNonBlocking(int fd);   // 设置非阻塞模式
    }                             // namespace myUtils

} // namespace swings

#endif