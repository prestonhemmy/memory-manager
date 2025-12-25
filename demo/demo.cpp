#include <iostream>
#include <iomanip>
#include <cstdint>
#include "../src/MemoryManager.h"

void printHoleList(MemoryManager& mm) {
    void* list = mm.getList();
    if (list == nullptr) {
        std::cout << "  Hole list: (empty)\n";
        return;
    }
    
    uint16_t* holeList = static_cast<uint16_t*>(list);
    uint16_t count = holeList[0];
    
    std::cout << "  Hole list: ";
    for (uint16_t i = 0; i < count; i++) {
        uint16_t offset = holeList[1 + i * 2];
        uint16_t length = holeList[2 + i * 2];
        std::cout << "[" << offset << ", " << length << "]";
        if (i < count - 1) std::cout << " - ";
    }
    std::cout << "\n";
    
    delete[] holeList;
}

void printSeparator(const char* title) {
    std::cout << "\n" << std::string(50, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(50, '=') << "\n";
}

int main() {
    const unsigned int WORD_SIZE = 8;  // 8 bytes per word
    const size_t POOL_SIZE = 32;       // 32 words = 256 bytes
    
    std::cout << "Memory Manager Demo\n";
    std::cout << "Word size: " << WORD_SIZE << " bytes\n";
    std::cout << "Pool size: " << POOL_SIZE << " words (" << POOL_SIZE * WORD_SIZE << " bytes)\n";
    
    // Demo 1: Basic allocation with Best-Fit
    printSeparator("Demo 1: Best-Fit Allocation");
    
    MemoryManager mm(WORD_SIZE, bestFit);
    mm.initialize(POOL_SIZE);
    
    std::cout << "\nInitial state:\n";
    printHoleList(mm);
    
    // allocate three blocks
    std::cout << "\nAllocating 32 bytes (4 words)...\n";
    void* block1 = mm.allocate(32);
    std::cout << "  Block 1 address: " << block1 << "\n";
    printHoleList(mm);
    
    std::cout << "\nAllocating 64 bytes (8 words)...\n";
    void* block2 = mm.allocate(64);
    std::cout << "  Block 2 address: " << block2 << "\n";
    printHoleList(mm);
    
    std::cout << "\nAllocating 16 bytes (2 words)...\n";
    void* block3 = mm.allocate(16);
    std::cout << "  Block 3 address: " << block3 << "\n";
    printHoleList(mm);
    
    // free middle block to create fragmentation
    std::cout << "\nFreeing Block 2 (creates hole in middle)...\n";
    mm.free(block2);
    printHoleList(mm);
    
    // allocate smaller block - best-fit should find exact or smallest fit
    std::cout << "\nAllocating 24 bytes (3 words) - best-fit selects smallest sufficient hole...\n";
    void* block4 = mm.allocate(24);
    std::cout << "  Block 4 address: " << block4 << "\n";
    printHoleList(mm);
    
    mm.shutdown();
    
    // Demo 2: Worst-Fit comparison
    printSeparator("Demo 2: Worst-Fit Allocation");
    
    MemoryManager mm2(WORD_SIZE, worstFit);
    mm2.initialize(POOL_SIZE);
    
    // create same fragmentation pattern
    void* w1 = mm2.allocate(32);
    void* w2 = mm2.allocate(64);
    void* w3 = mm2.allocate(16);
    mm2.free(w2);  // free middle block
    
    std::cout << "\nSame setup: freed middle 8-word block\n";
    printHoleList(mm2);
    
    std::cout << "\nAllocating 24 bytes (3 words) - worst-fit selects largest hole...\n";
    void* w4 = mm2.allocate(24);
    std::cout << "  Block address: " << w4 << "\n";
    printHoleList(mm2);
    
    mm2.shutdown();
    
    // Demo 3: Hole coalescing
    printSeparator("Demo 3: Hole Coalescing");
    
    MemoryManager mm3(WORD_SIZE, bestFit);
    mm3.initialize(POOL_SIZE);
    
    void* c1 = mm3.allocate(64);  // 8 words
    void* c2 = mm3.allocate(64);  // 8 words  
    void* c3 = mm3.allocate(64);  // 8 words
    
    std::cout << "\nAllocated three 8-word blocks:\n";
    printHoleList(mm3);
    
    std::cout << "\nFreeing first block (non-adjacent, no coalesce)...\n";
    mm3.free(c1);
    printHoleList(mm3);
    
    std::cout << "\nFreeing third block (adjacent to Hole 2 - coalesces)...\n";
    mm3.free(c3);
    printHoleList(mm3);
    
    std::cout << "\nFreeing second block (adjacent to both holes - coalesces)...\n";
    mm3.free(c2);
    printHoleList(mm3);
    
    mm3.shutdown();
    
    // Demo 4: Memory map dump
    printSeparator("Demo 4: Memory Map Dump");
    
    MemoryManager mm4(WORD_SIZE, bestFit);
    mm4.initialize(POOL_SIZE);
    
    mm4.allocate(32);
    mm4.allocate(48);
    
    char filename[] = "memory_map.txt";
    int result = mm4.dumpMemoryMap(filename);
    
    if (result == 0) {
        std::cout << "\nMemory map written to: " << filename << "\n";
        std::cout << "Contents show hole list in format: [offset, length]\n";
    }
    
    mm4.shutdown();
    
    printSeparator("Demo Complete");
    
    return 0;
}
