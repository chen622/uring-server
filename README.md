# io_uring 与 epoll 性能对比测试

本测试对比了作为HTTP Server时，两种io框架的性能差距。为节省工作量，io_uring 使用了 liburing 包来避免需要修改底层参数。

## 使用教程

### 1. 安装liburing

```bash
wget  https://github.com/axboe/liburing/archive/liburing-0.7.zip
unzip liburing-0.7.zip
cd liburing-liburing-0.7/
./configure --libdir=/usr/lib64 
make CFLAGS=-std=gnu99 && make install
```

### 2. 编译测试程序

```bash
cd build
cmake ..
cmake --build .
```

### 3. 运行测试程序

```bash
./uring_server
# or
./epoll_server
```

### 4. 使用jmeter进行压测

首先下载jmeter，之后载入 `压测线程.jmx`



## 调研报告

[跳转链接](/调研报告.md)

