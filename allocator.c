#include "allocator.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>


#define LOG_ERROR(message) \
    (fprintf(stderr, "[File: %s, Function: %s, Line: %d]: %s!\n", __FILE__, __func__, __LINE__, (message)))


#define HEAP_SIZE (1024u * 1024)


typedef uint8_t byte_t;


typedef struct Header {
    size_t size;
    bool is_free;
} Header;


typedef struct FreeHeader {
    Header header;
    struct FreeHeader* next;
    struct FreeHeader* prev;
} FreeHeader;


typedef size_t Footer;


struct Allocator {
    byte_t memory[HEAP_SIZE];
    FreeHeader* free_list_head;
};


Allocator* allocator_create() {
    Allocator* allocator = malloc(sizeof(*allocator));
    if (!allocator) {
        LOG_ERROR("Not able to allocate memory to allocator");
        return NULL;
    }
    allocator_reset(allocator);
    return allocator;
}


void allocator_destroy(Allocator* allocator) {
    free(allocator);
}


#define AS_PTR_TO(T, pointer) \
    ((T*)(pointer))


static Footer* get_footer_ptr(void* block) {
    if (!block) {
        return NULL;
    }
    Header* header = AS_PTR_TO(Header, block);
    return AS_PTR_TO(Footer, AS_PTR_TO(byte_t, header) + header->size - sizeof(Footer));
}


void allocator_reset(Allocator* allocator) {
    if (!allocator) {
        return;
    }
    FreeHeader* free_header = AS_PTR_TO(FreeHeader, allocator->memory);
    Header* header = AS_PTR_TO(Header, free_header);
    header->is_free = true;
    header->size = HEAP_SIZE;
    free_header->next = NULL;
    free_header->prev = NULL;
    *get_footer_ptr(allocator->memory) = HEAP_SIZE;
    allocator->free_list_head = free_header;
}


#define HEADER_FOOTER_SIZE (sizeof(Header) + sizeof(Footer))
#define MINIMUM_BLOCK_SIZE (sizeof(FreeHeader) + sizeof(Footer))


static bool is_fit(void* block, size_t size) {
    Header* header = block;
    if (header->size < HEADER_FOOTER_SIZE) {
        return false;
    }
    return size <= header->size - HEADER_FOOTER_SIZE;
}


static void* find_first_fit(FreeHeader* free_list_head, size_t size) {
    FreeHeader* current = free_list_head;
    while (current && !is_fit(current, size)) {
        current = current->next;
    }
    if (!current) {
        return NULL;
    }
    return current;
}


static bool should_split(void* block, size_t size) {
    Header* header = AS_PTR_TO(Header, block);
    if (header->size < HEADER_FOOTER_SIZE + MINIMUM_BLOCK_SIZE) {
        return false;
    }
    return size <= header->size - HEADER_FOOTER_SIZE - MINIMUM_BLOCK_SIZE;
}


static void* split_and_use(FreeHeader** free_list_head, void* free_block, size_t size) {
    FreeHeader* free_header = free_block;
    FreeHeader* prev_header = free_header->prev;
    FreeHeader* next_header = free_header->next;
    if (!should_split(free_block, size)) {
        if (prev_header) {
            prev_header->next = next_header;
        } else {
            *free_list_head = next_header;
        }
        if (next_header) {
            next_header->prev = prev_header;
        }
        ((Header*)free_block)->is_free = false;
        return free_block;
    }
    size_t old_size = AS_PTR_TO(Header, free_block)->size;
    size_t required_size = size + HEADER_FOOTER_SIZE;

    void* new_free_block = AS_PTR_TO(byte_t, free_block) + required_size;
    FreeHeader* new_free_header = AS_PTR_TO(FreeHeader, new_free_block);
    Header* new_header = AS_PTR_TO(Header, new_free_block);

    new_free_header->prev = prev_header;
    if (prev_header) {
        prev_header->next = new_free_header;
    } else {
        *free_list_head = new_free_header;
    }
    new_free_header->next = next_header;
    if (next_header) {
        next_header->prev = new_free_header;
    }
    new_header->is_free = true;
    new_header->size = old_size - required_size;
    *get_footer_ptr(new_free_block) = old_size - required_size;

    Header* use_header = AS_PTR_TO(Header, free_block);
    use_header->is_free = false;
    use_header->size = required_size;
    *get_footer_ptr(free_block) = required_size;
    return free_block;
}


