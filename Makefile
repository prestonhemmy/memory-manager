all: demo/demo

demo/demo: src/MemoryManager.o demo/demo.cpp
	g++ -std=c++17 -g -o demo/demo demo/demo.cpp src/MemoryManager.o

src/MemoryManager.o: src/MemoryManager.cpp src/MemoryManager.h
	g++ -std=c++17 -g -c src/MemoryManager.cpp -o src/MemoryManager.o

run: demo/demo
	./demo/demo

clean:
	rm -f src/MemoryManager.o demo/demo memory_map.txt