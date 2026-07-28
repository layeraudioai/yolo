CC=c++
CFLAGS=-Wall -Wextra -Werror
TARGET=yolo

all: $(TARGET)

$(TARGET): yolo_app.o
	$(CC) $(CFLAGS) -o $(TARGET) yolo_app.o

yolo_app.o: yolo_app.cpp yolo_core.hpp
	$(CC) $(CFLAGS) -c yolo_app.cpp

clean:
	rm *.o 
	rm *.exe 

install:
	cp yolo.exe /bin 
	cp yolo.exe /usr/bin 
	cp yolo.exe c:\yolo 
	
