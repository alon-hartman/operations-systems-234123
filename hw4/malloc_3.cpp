#include <unistd.h>
// #include <iostream>
#include <cstring>
#include <sys/mman.h>
#include <sys/user.h>
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

// forward decleration
void* smalloc(size_t);
void sfree(void*);

static size_t allocated_blocks;
static size_t free_blocks;
static size_t allocated_bytes;
static size_t free_bytes;
static size_t metadata_bytes;
static size_t size_of_metadata = sizeof(MallocMetadata);
static void* sbrk_start = sbrk(0);
/**
 * @brief 
 * 
 * @param allocated_blocks # of blocks in list
 * @param free_blocks # of free blocks in list
 * @param allocated_bytes # whole block - metadata
 * @param free_bytes # payload in free package
 * @param metadata_bytes # self explanitory
 */
void updateStatistics(long allocated_blocks_p, long free_blocks_p, long allocated_bytes_p , long free_bytes_p,
                                                                                            long metadata_bytes_p) {
    allocated_blocks += allocated_blocks_p;
    free_blocks += free_blocks_p;
    allocated_bytes += allocated_bytes_p;
    free_bytes += free_bytes_p;
    metadata_bytes += metadata_bytes_p;
}


class BlockList
{
    private:
        MallocMetadata* head = nullptr;
        
    public:
        MallocMetadata* wilderness = nullptr;
        int size = 0;

        void addMetadata(MallocMetadata* metadata) {
            size++;
            if(head == nullptr) {
                head = metadata;
                head->next = nullptr;
                head->prev = nullptr;
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
                else if(!(head->next && head->next->size == metadata->size && metadata > head->next)) {
                    // metadata->size == head->size && metadata > head
                    // add after head
                    metadata->next = head->next;
                    metadata->prev = head;
                    head->next = metadata;
                    if(metadata->next) {
                        metadata->next->prev = metadata;
                    }
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
                    else if(!(head->next && iter->next->size == metadata->size && metadata > iter->next)) {
                        metadata->next = iter->next->next;
                        metadata->prev = iter->next;
                        iter->next->next = metadata;
                        if(metadata->next) {
                            metadata->next->prev = metadata;
                        }
                        return;
                    }
                }
                iter = iter->next;
            }
            // std::cout << "should never reach here: didn't find slot to add metadata" << std::endl;
            size--;
        }

        void removeMetadata(MallocMetadata* metadata) {
            size--;
            if(head == metadata) {
                head = head->next;
                if(head) {
                    head->prev = nullptr;
                }
                return;
            }
            MallocMetadata* iter = head->next;
            while(iter) {
                if(iter == metadata) {
                    iter->prev->next = iter->next;
                    if(iter->next) {
                        iter->next->prev = iter->prev;
                    }
                    return;
                }
                iter = iter->next;
            }
            // std::cout << "should never reach here COPIUM: didn't find metadata to remove" << std::endl;
            size++;
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
                head->next = nullptr;
                head->prev = nullptr;
                return;
            }
            head->prev = metadata;
            metadata->next = head;
            metadata->prev = nullptr;
            head = metadata;
        }

        void removeMetadata(MallocMetadata* metadata) {
            if(head == metadata) {
                head = head->next;
                if(head) {
                    head->prev = nullptr;
                }
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
            // std::cout << "Mmap should never reach here COPIUM: didn't find metadata to remove" << std::endl;
        }
};

BlockList block_list;
// MmapList mmap_list;

/**
 * @brief splits block at address to 2 blocks.
 * 
 * @param size size of new allocated block, smaller than size of old block
 * @param address address of old block, with size much bigger than size
 */
