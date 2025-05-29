# 编译说明
目前 sr_demo 支持 Android/Linux 版本编译，统一通过 build.sh 脚本编译，build.sh 帮助信息如下：
```shell
$ ./build.sh 
./build.sh -t <target> -a <arch> -s <sdk> -b <build_type>
Please select target platform: rk356x or rk3588

    -t : target (rk356x/rk3588)
    -s : system (Linux/Android)
    -a : arch (linux: aarch64/armhf; android: arm64-v8a/armeabi-v7a)
    -n : sdk name(libsvepsr)
    -b : build_type(Debug/Release/RelWithDebInfo)
    -m : enable asan
```

编译之前请先配置 env_android.sh / env_linux.sh 文件，指定编译工具链地址。
- Android 请配置NDK目录地址
- Linux 请配置 gcc 目录地址

## Android 编译命令
### RK3588

```shell
./build.sh -t rk3588 -s Android -a arm64-v8a -b Release
./build.sh -t rk3588 -s Android -a armeabi-v7a -b Release
```

### RK356x

```shell
./build.sh -t rk356x -s Android -a arm64-v8a -b Release
./build.sh -t rk356x -s Android -a armeabi-v7a -b Release
```

## Linux 编译命令
### RK3588

```shell
./build.sh -t rk3588 -s Linux -a aarch64 -b Release
./build.sh -t rk3588 -s Linux -a armhf -b Release
```

### RK356x

```shell
./build.sh -t rk356x -s Linux -a aarch64 -b Release
./build.sh -t rk356x -s Linux -a armhf -b Release
```


## 目录文件说明
```shell
├── build-android_rk356x.sh   # rk356x Android 编译脚本
├── build-android_rk3588.sh   # rk3588 Android 编译脚本
├── build-linux_rk356x.sh     # rk356x Linux 编译脚本
├── build-linux_rk3588.sh     # rk3588 Linux 编译脚本
├── build.sh                  # 通用编译脚本
├── CMakeLists.txt            
├── env_android.sh            # NDK 编译工具链地址配置
├── env_linux.sh              # gcc 编译工具链地址配置
├── include
│   ├── AsyncWorker.h         
│   └── worker.h              
├── README.md
└── src
    ├── AsyncWorker.cpp
    ├── main.cpp
    └── worker.cpp
```