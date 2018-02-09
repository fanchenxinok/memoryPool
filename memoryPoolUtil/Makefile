SOURCES = $(wildcard ./*.cpp)
INCLUDE_DIRS = -I./ -I../include -I../autoLock#指定头文件目录，代码中就不需要把头文件的完整路径写出来了
 
OBJECTS = $(patsubst %.cpp,%.o,$(SOURCES)) 
	
$(OBJECTS) : %.o : %.cpp 
	$(XX) -c $(CFLAGS) $< -o $@ $(INCLUDE_DIRS)

.PHONY : clean
clean:
	rm -rf $(OBJECTS) 