void splitBlock(size_t size, void* address) {
    void* address_low = address;
    void* address_high = (char*)address + size + 2*SOM;

    block_list.removeMetadata((MallocMetadata*)address_low);

    ((MallocMetadata*)address_high)->size = ((MallocMetadata*)address_low)->size - size - 2*SOM;
    ((MallocMetadata*)address_high)->is_free = false;
    ((MallocMetadata*)address_low)->size = size;
    ((MallocMetadata*)address_low)->is_free = false;
    block_list.addMetadata((MallocMetadata*)address_low);
    block_list.addMetadata((MallocMetadata*)address_high);
    std::memmove((char*)address_high + ((MallocMetadata*)address_high)->size + SOM, address_high, SOM);
    std::memmove((char*)address_low + size + SOM, address_low, SOM);

    updateStatistics(1, -1, -2*SOM, -(2*SOM + ((MallocMetadata*)address_low)->size + ((MallocMetadata*)address_high)->size), 2*SOM);
    sfree((MallocMetadata*)address_high + 1);

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
    MallocMetadata* current = (MallocMetadata*)address;
    if(address == sbrk_start) {
        prev_metadata = nullptr;
    }
    else {
        prev_metadata = (MallocMetadata*)((char*)address - SOM);
    }
    if((char*)address + current->size + 2*SOM == sbrk(0)) {
        next_metadata = nullptr;
    }
    else {
        next_metadata = (MallocMetadata*)((char*)address + current->size + 2*SOM);
    }
    bool prev_free = prev_metadata ? prev_metadata->is_free : false;
    bool next_free = next_metadata ? next_metadata->is_free : false;
    if(next_free && prev_free) {
        prev_metadata = (MallocMetadata*)((char*)prev_metadata - prev_metadata->size - SOM);
        prev_metadata->size += next_metadata->size + current->size + 4*SOM;
        block_list.removeMetadata(next_metadata);
        block_list.removeMetadata(prev_metadata);
        block_list.removeMetadata(current);
        block_list.addMetadata(prev_metadata);
        std::memmove((char*)prev_metadata + prev_metadata->size + SOM, prev_metadata, SOM);
        updateStatistics(-2, -1, 4*SOM, current->size + 4*SOM, -4*SOM);
        if(next_metadata == block_list.wilderness) {
            block_list.wilderness = prev_metadata;
        }
    }
    else if(next_free) {
        current->size += next_metadata->size + 2*SOM;
        block_list.removeMetadata(next_metadata);
        block_list.removeMetadata(current);
        block_list.addMetadata(current);
        std::memmove((char*)address + current->size + SOM, address, SOM);
        updateStatistics(-1, 0, 2*SOM, current->size - next_metadata->size, -2*SOM);
        if(next_metadata == block_list.wilderness) {
            block_list.wilderness = (MallocMetadata*)address;
        }
    }
    else if(prev_free) {
        prev_metadata = (MallocMetadata*)((char*)prev_metadata - prev_metadata->size - SOM);
        prev_metadata->size += current->size + 2*SOM;
        block_list.removeMetadata(prev_metadata);
        block_list.removeMetadata(current);
        block_list.addMetadata(prev_metadata);
        std::memmove((char*)prev_metadata + prev_metadata->size + SOM, prev_metadata, SOM);
        updateStatistics(-1, 0, 2*SOM, (current)->size + 2*SOM, -2*SOM);
        if(address == block_list.wilderness) {
            block_list.wilderness = prev_metadata;
        }
    }
    else {
        updateStatistics(0, 1, 0, current->size, 0);
    }
}

