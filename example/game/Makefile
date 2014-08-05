#交叉编译器路径
CROSS=
CP=/bin/cp
RM=-/bin/rm -rf
LN=/bin/ln -s 
#是否开启sqlite的支持，如需要关闭，设置为空字符串
FF_ENABLE_SQLITE_FLAG =-DFF_ENABLE_SQLITE
FF_ENABLE_CURL_FLAG   =
#-DFF_ENABLE_CURL
CFLAGS=-g -Wall $(FF_ENABLE_CURL_FLAG)  $(FF_ENABLE_SQLITE_FLAG)
LDFLAGS= -O2 -lpthread  -ldl -lpython2.6 -lmysqlclient 
#-lcurl
#-llua 
#链接库名
LIB_NAME=
#链接库版本
LIB_VER=1.0.0
#平台
ARCH=
# 二进制目标
BIN=app_game

#源文件目录
SrcDir= . ../../fflib/base ../../fflib/net ../../fflib/rpc  ../../fflib/rpc/msg_def  ../../fflib/db ../../fflib/xml ../../fflib/server
#头文件目录
IncDir=  ../../fflib/  ../../lib3party /usr/include/python2.7/ /usr/include/python2.6/
#连接库目录
LibDir= /usr/local/lib /usr/lib/mysql /usr/lib64/mysql
SRCS=$(foreach dir,$(SrcDir),$(wildcard $(dir)/*.cpp))
#INCS=$(foreach dir,$(IncDir),$(wildcard $(dir)/*.h))
INCS=$(foreach dir,$(IncDir),$(addprefix -I,$(dir)))
LINKS=$(foreach dir,$(LibDir),$(addprefix -L,$(dir)))
CFLAGS := $(CFLAGS) $(INCS)
LDFLAGS:= $(LINKS) $(LDFLAGS) 
CC=gcc
ARCH=PC
OBJS = $(SRCS:%.cpp=%.o)
.PHONY:all clean

all:$(BIN)
$(BIN):$(OBJS)
ifeq ($(FF_ENABLE_SQLITE_FLAG),-DFF_ENABLE_SQLITE)
	gcc -c ../../fflib/db/sqlite3.c -o sqlite3.o
	g++ -o $(BIN) $(OBJS) sqlite3.o $(LDFLAGS)  
else
	g++ -o $(BIN) $(OBJS) $(LDFLAGS)  
endif
	@echo  " OK! Compile $@ "
# @$(LN) $(shell pwd)/$(LIB_NAME).$(LIB_VER) /lib/$(LIB_NAME)

%.o:%.cpp
	@echo  "[$(ARCH)] Compile $@..."
	@$(CC) $(CFLAGS)  -c $< -o $@

.PHONY: clean
clean:
	@echo  "[$(ARCH)] Cleaning files..."
	@$(RM) $(OBJS) $(BIN) 
	@$(RM) sqlite3.o
