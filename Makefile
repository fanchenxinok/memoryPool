XX = g++
AR = ar
ARFLAG = -rcs
# 静态库链接成.so动态库，编译静态库的时候需要加 -fPIC这个参数。
CFLAGS = -g -fPIC
CLIBS = -lpthread -lrt
SUBDIRS = ./memoryPool ./autoLock 

#指定头文件目录，代码中就不需要把头文件的完整路径写出来了
INCLUDE_DIRS = -I./include -I./memoryPool -I./autoLock
OBJECTS = memPoolApi.o

SUB_SOURCES = $(wildcard ./memoryPool/*.cpp ./autoLock/*.cpp)
OBJECTS += $(patsubst %.cpp, %.o, $(SUB_SOURCES))

export XX CFLAGS AR ARFLAG

TARGET = libMmPool.so          

$(TARGET) : $(OBJECTS) $(SUBDIRS)
	$(XX) -shared -fPIC $(OBJECTS) -o $@ $(CLIBS)     # $< 表示依赖列表的第一个 也就是 $(OBJECTS)
	cp $@ ./lib
	
$(OBJECTS) : %.o : %.cpp 
	$(XX) -c $(CFLAGS) $< -o $@ $(INCLUDE_DIRS)
	
$(SUBDIRS):ECHO
	+$(MAKE) -C $@

ECHO:
	@echo $(SUBDIRS)
	@echo begin compile

.PHONY : clean
clean:
	for dir in $(SUBDIRS);\
	do $(MAKE) -C $$dir clean||exit 1;\
	done
	rm -rf $(OBJECTS)  ./lib/*.so ./*.so
