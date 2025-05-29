#!/bin/bash

set -e

echo "$0 $@"
while getopts "t:s:a:b:mn" opt; do
  case $opt in
    n)
      TARGET_SDK=$OPTARG
      ;;
    s)
      TARGET_SYSTEM=$OPTARG
      ;;
    a)
      TARGET_ARCH=$OPTARG
      ;;
    b)
      BUILD_TYPE=$OPTARG
      ;;
    :)
      echo "Option -$OPTARG requires an argument."
      exit 1
      ;;
    ?)
      echo "Invalid option: -$OPTARG index:$OPTIND"
      ;;
  esac
done

if [ x"${TARGET_SYSTEM}" != x"Linux" ]  &&  [ x"${TARGET_SYSTEM}" != x"Android" ]; then
  echo "$0 -t <target> -a <arch> -s <sdk> -b <build_type>"
  echo "Please select target sys: Linux or Android"
  echo ""
  echo "    -s : system (Linux/Android)"
  echo "    -a : arch (linux: aarch64/armhf; android: arm64-v8a/armeabi-v7a)"
  echo "    -n : sdk name(vendor-storage-test)"
  echo "    -b : build_type(Debug/Release/RelWithDebInfo)"
  echo ""
  exit -1
fi

# Debug / Release / RelWithDebInfo
if [[ -z ${BUILD_TYPE} ]];then
    BUILD_TYPE=Release
fi

if [[ -z ${ENABLE_ASAN} ]];then
    ENABLE_ASAN=OFF
fi

if [[ -z ${TARGET_SDK} ]];then
    TARGET_SDK=libsvepsr
fi

if [ "${TARGET_SYSTEM}" == "Android" ]; then
source env_android.sh
fi

if [ "${TARGET_SYSTEM}" == "Linux" ]; then
source env_linux.sh
fi

TARGET_PLATFORM=${TARGET_SOC}_${TARGET_SYSTEM}
if [[ -n ${TARGET_ARCH} ]];then
  TARGET_PLATFORM=${TARGET_PLATFORM}_${TARGET_ARCH}
fi
ROOT_PWD=$( cd "$( dirname $0 )" && cd -P "$( dirname "$SOURCE" )" && pwd )
INSTALL_DIR=${ROOT_PWD}/build/install/${TARGET_SYSTEM}/${TARGET_ARCH}
BUILD_DIR=${ROOT_PWD}/build/build_${TARGET_SDK}_${TARGET_PLATFORM}_${BUILD_TYPE}

echo "==================================="
echo "TARGET_ARCH=${TARGET_ARCH}"
echo "TARGET_SYSTEM=${TARGET_SYSTEM}"
echo "TARGET_SDK=${TARGET_SDK}"
echo "BUILD_TYPE=${BUILD_TYPE}"
echo "C_COMPILER=${C_COMPILER}"
echo "CXX_COMPILER=${CXX_COMPILER}"
echo "INSTALL_DIR=${INSTALL_DIR}"
echo "BUILD_DIR=${BUILD_DIR}"
echo "==================================="

if [[ ! -d "${BUILD_DIR}" ]]; then
  mkdir -p ${BUILD_DIR}
fi

cd ${BUILD_DIR}
if [ "${TARGET_SYSTEM}" == "Linux" ]; then
  cmake ../.. \
      -DCMAKE_SYSTEM_NAME=Linux \
      -DCMAKE_C_COMPILER=${C_COMPILER} \
      -DCMAKE_CXX_COMPILER=${CXX_COMPILER} \
      -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
      -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}
elif [ "${TARGET_SYSTEM}" == "Android" ]; then
  cmake ../.. \
      -DCMAKE_SYSTEM_NAME=Android \
      -DANDROID_PLATFORM=android-26 \
      -DANDROID_ABI=${TARGET_ARCH} \
      -DANDROID_STL=c++_static \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      -DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK_PATH}/build/cmake/android.toolchain.cmake \
      -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
      -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}
fi
make -j16
make install
cd -
