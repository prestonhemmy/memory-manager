#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include "MemoryManager.h"

MemoryManager::MemoryManager(unsigned int wordSize, std::function<int(int, void*)> allocator)
    : wordSize(wordSize), memoryStart(nullptr), memoryLimit(0), allocator(allocator) {}

MemoryManager::~MemoryManager() {
    shutdown();
}

// Core functionality

void MemoryManager::initialize(size_t sizeInWords) {
    // Clean up existing memory
    if (memoryStart != nullptr) {
        shutdown();
    }

    // Validate 'sizeInWords'
    if (sizeInWords > MAX_NUM_WORDS) {
        throw std::invalid_argument(
            "Expected sizeInWords to be in range 1 to 65535, but got " + std::to_string(sizeInWords)
        );
    }

    memoryLimit = sizeInWords * wordSize;

    // Allocate memory using mmap
    memoryStart = mmap(nullptr, memoryLimit, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memoryStart == MAP_FAILED) {
        memoryLimit = 0;
        memoryStart = nullptr;
        throw std::runtime_error("Memory allocation failed during initialization.");
    }

    holeList.clear();
    holeList.push_back(Hole{ 0, static_cast<unsigned int>(sizeInWords) });

    allocatedList.clear();
}

void MemoryManager::shutdown() {
    // Check if memory allocated
    if (memoryStart != nullptr) {
        munmap(memoryStart, memoryLimit);

        memoryLimit = 0;
        memoryStart = nullptr;
    }

    holeList.clear();
    allocatedList.clear();
}

void* MemoryManager::allocate(size_t sizeInBytes) {
    if (memoryStart == nullptr || sizeInBytes == 0) {
        return nullptr;
    }

    unsigned int sizeInWords = (sizeInBytes + wordSize - 1) / wordSize; // Round up to nearest word

    // Check if enough memory available
    void* list = getList();
    if (list == nullptr) {
        return nullptr;
    }

    // Find hole according to allocation strategy
    int wordOffset = allocator(sizeInWords, list);

    delete[] static_cast<uint16_t*>(list);

    // Check if suitable hole found
    if (wordOffset == -1) {
        return nullptr;
    }

    // Find and update hole in hole list
    for (auto it = holeList.begin(); it != holeList.end(); ++it) {
        if (it->offset == wordOffset) {

            // Allocate memory from hole
            void* allocatedMemory = static_cast<uint8_t*>(memoryStart) + (wordOffset * wordSize);

            // Add to 'allocatedList'
            Block block = { static_cast<unsigned int>(wordOffset), sizeInWords };

            auto itr = allocatedList.begin();
            while (itr != allocatedList.end() && itr->offset < block.offset) {
                ++itr;
            }

            allocatedList.insert(itr, block);

            // Update 'holeList'

            // If "exact fit"
            if (it->length == sizeInWords) {
                holeList.erase(it);
            
            // Otherwise "partial fit"
            } else {
                it->offset += sizeInWords;
                it->length -= sizeInWords;
            }

            return allocatedMemory;
        }
    }

    return nullptr;
}

void MemoryManager::free(void* address) {
    if (memoryStart == nullptr || address == nullptr) {
        return;
    }

    size_t offsetInBytes = static_cast<uint8_t*>(address) - static_cast<uint8_t*>(memoryStart);
    
    unsigned int wordOffset = offsetInBytes / wordSize;

    // Find length of allocated block
    unsigned int allocatedLength = 0;
    for (auto it = allocatedList.begin(); it != allocatedList.end(); ++it) {
        if (it->offset == wordOffset) {
            allocatedLength = it->length;
            allocatedList.erase(it);
            
            // Add to 'holeList'
            Hole hole = { static_cast<unsigned int>(wordOffset), allocatedLength };

            auto itr = holeList.begin();
            while (itr != holeList.end() && itr->offset < hole.offset) {
                ++itr;
            }

            holeList.insert(itr, hole);

            break;
        }
    }

    // Check if block found
    if (allocatedLength == 0) {
        return;
    }

    mergeHoles();
}

