# linux-thread_queue
linux系统中一个用于线程间发送消息（模仿windows系统的UI程序的消息队列）的框架，main.cpp文件内含详细使用教程和步骤（周末帮同学做的）。</br>
## 编译
在main.cpp文件的目录中使用termainal执行下面的命令（注意使用-pthread编译选项）：</br>
g++ -o main.out main.cpp -pthread
## 运行
在main.cpp文件的目录中使用termainal执行下面的命令：</br>
./main.out
## 执行结果(可能)
Main thread is runing.</br>
Sub thread is running.</br>
Buy a package of instant noodles.</br>
Get msg type:1</br>
123456</br>
Get msg type:2</br>
age:18</br>
humidity:45.2</br>
high:199.3</br>
tempture:36.5</br>
length:206</br>
0 1 2 3 4 5 6 7 8 9 </br>
Get msg type:3</br>
Finish sub thread.</br>
Get msg type:0</br>
Sub thread is finished.</br>
Main thread is finfshed.</br>

