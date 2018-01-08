XX = g++
AR = ar
ARFLAG = -rcs
# ��̬�����ӳ�.so��̬�⣬���뾲̬���ʱ����Ҫ�� -fPIC���������
CFLAGS = -g -fPIC
CLIBS = -lpthread -lrt
SUBDIRS = ./memoryPool ./autoLock 

#ָ��ͷ�ļ�Ŀ¼�������оͲ���Ҫ��ͷ�ļ�������·��д������
INCLUDE_DIRS = -I./include -I./memoryPool -I./autoLock
OBJECTS = memPoolApi.o

SUB_SOURCES = $(wildcard ./memoryPool/*.cpp ./autoLock/*.cpp)
OBJECTS += $(patsubst %.cpp, %.o, $(SUB_SOURCES))

export XX CFLAGS AR ARFLAG

TARGET = libMmPool.so          

$(TARGET) : $(OBJECTS) $(SUBDIRS)
	$(XX) -shared -fPIC $(OBJECTS) -o $@ $(CLIBS)     # $< ��ʾ�����б�ĵ�һ�� Ҳ���� $(OBJECTS)
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