void* tryMergeRealloc(void* address, size_t size) {
    MallocMetadata* prev_metadata;
    MallocMetadata* next_metadata;
    MallocMetadata* current = (MallocMetadata*)address;
    if(address == sbrk_start) {
        prev_metadata = nullptr;
    }
    else {
        prev_metadata = (MallocMetadata*)((char*)address - SOM);
    }
    if((char*)address + current->size + 2*SOM == sbrk(0)) {
        next_metadata = nullptr;
    }
    else {
        next_metadata = (MallocMetadata*)((char*)address + current->size + 2*SOM);
    }
    bool prev_free = prev_metadata ? prev_metadata->is_free : false;
    bool next_free = next_metadata ? next_metadata->is_free : false;
    if(prev_free) {
        // case b
        if(current == block_list.wilderness) {
            if(size >= (current->size + prev_metadata->size + 2*SOM)) {
                // need to enlarge wilderness
                size_t added_size = size - (prev_metadata->size + current->size + 2*SOM);
                if(sbrk(added_size) == (void*)-1) {
                    return nullptr;
                }
                current->size += added_size;
                updateStatistics(0, 0, added_size, 0, 0);
                assert(size <= current->size + prev_metadata->size + 2*SOM);
            }
            block_list.wilderness = (MallocMetadata*)((char*)prev_metadata - prev_metadata->size - SOM);
        }
        if(size <= current->size + prev_metadata->size + 2*SOM) {
            prev_metadata = (MallocMetadata*)((char*)prev_metadata - prev_metadata->size - SOM);
            updateStatistics(-1, -1, 2*SOM, -prev_metadata->size, -2*SOM);
            prev_metadata->size += current->size + 2*SOM;
            prev_metadata->is_free = false;
            std::memmove((char*)prev_metadata + SOM, (char*)address + SOM, current->size);
            block_list.removeMetadata(current);
            block_list.removeMetadata(prev_metadata);
            block_list.addMetadata(prev_metadata);
            std::memmove((char*)prev_metadata + prev_metadata->size + SOM, prev_metadata, SOM);
            return (char*)prev_metadata + SOM;
        }
    }
    if(current == block_list.wilderness) {
        // case c
        size_t added_size = size - current->size;
        assert(added_size > 0);
        assert(added_size % 8 == 0);
        updateStatistics(0, 0, added_size, 0, 0);
        if(sbrk(added_size) == (void*)-1) {
            return nullptr;
        }
        assert((unsigned long)sbrk(0) % 8 == 0);
        current->size += added_size;
        current->is_free = false;
        assert(size <= current->size);
        block_list.removeMetadata(current);
        block_list.addMetadata(current);
        std::memmove((char*)current + current->size + SOM, current, SOM);
        return (char*)address + SOM;
    }
    if(next_free && (size <= next_metadata->size + current->size + 2*SOM)) {
        // case d
        updateStatistics(-1, -1, 2*SOM, -next_metadata->size, -2*SOM);
        current->size += next_metadata->size + 2*SOM;
        current->is_free = false;
        block_list.removeMetadata(current);
        block_list.removeMetadata(next_metadata);
        block_list.addMetadata(current);
        std::memmove((char*)current + current->size + SOM, current, SOM);
        return (char*)address + SOM;
    }
    if(prev_free && next_free && (size <= prev_metadata->size + current->size + next_metadata->size + 4*SOM)) {
        // case e
        updateStatistics(-2, -2, 4*SOM, -(prev_metadata->size + next_metadata->size), -4*SOM);
        prev_metadata = (MallocMetadata*)((char*)prev_metadata - prev_metadata->size - SOM);
        prev_metadata->size += current->size + next_metadata->size + 4*SOM;
        prev_metadata->is_free = false;
        std::memmove((char*)prev_metadata + SOM, (char*)current + SOM, current->size);  // copy data from current to prev
        block_list.removeMetadata(prev_metadata);
        block_list.removeMetadata(current);
        block_list.removeMetadata(next_metadata);
        block_list.addMetadata(prev_metadata);
        std::memmove((char*)prev_metadata + prev_metadata->size + SOM, prev_metadata, SOM);  // copy metadata from start to end
        return (char*)prev_metadata + SOM;
    }
    if(next_free && next_metadata == block_list.wilderness) {
        // case f
        if(prev_free) {
            // enlarge wilderness as needed and merge all 3 blocks
            prev_metadata = (MallocMetadata*)((char*)prev_metadata - prev_metadata->size - SOM);
            size_t added_size = size - (prev_metadata->size + current->size + next_metadata->size + 4*SOM);
            assert(added_size > 0);
            assert(added_size % 8 == 0);
            if(sbrk(added_size) == (void*)-1) {
                return nullptr;
            }
            updateStatistics(-2, -2, added_size + 4*SOM, -(prev_metadata->size + next_metadata->size), -4*SOM);
            prev_metadata->size += current->size + next_metadata->size + added_size + 4*SOM;
            prev_metadata->is_free = false;
            block_list.wilderness = prev_metadata;
            std::memmove((char*)prev_metadata + SOM, (char*)current + SOM, current->size);
            block_list.removeMetadata(prev_metadata);
            block_list.removeMetadata(current);
            block_list.removeMetadata(next_metadata);
            block_list.addMetadata(prev_metadata);
            std::memmove((char*)prev_metadata + prev_metadata->size + SOM, prev_metadata, SOM);
            return (char*)prev_metadata + SOM;
        }
        else {
            // enlarge wilderness as needed and merge current with next
            size_t added_size = size - (current->size + next_metadata->size + 2*SOM);
            assert(added_size > 0);
            assert(added_size % 8 == 0);
            if(sbrk(added_size) == (void*)-1){
                return nullptr;
            }
            updateStatistics(-1, -1, added_size + 2*SOM, -next_metadata->size, -2*SOM);
            next_metadata->size += added_size;
            current->size += next_metadata->size + 2*SOM;
            block_list.removeMetadata(current);
            block_list.removeMetadata(next_metadata);
            block_list.addMetadata(current);
            std::memmove((char*)address + current->size + SOM, current, SOM);
            block_list.wilderness = current;
            return (char*)address + SOM;
        }
    }
    return nullptr;
}

