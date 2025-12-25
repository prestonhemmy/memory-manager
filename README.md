# MemoryManager

A custom heap memory allocator implemented in C++ using direct memory mapping (`mmap`/`munmap`) with configurable allocation strategies.

## Features

- **Direct Memory Mapping**: Uses `mmap`/`munmap` system calls instead of `new`/`delete` for explicit control over virtual memory
- **Configurable Allocation Strategies**: 
  - **Best-Fit**: Minimizes wasted space by selecting the smallest sufficient hole
  - **Worst-Fit**: Selects the largest hole to reduce fragmentation from small remnants
- **Automatic Hole Coalescing**: Adjacent free blocks are merged to combat fragmentation
- **Memory State Inspection**:
  - Hole list retrieval for debugging allocation state
  - Bitmap representation for O(1) word-level allocation queries
  - Memory map dump to file for analysis

## Architecture

```
┌───────────────────────────────────────────────────────────────┐
│                       Memory Pool (mmap)                      │
├───────────┬──────────┬───────────┬──────────┬─────────────────┤
│  Block A  │  Hole 1  │  Block B  │  Hole 2  │     Hole 3      │
│(allocated)│  (free)  │(allocated)│  (free)  │     (free)      │
└───────────┴──────────┴───────────┴──────────┴─────────────────┘
      │                                               │
      ▼                                               ▼
┌───────────┐                                   ┌───────────┐
│ allocList │                                   │ holeList  │
│  [A, B]   │                                   │[H1,H2,H3] │
└───────────┘                                   └───────────┘
                                                      │
                                       ┌──────────────┴──────────────┐
                                       ▼                             ▼
                                  mergeHoles()                   allocator()
                              (coalesce adjacent)           (best-fit/worst-fit)
```

## API Reference

### Core Methods

| Method | Description |
|--------|-------------|
| `initialize(size_t sizeInWords)` | Allocates memory pool via `mmap` |
| `shutdown()` | Releases memory pool via `munmap` |
| `allocate(size_t sizeInBytes)` | Returns pointer to allocated block |
| `free(void* address)` | Frees block and coalesces adjacent holes |
| `setAllocator(function)` | Switches allocation strategy at runtime |

### Inspection Methods

| Method | Description |
|--------|-------------|
| `getList()` | Returns hole list as `[count, offset₁, len₁, ...]` |
| `getBitmap()` | Returns bitmap where `1` = allocated word |
| `dumpMemoryMap(char* filename)` | Writes hole list to file |

## Building

```bash
make          # Build library and demo
make clean    # Remove build artifacts
```

## Usage

```cpp
#include "MemoryManager.h"

int main() {
    // Initialize with 8-byte words and best-fit strategy
    MemoryManager mm(8, bestFit);
    mm.initialize(1024);  // 1024 words = 8KB
    
    // Allocate memory
    void* ptr1 = mm.allocate(64);   // 64 bytes
    void* ptr2 = mm.allocate(128);  // 128 bytes
    
    // Free memory (holes auto-coalesce)
    mm.free(ptr1);
    
    // Switch strategy at runtime
    mm.setAllocator(worstFit);
    
    // Allocate with new strategy
    void* ptr3 = mm.allocate(32);
    
    // Debug: dump memory map
    mm.dumpMemoryMap("memory_state.txt");
    
    mm.shutdown();
    return 0;
}
```

## Allocation Strategies Explained

### Best-Fit
Searches for the smallest hole that satisfies the request. Minimizes immediate waste but can create many small unusable fragments over time.

```
Request: 3 words
                        ↓
Holes:    [5 words] [3 words] [10 words]

Selected: [3 words]     exact fit, no waste
```

### Worst-Fit
Searches for the largest hole. Leaves larger remaining fragments that are more likely to satisfy future requests.

```
Request: 3 words  
                                  ↓
Holes:    [5 words] [3 words] [10 words]

Selected: [10 words]    leaves 7-word hole
```

## Technical Details

- **Word Size**: Configurable (typically 4 or 8 bytes)
- **Maximum Pool**: 65,535 words (16-bit offset addressing)
- **Memory Mapping**: `MAP_PRIVATE | MAP_ANONYMOUS` for process-private allocation
- **Thread Safety**: Not thread-safe (external synchronization required)

## File Structure

```
MemoryManager/
├── src/
│   ├── MemoryManager.cpp    # Implementation
│   └── MemoryManager.h      # Header with class definition
├── demo/
│   └── demo.cpp             # Usage demonstration
├── Makefile
└── README.md
```
