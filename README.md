# linux-threadqueue
## linux系统中一个用于线程间发送消息（模仿windows系统的UI程序的消息队列）的框架，main.cpp文件内含详细使用教程和步骤
## 编译
在main.cpp文件的目录中使用termainal执行下面的命令（注意使用-pthread编译选项）：</br>
g++ -o a.out main.cpp -pthread
## 运行
在main.cpp文件的目录中使用termainal执行下面的命令（注意使用-pthread编译选项）：</br>
./a.out
## 执行结果
Main thread is runing.</br>
subthread is running.</br>
msg type:0</br>
msg type:1</br>
sub thread is finished.</br>
Main thread is finfshed.</br>

