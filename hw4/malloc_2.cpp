#include <unistd.h>
#include <iostream>
#include <cstring>

struct MallocMetadata {
    // payload size in bytes
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
};

class BlockList
{
    private:
        MallocMetadata* head = nullptr;
        
    public:
        size_t allocated_blocks = 0;
        size_t free_blocks = 0;
        size_t allocated_bytes = 0;
        size_t free_bytes = 0;
        size_t metadata_bytes = 0;
        size_t size_of_metadata = sizeof(MallocMetadata);

        void addMetadata(MallocMetadata* metadata) {
            if(head == nullptr) {
                head = metadata;
                return;
            }
            else if(metadata < head) {
                metadata->next = head;
                metadata->prev = head->prev;
                metadata->next->prev = metadata;
                head = metadata;
                return;
            }
            MallocMetadata* iter = head;
            while(iter) {
                if(iter->next == nullptr) {
                    metadata->prev = iter;
                    metadata->next = nullptr;
                    iter->next = metadata;
                    return;
                }
                if(metadata < iter->next) {
                    metadata->prev = iter;
                    metadata->next = iter->next;
                    iter->next = metadata;
                    metadata->next->prev = metadata;
                    return;
                }
                iter = iter->next;
            }
            std::cout << "should never reach here: didn't find slot to add metadata" << std::endl;
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
        void updateStatistics(size_t allocated_blocks, size_t free_blocks, size_t allocated_bytes , size_t free_bytes,
                                                                                            size_t metadata_bytes) {
            this->allocated_blocks += allocated_blocks;
            this->free_blocks += free_blocks;
            this->allocated_bytes += allocated_bytes;
            this->free_bytes += free_bytes;
            this->metadata_bytes += metadata_bytes;
        }
};

BlockList block_list;

void* smalloc(size_t size) {
    if(size == 0 || size > 100000000 /*10^8*/) {
        return nullptr;
    }
    MallocMetadata* address = block_list.searchFreeBlock(size);
    if(address) {
        ((MallocMetadata*)address)->is_free = false;
        block_list.updateStatistics(0, -1, 0, -((MallocMetadata*)address)->size, 0);
        return (void*)address + block_list.size_of_metadata;
    }
    void* new_address = sbrk(size + block_list.size_of_metadata);
    if(new_address == (void*)-1) {
        return nullptr;
    }
    ((MallocMetadata*)new_address)->is_free = false;
    ((MallocMetadata*)new_address)->size = size;
    block_list.updateStatistics(1, 0, size, 0, block_list.size_of_metadata);
    block_list.addMetadata((MallocMetadata*)new_address);
    return new_address + block_list.size_of_metadata;
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
    block_list.updateStatistics(0, 1, 0, block->size, 0);
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
    return block_list.free_blocks;
};

/**
 * @brief 
 * Returns the number of bytes in all allocated blocks in the heap that are currently free,
    excluding the bytes used by the meta-data structs.
 * @return size_t 
 */
size_t _num_free_bytes(){
    return block_list.free_bytes;
};

/**
 * @brief 
 * Returns the overall (free and used) number of allocated blocks in the heap.
 * @return size_t 
 */
size_t _num_allocated_blocks() {
    return block_list.allocated_blocks;
}

/**
 * @brief 
 * Returns the overall number (free and used) of allocated bytes in the heap, excluding
    the bytes used by the meta-data structs.
 * @return size_t 
 */
size_t _num_allocated_bytes() {
    return block_list.allocated_bytes;
}

/** 
 * @brief 
 * Returns the overall number of meta-data bytes currently in the heap.
 * @return size_t 
*/
size_t _num_meta_data_bytes() {
    return block_list.metadata_bytes;
}
/**
 * @brief 
 * Returns the number of bytes of a single meta-data structure in your system.
 * @return size_t 
 */
size_t _size_meta_data() {
    return block_list.size_of_metadata;
}