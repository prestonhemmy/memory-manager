#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <cstddef>
#include <functional>
#include <list>

class MemoryManager {
public:
    static const unsigned int MAX_NUM_WORDS = 65535;

    MemoryManager(unsigned int wordSize, std::function<int(int, void*)> allocator);
    ~MemoryManager();

    // Core functionality
    void initialize(size_t sizeInWords);
    void shutdown();
    void* allocate(size_t sizeInBytes);
    void free(void* address);
    void setAllocator(std::function<int(int, void*)> allocator);

    // Getters
    void* getList();
    void* getBitmap();
    unsigned int getWordSize();
    void* getMemoryStart();
    unsigned int getMemoryLimit();

    // Debugging
    int dumpMemoryMap(char* filename);

private:
    struct Hole {
        unsigned int offset;
        unsigned int length;
    };

    struct Block {
        unsigned int offset;
        unsigned int length;
    };

    unsigned int wordSize;
    void* memoryStart;
    size_t memoryLimit;
    std::function<int(int, void*)> allocator;
    std::list<Hole> holeList;
    std::list<Block> allocatedList;

    void mergeHoles();
};

// Allocation strategies
int bestFit(int sizeInWords, void* list);
int worstFit(int sizeInWords, void* list);

#endif // MEMORY_MANAGER_H