static void* get_payload(void* block) {
    return AS_PTR_TO(byte_t, block) + sizeof(Header);
}


static void* allocate(Allocator* allocator, size_t size) {
    if (!allocator || !size) {
        return NULL;
    }
    void* block = find_first_fit(allocator->free_list_head, size);
    if (!block) {
        LOG_ERROR("No fitting block was found");
        return NULL;
    }
    block = split_and_use(&allocator->free_list_head, block, size);
    return get_payload(block);
}


void* allocator_malloc(Allocator* allocator, size_t size) {
    return allocate(allocator, size);
}


void* allocator_calloc(Allocator* allocator, size_t count, size_t object_size) {
    if (!object_size || count > SIZE_MAX / object_size) {
        return NULL;
    }

    size_t size = count * object_size;

    void* pointer = allocate(allocator, size);

    if (!pointer) {
        return NULL;
    }

    memset(pointer, 0, size);

    return pointer;
}


static FreeHeader* find_next_free_header(FreeHeader* header, byte_t* memory) {
#define IS_LAST_BLOCK(header) \
    (memory + HEAP_SIZE <= AS_PTR_TO(byte_t, header) + AS_PTR_TO(Header, header)->size)

#define GET_NEXT_BLOCK(header) \
    (AS_PTR_TO(FreeHeader, AS_PTR_TO(byte_t, header) + AS_PTR_TO(Header, header)->size))

    if (IS_LAST_BLOCK(header)) {
        return NULL;
    }

    FreeHeader* curr = header;
    FreeHeader* next = GET_NEXT_BLOCK(curr);

    while (!next->header.is_free) {
        curr = next;
        if (IS_LAST_BLOCK(curr)) {
            return NULL;
        }
        next = GET_NEXT_BLOCK(curr);
    }

    return next;

#undef GET_NEXT_BLOCK
#undef IS_LAST_BLOCK
}


static FreeHeader* find_prev_free_header(FreeHeader* header, byte_t* memory) {
#define GET_PREV_FOOTER(curr) \
    (AS_PTR_TO(Footer, AS_PTR_TO(byte_t, curr) - sizeof(Footer)))

#define GET_PREV_BLOCK(curr) \
    (AS_PTR_TO(FreeHeader, AS_PTR_TO(byte_t, curr) - *GET_PREV_FOOTER(curr)))

#define IS_FIRST_BLOCK(curr) \
    (AS_PTR_TO(byte_t, curr) == memory)

    if (IS_FIRST_BLOCK(header)) {
        return NULL;
    }

    FreeHeader* curr = header;

    FreeHeader* prev = GET_PREV_BLOCK(curr);

    while (!prev->header.is_free) {
        curr = prev;
        if (IS_FIRST_BLOCK(curr)) {
            return NULL;
        }
        prev = GET_PREV_BLOCK(curr);
    }

    return prev;

#undef IS_FIRST_BLOCK
#undef GET_PREV_BLOCK
#undef GET_PREV_FOOTER
}


static void merge_next_or_connect(FreeHeader* curr, FreeHeader* next) {
    if (AS_PTR_TO(byte_t, curr) + curr->header.size == AS_PTR_TO(byte_t, next)) {
        size_t new_size = curr->header.size + next->header.size;
        curr->header.size = new_size;
        *get_footer_ptr(curr) = new_size;
        FreeHeader* prev = next->prev;
        if (prev) {
            prev->next = curr;
        }
        curr->prev = prev;
        curr->next = next->next;
        if (curr->next) {
            curr->next->prev = curr;
        }
    } else {
        FreeHeader* prev = next->prev;
        if (prev) {
            prev->next = curr;
        }
        curr->prev = prev;
        curr->next = next;
        next->prev = curr;
    }
}


static void merge_prev_or_connect(FreeHeader* prev, FreeHeader* curr) {
    if (AS_PTR_TO(byte_t, prev) + prev->header.size == AS_PTR_TO(byte_t, curr)) {
        size_t new_size = prev->header.size + curr->header.size;
        prev->header.size = new_size;
        *get_footer_ptr(prev) = new_size;
    } else {
        FreeHeader* next = prev->next;
        prev->next = curr;
        curr->prev = prev;
        curr->next = next;
        if (next) {
            next->prev = curr;
        }
    }
}


