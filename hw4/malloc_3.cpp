#include <unistd.h>
#include <iostream>
#include <cstring>
#include <cassert>

#define MIN_SPLIT_SIZE 128
#define SOM size_of_metadata
#define MMAP_THRESHOLD 128*1024

struct MallocMetadata {
    // payload size in bytes
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
};

void updateStatistics(size_t allocated_blocks, size_t free_blocks, size_t allocated_bytes , size_t free_bytes,
                                                                                            size_t metadata_bytes) {
    allocated_blocks += allocated_blocks;
    free_blocks += free_blocks;
    allocated_bytes += allocated_bytes;
    free_bytes += free_bytes;
    metadata_bytes += metadata_bytes;
}

static size_t allocated_blocks;
static size_t free_blocks;
static size_t allocated_bytes;
static size_t free_bytes;
static size_t metadata_bytes;
static size_t size_of_metadata = sizeof(MallocMetadata);
static void* sbrk_start = sbrk(0);

class BlockList
{
    private:
        MallocMetadata* head = nullptr;
        
    public:
        MallocMetadata* wilderness = nullptr;

        void addMetadata(MallocMetadata* metadata) {
            if(head == nullptr) {
                head = metadata;
                return;
            }
            else if(metadata->size <= head->size) {
                if(metadata->size < head->size || (metadata->size == head->size && metadata < head)) {
                    metadata->next = head;
                    metadata->prev = head->prev;
                    metadata->next->prev = metadata;
                    head = metadata;
                    return;
                }
                else {
                    // metadata->size == head->size && metadata > head
                    // add after head
                    metadata->next = head->next;
                    metadata->prev = head;
                    head->next = metadata;
                    return;
                }
            }
            MallocMetadata* iter = head;
            while(iter) {
                if(iter->next == nullptr) {
                    metadata->prev = iter;
                    metadata->next = nullptr;
                    iter->next = metadata;
                    return;
                }
                if(metadata->size <= iter->next->size) {
                    if(metadata->size < iter->next->size || (metadata->size == iter->next->size && metadata < iter->next)) {
                        metadata->prev = iter;
                        metadata->next = iter->next;
                        iter->next = metadata;
                        metadata->next->prev = metadata;
                        return;
                    }
                    else {
                        metadata->next = iter->next->next;
                        metadata->prev = iter->next;
                        iter->next->next = metadata;
                        return;
                    }
                }
                iter = iter->next;
            }
            std::cout << "should never reach here: didn't find slot to add metadata" << std::endl;
        }

        void removeMetadata(MallocMetadata* metadata) {
            if(head == metadata) {
                head = head->next;
                head->prev = nullptr;
                return;
            }
            MallocMetadata* iter = head->next;
            while(iter) {
                if(head == metadata) {
                    head->prev->next = head->next;
                    if(head->next) {
                        head->next->prev = head->prev;
                    }
                    return;
                }
                iter = iter->next;
            }
            std::cout << "should never reach here COPIUM: didn't find metadata to remove" << std::endl;
        }

        /**
         * @brief search for first (address-wise) free block of at least size bytes
         * 
         * @param size payload length
         * @return MallocMetadata*: pointer to entire block, including metadata
         */
        MallocMetadata* searchFreeBlock(size_t size) {
            MallocMetadata* iter = head;
            while(iter){
                if(iter->is_free && iter->size >= size){
                    return iter;
                }
                iter = iter->next;
            }
            return nullptr;
        }
};


class MmapList
{
    private:
        MallocMetadata* head = nullptr;
        
    public:
        void addMetadata(MallocMetadata* metadata) {
            if(head == nullptr) {
                head = metadata;
                return;
            }
            head->prev = metadata;
            metadata->next = head;
            head = metadata;
        }

        void removeMetadata(MallocMetadata* metadata) {
            if(head == metadata) {
                head = head->next;
                head->prev = nullptr;
                return;
            }
            MallocMetadata* iter = head->next;
            while(iter) {
                if(head == metadata) {
                    head->prev->next = head->next;
                    if(head->next) {
                        head->next->prev = head->prev;
                    }
                    return;
                }
                iter = iter->next;
            }
            std::cout << "Mmap should never reach here COPIUM: didn't find metadata to remove" << std::endl;
        }
};