void MemoryManager::mergeHoles() {
    auto it = holeList.begin();
    while (it != holeList.end()) {
        auto next = std::next(it);
        if (next != holeList.end() && it->offset + it->length == next->offset) {
            it->length += next->length;
            holeList.erase(next);
        } else {
            ++it;
        }
    }
}

void MemoryManager::setAllocator(std::function<int(int, void*)> allocator) {
    this->allocator = allocator;
}

// Getters

void* MemoryManager::getList() {
    if (holeList.empty()) {
        return nullptr;
    }

    uint16_t* list = new uint16_t[1 + holeList.size() * 2]; // 1 for count, 2 per hole
    list[0] = static_cast<uint16_t>(holeList.size());       // Hole count

    size_t index = 1;
    for (const auto& hole : holeList) {
        list[index++] = static_cast<uint16_t>(hole.offset);
        list[index++] = static_cast<uint16_t>(hole.length);
    }

    return list;
}

void* MemoryManager::getBitmap() {
    if (memoryStart == nullptr) {
        return nullptr;
    }

    unsigned int numWords = memoryLimit / wordSize;

    unsigned int bitmapSizeInBytes = (numWords + 7) / 8;  // Round up to nearest byte

    uint8_t* bitmap = new uint8_t[2 + bitmapSizeInBytes];

    bitmap[0] = static_cast<uint8_t>(bitmapSizeInBytes & 0xFF);         // Lower byte
    bitmap[1] = static_cast<uint8_t>((bitmapSizeInBytes >> 8) & 0xFF);  // Higher byte
    
    std::memset(bitmap + 2, 0, bitmapSizeInBytes);  // Unset all bits
    
    // Set bits according to allocated blocks
    for (const auto& block : allocatedList) {
        for (unsigned int l = 0; l < block.length; ++l) {
            unsigned int wordIndex = block.offset + l;
            unsigned int byteIndex = wordIndex / 8;
            unsigned int bitIndex = wordIndex % 8;

            bitmap[2 + byteIndex] |= (1 << bitIndex);
        }
    }

    return bitmap;
}

unsigned int MemoryManager::getWordSize() {
    return wordSize;
}

void* MemoryManager::getMemoryStart() {
    return memoryStart;
}

unsigned int MemoryManager::getMemoryLimit() {
    return memoryLimit;
}

// Debugging

int MemoryManager::dumpMemoryMap(char* filename) {
    int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0777);

    if (fd == -1) {
        return -1;  // Error opening file
    }

    for (auto it = holeList.begin(); it != holeList.end(); ++it) {
        std::string msg = "[" + std::to_string(it->offset) + ", " + std::to_string(it->length) + "]";

        int nb = write(fd, msg.data(), msg.size());

        if (nb == -1) {
            if (close(fd) == -1) {
                return -1;  // Error closing file
            }

            return -1;  // Error writing to file
        }

        auto next = std::next(it);
        if (next != holeList.end()) {
            std::string delim = " - ";

            write(fd, delim.data(), delim.size());
        }
    }

    if (close(fd) == -1) {
        return -1;  // Error closing file
    }

    return 0;
}

// Allocators

int bestFit(int sizeInWords, void* list) {
    uint16_t* holeList = static_cast<uint16_t*>(list);
    uint16_t holeCount = holeList[0];

    int bestOffset = -1;
    int bestLength = MemoryManager::MAX_NUM_WORDS + 1;

    for (uint16_t i = 0; i < holeCount; i++) {
        uint16_t offset = holeList[1 + i * 2];
        uint16_t length = holeList[2 + i * 2];

        // Check if smaller suitable hole found
        if (length >= sizeInWords && length < bestLength) {
            bestOffset = offset;
            bestLength = length;
        }
    }

    return bestOffset;
}

int worstFit(int sizeInWords, void* list) {
    uint16_t* holeList = static_cast<uint16_t*>(list);
    uint16_t holeCount = holeList[0];

    int bestOffset = -1;
    int bestLength = -1;

    for (uint16_t i = 0; i < holeCount; i++) {
        uint16_t offset = holeList[1 + i * 2];
        uint16_t length = holeList[2 + i * 2];

        // Check if larger suitable hole found
        if (length >= sizeInWords && length > bestLength) {
            bestOffset = offset;
            bestLength = length;
        }
    }

    return bestOffset;
}