void* smalloc(size_t size) {
    if(size == 0 || size > 1e8 /*10^8*/) {
        return nullptr;
    }
    size = (size % 8) ? size + (8 - (size % 8)) : size;
    if(size < MMAP_THRESHOLD) {
        MallocMetadata* address = block_list.searchFreeBlock(size);
        if(address) {
            if(address->size - size - 2*SOM >= MIN_SPLIT_SIZE && address->size - size - 2*SOM < address->size) {
                splitBlock(size, address);
            }
            else {
                address->is_free = false;
                ((MallocMetadata*)((char*)address + address->size + SOM))->is_free = false;
                updateStatistics(0, -1, 0, -((MallocMetadata*)address)->size, 0);
            }
            return (char*)address + SOM;
        }
        void* new_address;
        if(block_list.wilderness && block_list.wilderness->is_free) { 
            //wilderness is free but not big enough
            assert(size > block_list.wilderness->size);
            if(sbrk(size - block_list.wilderness->size) == (void*)-1) {
                return nullptr;
            }
            new_address = (void*)block_list.wilderness;
            updateStatistics(0, -1, size - block_list.wilderness->size, -block_list.wilderness->size, 0);
            block_list.removeMetadata(block_list.wilderness);
        }
        else {
            //either wilderness doesn't exist or not free
            if(block_list.size == 0 && ((unsigned long)sbrk(0) % 8) != 0) {
                size_t remainder = (unsigned long)sbrk(0) % 8;
                sbrk_start = sbrk(8-remainder);
                if(sbrk_start == (void*)-1) {
                    return nullptr;
                }
            }
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
        std::memmove((char*)new_address + size + SOM, new_address, SOM);
        return (char*)new_address + SOM;
    }
    else {
        void* address = mmap(NULL, size + SOM, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 );
        if(address == (void*)-1){
            return nullptr;
        }
        ((MallocMetadata*)address)->size = size;
        ((MallocMetadata*)address)->is_free = false;
        // mmap_list.addMetadata((MallocMetadata*)address);
        updateStatistics(1, 0, size, 0, 2*SOM);
        return (char*)address + SOM;
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
    if(block->size < MMAP_THRESHOLD) {
        MallocMetadata* end_metadata = (MallocMetadata*)((char*)p + block->size);
        end_metadata->is_free = true;
        mergeBlocks(block);
    }
    else {
        // mmap_list.removeMetadata(block);
        updateStatistics(-1, 0, -block->size, 0, -2*SOM);
        munmap(block, block->size + SOM);
    }
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
        // case a
        if(block->size - size - 2*SOM >= MIN_SPLIT_SIZE && block->size - size - 2*SOM < block->size) {
            updateStatistics(0, 1, 0, block->size, 0);  // splitBlock thinks the block is free
            splitBlock(size, block);
        }
        return oldp;
    }
    if(block->size < MMAP_THRESHOLD) {
        // allocated with sbrk
        size_t aligned_size = (size % 8) ? size + (8 - (size % 8)) : size;
        void* address = (char*)tryMergeRealloc(block, aligned_size);  // multiple of 8
        if(address != nullptr) {
            address = (char*)address - SOM;
            if(((MallocMetadata*)address)->size - size - 2*SOM >= MIN_SPLIT_SIZE && 
                ((MallocMetadata*)address)->size - size - 2*SOM < ((MallocMetadata*)address)->size) {
                updateStatistics(0, 1, 0, ((MallocMetadata*)address)->size, 0);  // splitBlock thinks the block is free
                splitBlock(size, address);
            }
            return (char*)address + SOM;
        }
        sfree(oldp);
        address = smalloc(size);
        std::memmove((char*)address, oldp, block->size);
        return (char*)address;
    }
    else {
        // allocated with mmap
        char* newp = (char*)smalloc(size);
        std::memmove(newp, oldp, block->size);
        sfree(oldp);
        return newp;
    }
    return nullptr;
}

/**
 * @brief 
 * Returns the number of allocated blocks in the heap that are currently free.
 * @return size_t 
 */
size_t _num_free_blocks() {
    return free_blocks;
}

/**
 * @brief 
 * Returns the number of bytes in all allocated blocks in the heap that are currently free,
    excluding the bytes used by the meta-data structs.
 * @return size_t 
 */
size_t _num_free_bytes() {
    return free_bytes;
}

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
    return SOM*2;
}