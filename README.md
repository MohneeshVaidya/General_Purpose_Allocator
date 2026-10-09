# General-Purpose Memory Allocator in C

## Overview

A custom memory allocator implemented in C using a fixed-size heap, block headers and footers, a doubly linked free list, and a first-fit allocation strategy.

The allocator provides `malloc`-like, `calloc`-like, `realloc`-like, and `free`-like operations through its own public API.

The purpose of this project is to understand memory management, heap organization, free-list management, block splitting, coalescing, and allocator interface design by implementing them from scratch.

## Features

- Fixed-size 1 MiB heap.
- First-fit allocation strategy.
- Block splitting to reduce wasted space.
- Block headers and footers for tracking block sizes.
- Doubly linked list of free blocks.
- Coalescing of adjacent free blocks.
- Zero-initialized allocation through `allocator_calloc()`.
- Resizing allocations through `allocator_realloc()`.
- In-place allocation growth when the following block is free.
- Relocation when in-place growth is not possible.
- Heap reset functionality.
- Opaque public `Allocator` type.

## Project Structure

```text
general_purpose_allocator/
├── allocator.h
├── allocator.c
├── main.c
└── README.md
```

## Heap Organization

The allocator manages a fixed-size heap of 1 MiB.

Each block contains:

```text
+------------------+
| Header           |
+------------------+
| Payload          |
|                  |
+------------------+
| Footer           |
+------------------+
```

The header stores:

- `size`: total block size, including the header and footer.
- `is_free`: whether the block is available for allocation.

The footer stores the total block size. It allows the allocator to locate the preceding physical block when freeing or coalescing memory.

Free blocks additionally store `next` and `prev` pointers in their header area to maintain the doubly linked free list.

## Allocation Strategy

### First-Fit Allocation

The allocator traverses the free list and selects the first block capable of satisfying the requested payload size.

### Block Splitting

When a free block is sufficiently large, the allocator divides it into:

1. An allocated block containing the requested payload.
2. A remaining free block containing the unused space.

Splitting avoids consuming an unnecessarily large free block for a small request.

### Block Coalescing

When a block is freed, the allocator searches for neighboring free blocks and merges physically adjacent blocks.

Coalescing reduces external fragmentation and allows larger future allocations.

## Public API

The public interface is declared in `allocator.h`.

```c
#ifndef allocator_h
#define allocator_h

#include <stddef.h>

typedef struct Allocator Allocator;

Allocator* allocator_create(void);

void allocator_destroy(Allocator* allocator);

void allocator_reset(Allocator* allocator);

void* allocator_malloc(Allocator* allocator, size_t size);

void* allocator_calloc(
    Allocator* allocator,
    size_t count,
    size_t object_size
);

void* allocator_realloc(
    Allocator* allocator,
    void* pointer,
    size_t new_size
);

void allocator_free(Allocator* allocator, void* pointer);

#endif
```

### `allocator_create()`

Creates an allocator instance and initializes its heap.

Returns a pointer to the allocator, or `NULL` if the allocator object cannot be created.

### `allocator_destroy()`

Destroys the allocator instance and releases the memory used by the allocator object.

### `allocator_reset()`

Restores the heap to its initial state, making the entire heap available again.

Existing allocations become invalid after a reset. They must not subsequently be accessed or freed.

### `allocator_malloc()`

Allocates a block capable of holding the requested number of payload bytes.

Returns a pointer to the payload, or `NULL` if the request cannot be satisfied.

A zero-size request returns `NULL`.

### `allocator_calloc()`

Allocates space for `count` objects, each of `object_size` bytes, and initializes the requested payload bytes to zero.

Checks for multiplication overflow before calculating the total allocation size.

Returns `NULL` for zero arguments, multiplication overflow, or allocation failure.

### `allocator_realloc()`

Changes the size of an existing allocation.

The implementation supports:

