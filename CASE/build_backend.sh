#!/bin/bash

# --- build_backend.sh (新版) ---
# 这个脚本用于一键编译集成了IMM算法的C++ Python模块。
# 请在 backend/ 目录下运行此脚本: ./build_backend.sh

# 设置颜色变量
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# 脚本出错时立即退出
set -e

echo -e "${YELLOW}步骤 1: 编译 C++ IMM 核心与 Python 包装器...${NC}"
# 进入Python包装器目录，这里是唯一的编译点
cd py_api
# 清理旧的编译文件
rm -rf build
# 创建新的编译目录并进入
mkdir build && cd build
# 运行CMake进行配置
# CMakeLists.txt 会负责找到所有相关的 .cpp 和 .c 文件
cmake ..
# 运行make进行编译 (这将构建出 imm_calculator.so)
make
# 返回到backend根目录
cd ../..
echo -e "${GREEN}步骤 1: Python 模块编译成功。${NC}\n"


echo -e "${YELLOW}步骤 2: 复制 Python 模块到正确位置...${NC}"
# 查找编译生成的模块文件 (兼容macOS和Linux)
MODULE_FILE=$(find py_api/build -name 'imm_calculator*.so' | head -n 1)

if [ -f "$MODULE_FILE" ]; then
    # 将模块文件复制到 py_api/ 目录下，以便app.py可以导入它
    cp "$MODULE_FILE" py_api/
    echo -e "${GREEN}步骤 2: 模块已成功复制到 py_api/ 目录。${NC}\n"
else
    echo -e "${RED}错误: 未找到编译后的模块文件 'imm_calculator.so'！构建可能已失败。${NC}"
    exit 1
fi

echo -e "${GREEN}✅ 后端构建流程成功完成！${NC}"
echo -e "\n--- ${YELLOW}下一步操作${NC} ---"
echo ""
echo "2. ${YELLOW}启动Python服务:${NC} 您现在可以从 'backend' 目录启动服务:"
echo "   ${GREEN}python py_api/app.py${NC}"
