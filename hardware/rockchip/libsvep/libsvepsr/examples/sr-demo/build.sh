#!/bin/bash

set -e

echo "$0 $@"
while getopts "t:s:a:b:mn:g" opt; do
  case $opt in
    t)
      TARGET_SOC=$OPTARG
      ;;
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
    m)
      ENABLE_ASAN=ON
      echo "ENABLE_ASAN"
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

if [ x"${TARGET_SOC}" != x"rk356x" ]  &&  [ x"${TARGET_SOC}" != x"rk3588" ]; then
  echo "$0 -t <target> -a <arch> -s <sdk> -b <build_type>"
  echo "Please select target platform: rk356x or rk3588"
  echo ""
  echo "    -t : target (rk356x/rk3588)"
  echo "    -s : system (Linux/Android)"
  echo "    -a : arch (linux: aarch64/armhf; android: arm64-v8a/armeabi-v7a)"
  echo "    -n : sdk name(libsvepsr)"
  echo "    -b : build_type(Debug/Release/RelWithDebInfo)"
  echo "    -m : enable asan"
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
source env_linux.sh ${TARGET_ARCH}
fi

TARGET_PLATFORM=${TARGET_SOC}_${TARGET_SYSTEM}
if [[ -n ${TARGET_ARCH} ]];then
  TARGET_PLATFORM=${TARGET_PLATFORM}_${TARGET_ARCH}
fi
ROOT_PWD=$( cd "$( dirname $0 )" && cd -P "$( dirname "$SOURCE" )" && pwd )
INSTALL_DIR=${ROOT_PWD}/build/install/
BUILD_DIR=${ROOT_PWD}/build/build_${TARGET_SDK}_${TARGET_PLATFORM}_${BUILD_TYPE}

echo "==================================="
echo "TARGET_SOC=${TARGET_SOC}"
echo "TARGET_ARCH=${TARGET_ARCH}"
echo "TARGET_SYSTEM=${TARGET_SYSTEM}"
echo "TARGET_SDK=${TARGET_SDK}"
echo "BUILD_TYPE=${BUILD_TYPE}"
echo "ENABLE_ASAN=${ENABLE_ASAN}"
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
      -DTARGET_SOC=${TARGET_SOC} \
      -DCMAKE_SYSTEM_NAME=Linux \
      -DCMAKE_C_COMPILER=${C_COMPILER} \
      -DCMAKE_CXX_COMPILER=${CXX_COMPILER} \
      -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
      -DENABLE_ASAN=${ENABLE_ASAN} \
      -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}
elif [ "${TARGET_SYSTEM}" == "Android" ]; then
  cmake ../.. \
      -DTARGET_SOC=${TARGET_SOC} \
      -DCMAKE_SYSTEM_NAME=Android \
      -DANDROID_PLATFORM=android-26 \
      -DANDROID_ABI=${TARGET_ARCH} \
      -DANDROID_STL=c++_static \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      -DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK_PATH}/build/cmake/android.toolchain.cmake \
      -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
      -DENABLE_ASAN=${ENABLE_ASAN} \
      -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}
fi
make -j16
make install
cd -