- Shrinking an existing allocation.
- Growing into an adjacent free block.
- Relocating an allocation when in-place growth is unavailable.
- Preserving existing payload data.
- Returning `NULL` on allocation failure without freeing the original allocation.

Additional behavior:

- A `NULL` pointer is handled as a new allocation.
- A zero-size request frees the existing allocation and returns `NULL`.
- Oversized requests that would overflow the metadata-size calculation are rejected.

### `allocator_free()`

Releases a previously allocated block and makes its memory available for reuse.

The implementation attempts to merge the released block with physically adjacent free blocks.

## Reallocation Strategy

The `allocator_realloc()` implementation delegates resizing work to helper functions.

### Shrinking: `trim()`

Reduces the allocated block's size and creates a free block from the released space when enough space remains for valid block metadata.

The released space can then be coalesced with neighboring free blocks.

### Relocation: `relocate()`

When the allocation cannot grow in place:

1. Allocates a replacement block.
2. Copies the old payload into the replacement.
3. Frees the old allocation.
4. Returns the replacement pointer.

If the replacement allocation fails, the original allocation remains valid.

### In-Place Growth: `allocate_more()`

Attempts to expand the allocation into the physically adjacent free block.

If sufficient space is available, it consumes the required portion and reconstructs the remaining free block.

Otherwise, it falls back to relocation.

## Testing

The test suite covers 19 test scenarios.

| No. | Test | Expected behavior |
|---:|---|---|
| 1 | Basic `malloc` | Allocation and payload access succeed |
| 2 | Zero-size `malloc` | Returns `NULL` |
| 3 | Multiple allocations | Allocations are distinct and usable |
| 4 | Oversized `malloc` | Returns `NULL` |
| 5 | `calloc` initialization | Allocated bytes are zero |
| 6 | Zero-argument `calloc` | Returns `NULL` |
| 7 | `calloc` overflow | Returns `NULL` |
| 8 | Oversized `calloc` | Returns `NULL` |
| 9 | Free and reuse | Released memory can be reused |
| 10 | Forward-order coalescing | Adjacent blocks merge |
| 11 | Reverse-order coalescing | Adjacent blocks merge |
| 12 | `realloc(NULL, size)` | Behaves like allocation |
| 13 | `realloc(ptr, 0)` | Frees the allocation and returns `NULL` |
| 14 | Shrinking | Existing payload data is preserved |
| 15 | In-place growth | Allocation grows into adjacent free space |
| 16 | Relocation | Data is preserved after moving |
| 17 | Failed growth | Original allocation remains valid |
| 18 | Size overflow | Request is rejected |
| 19 | Reset | Heap returns to its initial state |

### Compile and Run

Compile the allocator and test suite:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -g \
    main.c allocator.c -o main
```

Run the tests:

```bash
./main
```

### Sanitizer Testing

To detect certain memory errors and undefined behavior, compile with sanitizers:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -g \
    -fsanitize=address,undefined \
    -fno-omit-frame-pointer \
    main.c allocator.c -o main
```

Run:

```bash
./main
```

## Current Test Status

All 19 test scenarios passed in the recorded test run, including the allocation, freeing, coalescing, and reallocation scenarios.

Passing these tests confirms the tested behaviors, but does not establish that every allocator invariant or edge case is correct.

## Current Scope and Future Improvements

Potential future improvements include:

- Memory alignment guarantees.
- Stronger internal heap and free-list invariant checks.
- Randomized allocation and deallocation tests.
- More extensive fragmentation testing.
- Additional `realloc()` edge cases.
- Improved diagnostics and debugging support.
- Performance measurement and alternative allocation strategies.

## Learning Objectives

This project provides practical experience with:

- Manual memory management.
- Pointer arithmetic and memory layout.
- Data structure design and implementation in C.
- Doubly linked lists.
- Block splitting and coalescing.
- Integer overflow prevention.
- API and interface design.
- Allocation failure handling.
- Testing low-level systems components.