BlockList block_list;
MmapList mmap_list;

/**
 * @brief splits block at address to 2 blocks.
 * 
 * @param size size of new allocated block, smaller than size of old block
 * @param address address of old block, with size much bigger than size
 */
void splitBlock(size_t size, void* address) {
    void* address_low = address;
    void* address_high = (void*)address + size + 2*SOM;

    block_list.removeMetadata((MallocMetadata*)address_low);

    ((MallocMetadata*)address_high)->size = ((MallocMetadata*)address_low)->size - size;
    ((MallocMetadata*)address_high)->is_free = true;
    ((MallocMetadata*)address_low)->size = size;
    ((MallocMetadata*)address_low)->is_free = false;
    block_list.addMetadata((MallocMetadata*)address_low);
    block_list.addMetadata((MallocMetadata*)address_high);
    std::memmove(address_high + ((MallocMetadata*)address_high)->size + SOM, address_high,
                                                                             ((MallocMetadata*)address_high)->size);
    std::memmove(address_low + size + SOM, address_low, size);

    updateStatistics(1, 0, -2*SOM, -(2*SOM + size), 2*SOM);

    if(address_low == block_list.wilderness) {
        block_list.wilderness = (MallocMetadata*)address_high;
    }
}

/**
 * @brief Merge a newly freed block with his adjacent free blocks
 * 
 * @param address of start of metadata (NOT PAYLOAD)
 */
void mergeBlocks(void* address) {
    MallocMetadata* prev_metadata;
    MallocMetadata* next_metadata;
    if(address == sbrk_start) {
        prev_metadata = nullptr;
    }
    else {
        prev_metadata = (MallocMetadata*)(address - SOM);
    }
    if(address + ((MallocMetadata*)address)->size + 2*SOM == sbrk(0)) {
        next_metadata = nullptr;
    }
    else {
        next_metadata = (MallocMetadata*)(address + ((MallocMetadata*)address)->size + SOM);
    }
    bool prev_free = prev_metadata ? prev_metadata->is_free : false;
    bool next_free = next_metadata ? next_metadata->is_free : false;
    if(next_free && prev_free) {
        prev_metadata = prev_metadata - prev_metadata->size - SOM;
        prev_metadata->size += next_metadata->size + ((MallocMetadata*)address)->size + 4*SOM;
        block_list.removeMetadata(next_metadata);
        block_list.removeMetadata(prev_metadata);
        block_list.removeMetadata((MallocMetadata*)address);
        block_list.addMetadata(prev_metadata);
        std::memmove((void*)prev_metadata + prev_metadata->size + 2*SOM, prev_metadata, SOM);
        updateStatistics(-2, -1, 4*SOM, ((MallocMetadata*)address)->size + 4*SOM, -4*SOM);
        if(next_metadata == block_list.wilderness) {
            block_list.wilderness = prev_metadata;
        }
    }
    else if(next_free) {
        ((MallocMetadata*)address)->size += next_metadata->size + 2*SOM;
        block_list.removeMetadata(next_metadata);
        block_list.removeMetadata((MallocMetadata*)address);
        block_list.addMetadata((MallocMetadata*)address);
        std::memmove(address + ((MallocMetadata*)address)->size + 2*SOM, address, SOM);
        updateStatistics(-1, 0, 2*SOM, ((MallocMetadata*)address)->size + 2*SOM, -2*SOM);
        if(next_metadata == block_list.wilderness) {
            block_list.wilderness = (MallocMetadata*)address;
        }
    }
    else if(prev_free) {
        prev_metadata = prev_metadata - prev_metadata->size - SOM;
        prev_metadata->size += ((MallocMetadata*)address)->size + 2*SOM;
        block_list.removeMetadata(prev_metadata);
        block_list.removeMetadata((MallocMetadata*)address);
        block_list.addMetadata(prev_metadata);
        std::memmove((void*)prev_metadata + prev_metadata->size + 2*SOM, prev_metadata, SOM);
        updateStatistics(-1, 0, 2*SOM, ((MallocMetadata*)address)->size + 2*SOM, -2*SOM);
        if(address == block_list.wilderness) {
            block_list.wilderness = prev_metadata;
        }
    }
    else {
        updateStatistics(0, 1, 0, ((MallocMetadata*)address)->size, 0);
    }
}

