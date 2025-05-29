# 编译说明
目前 sr_demo 支持 Android/Linux 版本编译，统一通过 build.sh 脚本编译，build.sh 帮助信息如下：
```shell
$ ./build.sh 
./build.sh 
./build.sh -t <target> -a <arch> -s <sdk> -b <build_type>
Please select target sys: Linux or Android

    -s : system (Linux/Android)
    -a : arch (linux: aarch64/armhf; android: arm64-v8a/armeabi-v7a)
    -n : sdk name(vendor-storage-test)
    -b : build_type(Debug/Release/RelWithDebInfo)
```

编译之前请先配置 env_android.sh / env_linux.sh 文件，指定编译工具链地址。
- Android 请配置NDK目录地址
- Linux 请配置 gcc 目录地址

## Android 编译命令
```shell
./build.sh -s Android -a arm64-v8a -b Release
./build.sh -s Android -a armeabi-v7a -b Release
```

## Linux 编译命令
```shell
./build.sh -s Linux -a aarch64 -b Release
./build.sh -s Linux -a armhf -b Release
```


## 目录文件说明
```shell
.
├── build.sh        ## 编译脚本
├── CMakeLists.txt  ## cmake编译文件
├── env_android.sh  ## Android NDK编译工具链路径配置脚本
├── env_linux.sh    ## Linux gcc编译工具链路径配置脚本
├── main.cpp        ## vendor-storage-test 源码
└── README.md       ## READEME
```