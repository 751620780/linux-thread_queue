# linux-threadqueue
linux系统中一个用于线程间发送消息（模仿windows系统的UI程序的消息队列）的框架，main.cpp文件内含详细使用教程和步骤（周末帮同学做的）。</br>
## 编译
在main.cpp文件的目录中使用termainal执行下面的命令（注意使用-pthread编译选项）：</br>
g++ -o main.out main.cpp -pthread
## 运行
在main.cpp文件的目录中使用termainal执行下面的命令：</br>
./main.out
## 执行结果(可能)
Main thread is runing.</br>
subthread is running.</br>
buy a package paomian.</br>
msg type:1</br>
123456</br>
msg type:2</br>
age:18</br>
humidity:45.2</br>
high:199.2</br>
tempture:36.5</br>
length:206</br>
0</br>
1</br>
2</br>
3</br>
4</br>
5</br>
6</br>
7</br>
8</br>
9</br>
msg type:3</br>
msg type:0</br>
sub thread is finished.</br>
Main thread is finfshed.</br>