void* smalloc(size_t size) {
    if(size == 0 || size > 100000000 /*10^8*/) {
        return nullptr;
    }
    if(size < MMAP_THRESHOLD) {
        MallocMetadata* address = block_list.searchFreeBlock(size);
        if(address) {
            if(address->size - size - 2*SOM >= MIN_SPLIT_SIZE) {
                splitBlock(size, address);
            }
            else {
                ((MallocMetadata*)address)->is_free = false;
                updateStatistics(0, -1, 0, -((MallocMetadata*)address)->size, 0);
            }
            return (void*)address + SOM;
        }
        void* new_address;
        if(block_list.wilderness && block_list.wilderness->is_free) { 
            //wilderness is free but not big enough
            assert(size > block_list.wilderness->size);
            if(sbrk(size - block_list.wilderness->size) == (void*)-1) {
                return nullptr;
            }
            new_address = (void*)block_list.wilderness;
            updateStatistics(0, -1, size - block_list.wilderness->size, 0, 0);
            block_list.removeMetadata(block_list.wilderness);
        }
        else {
            //either wilderness doesnt exist or he is not free
            new_address = sbrk(size + 2*SOM);
            if(new_address == (void*)-1) {
                return nullptr;
            }
            block_list.wilderness = (MallocMetadata*)new_address;
            updateStatistics(1, 0, size, 0, 2*SOM);
        }
        ((MallocMetadata*)new_address)->size = size;
        ((MallocMetadata*)new_address)->is_free = false;
        block_list.addMetadata((MallocMetadata*)new_address);
        std::memmove(new_address + SOM + size, new_address, SOM);
        return new_address + SOM;
    }
    else {
        //TODO: complete mmap list (everything)
    }
}

void* scalloc(size_t num, size_t size) {
    void* address = smalloc(num * size);
    if(address == nullptr) {
        return nullptr;
    }
    std::memset(address, 0, size*num);
    return address;
}

void sfree(void* p) {
    if(p == nullptr) {
        return;
    }
    MallocMetadata* block = (MallocMetadata*)p - 1;
    if(block->is_free == true) {
        return;
    }
    block->is_free = true;
    MallocMetadata* end_metadata = (MallocMetadata*)((void*)block + block->size);
    end_metadata->is_free = true;
    mergeBlocks(block);
}

void* srealloc(void* oldp, size_t size) {
    if(size == 0 || size > 100000000 /*10^8*/) {
        return nullptr;
    }
    if(oldp == nullptr){
         return smalloc(size);
    }
    MallocMetadata* block = (MallocMetadata*)oldp - 1;
    if(block->size >= size) {
        return oldp;
    }
    void* newp = smalloc(size);
    if(newp != nullptr) {
        std::memmove(newp, oldp, size);
        sfree(oldp);
    }
    return newp;
    
}

/**
 * @brief 
 * Returns the number of allocated blocks in the heap that are currently free.
 * @return size_t 
 */
size_t _num_free_blocks(){
    return free_blocks;
};

/**
 * @brief 
 * Returns the number of bytes in all allocated blocks in the heap that are currently free,
    excluding the bytes used by the meta-data structs.
 * @return size_t 
 */
size_t _num_free_bytes(){
    return free_bytes;
};

/**
 * @brief 
 * Returns the overall (free and used) number of allocated blocks in the heap.
 * @return size_t 
 */
size_t _num_allocated_blocks() {
    return allocated_blocks;
}

/**
 * @brief 
 * Returns the overall number (free and used) of allocated bytes in the heap, excluding
    the bytes used by the meta-data structs.
 * @return size_t 
 */
size_t _num_allocated_bytes() {
    return allocated_bytes;
}

/** 
 * @brief 
 * Returns the overall number of meta-data bytes currently in the heap.
 * @return size_t 
*/
size_t _num_meta_data_bytes() {
    return metadata_bytes;
}
/**
 * @brief 
 * Returns the number of bytes of a single meta-data structure in your system.
 * @return size_t 
 */
size_t _size_meta_data() {
    return SOM;
}