static void trim(Allocator* allocator, Header* header, size_t new_size) {
    size_t size = header->size;
    header->size = new_size;
    *get_footer_ptr(header) = new_size;

    FreeHeader* curr = AS_PTR_TO(FreeHeader, AS_PTR_TO(byte_t, header) + new_size);
    curr->header.is_free = true;
    curr->header.size = size - new_size;
    *get_footer_ptr(curr) = size - new_size;
    FreeHeader* next = find_next_free_header(curr, allocator->memory);
    if (next) {
        merge_next_or_connect(curr, next);
    }
    FreeHeader* prev = find_prev_free_header(AS_PTR_TO(FreeHeader, header), allocator->memory);
    if (prev) {
        merge_prev_or_connect(prev, curr);
    } else {
        allocator->free_list_head = curr;
    }
}


static void* relocate(Allocator* allocator, Header* header, size_t size) {
    void* pointer = allocate(allocator, size - HEADER_FOOTER_SIZE);
    if (!pointer) {
        return NULL;
    }
    size_t payload_size = header->size - HEADER_FOOTER_SIZE;
    void* payload = get_payload(header);
    memmove(pointer, payload, payload_size);
    allocator_free(allocator, payload);
    return pointer;
}


static void* allocate_more(Allocator* allocator, Header* header, size_t new_size) {
    size_t required_size = new_size - header->size;
    Header* next_header = AS_PTR_TO(Header, AS_PTR_TO(byte_t, header) + header->size);
    if (
        AS_PTR_TO(byte_t, next_header) - allocator->memory >= HEAP_SIZE ||
        !next_header->is_free ||
        next_header->size < required_size ||
        next_header->size - required_size < MINIMUM_BLOCK_SIZE
    ) {
        return relocate(allocator, header, new_size);
    }

    FreeHeader* prev = find_prev_free_header(AS_PTR_TO(FreeHeader, header), allocator->memory);
    FreeHeader* next = AS_PTR_TO(FreeHeader, next_header)->next;
    FreeHeader* curr = AS_PTR_TO(FreeHeader, AS_PTR_TO(byte_t, next_header) + required_size);

    curr->header.is_free = true;
    curr->header.size = next_header->size - required_size;
    *get_footer_ptr(curr) = next_header->size - required_size;

    header->size = new_size;
    *get_footer_ptr(header) = new_size;

    if (prev) {
        prev->next = curr;
    } else {
        allocator->free_list_head = curr;
    }
    curr->prev = prev;
    curr->next = next;
    if (next) {
        next->prev = curr;
    }
    return AS_PTR_TO(byte_t, header) + sizeof(Header);
}


void* allocator_realloc(Allocator* allocator, void* pointer, size_t new_size) {
    if (!pointer) {
        return allocate(allocator, new_size);
    }

    if (!new_size) {
        allocator_free(allocator, pointer);
        return NULL;
    }

    if (new_size > SIZE_MAX - HEADER_FOOTER_SIZE) {
        LOG_ERROR("Not able to allocate the requried memory");
        return NULL;
    }

    size_t total_new_size = new_size + HEADER_FOOTER_SIZE;

    Header* header = AS_PTR_TO(Header, AS_PTR_TO(byte_t, pointer) - sizeof(Header));
    size_t size = header->size;

    if (total_new_size <= size) {
        if (size - total_new_size < MINIMUM_BLOCK_SIZE) {
            return pointer;
        }
        trim(allocator, header, total_new_size);
        return pointer;
    }

    return allocate_more(allocator, header, total_new_size);
}


void allocator_free(Allocator* allocator, void* pointer) {
    if (!allocator || !pointer) {
        return;
    }

    if (AS_PTR_TO(byte_t, pointer) < allocator->memory || AS_PTR_TO(byte_t, pointer) - allocator->memory >= HEAP_SIZE) {
        return;
    }

    FreeHeader* header = AS_PTR_TO(FreeHeader, AS_PTR_TO(byte_t, pointer) - sizeof(Header));
    AS_PTR_TO(Header, header)->is_free = true;
    FreeHeader* next_header = find_next_free_header(header, allocator->memory);
    if (next_header) {
        merge_next_or_connect(header, next_header);
    }
    FreeHeader* prev_header = find_prev_free_header(header, allocator->memory);
    if (prev_header) {
        merge_prev_or_connect(prev_header, header);
    } else {
        allocator->free_list_head = header;
    }
}
