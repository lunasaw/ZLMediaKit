#!/bin/bash

# 默认为Release版本
build_type="Release"

# 解析参数
while getopts ":t:" opt; do
  case $opt in
    t)
      build_type=$OPTARG
      ;;
    \?)
      echo "无效的选项: -$OPTARG" >&2
      exit 1
      ;;
  esac
done

# 判断MediaServer进程是否正在运行，如果在运行则关闭
if pgrep MediaServer >/dev/null; then
  echo "关闭正在运行的MediaServer进程..."
  pkill MediaServer
fi

# 删除cmake-build-${build_type}目录
echo "删除cmake-build-${build_type}目录..."
rm -rf cmake-build-${build_type}

# 执行cmake命令
echo "执行cmake命令..."
cmake -DCMAKE_BUILD_TYPE=${build_type} -DCMAKE_MAKE_PROGRAM=ninja -G Ninja -S ./ -B ./cmake-build-${build_type}

# 执行编译命令
echo "执行编译命令..."
cmake --build ./cmake-build-${build_type} --target all -j 6

# 进入编译后的产物目录
cd ./release/darwin/${build_type}

# 启动MediaServer服务
echo "启动MediaServer服务..."
./MediaServer -d -l 0

