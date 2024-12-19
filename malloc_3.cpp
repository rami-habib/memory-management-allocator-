#include <unistd.h>
#include <cstring>

#include <cmath>
#include <sys/mman.h>
#include <cstdint>

#define MAX_ORDER 10
#define MAX_BLOCK_SIZE 128*1024


////////////// Declearations

struct MallocMetadata {
    int order;
    size_t mm_data_size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
};



class MemoryBlocksList{
public:
    MallocMetadata* m_list_head;

    size_t num_blocks;
    size_t num_bytes;
    size_t meta_data_bytes;


    int order_list;
    bool is_free_list;


    MemoryBlocksList(){};
    MemoryBlocksList(int order, bool is_free);
    void add_new_block(MallocMetadata*);
//    void* find_first_free_block(size_t size);
    void* remove_block(MallocMetadata*);
};


void* smalloc(size_t size);
void* scalloc(size_t num, size_t size);
void sfree(void* p);
void* srealloc(void* oldp, size_t size);
size_t _num_free_blocks();
size_t _num_free_bytes();
size_t _num_allocated_blocks();
size_t _num_allocated_bytes();
size_t _num_meta_data_bytes();
size_t _size_meta_data();




//////////////////// Implementations
MemoryBlocksList::MemoryBlocksList(int order, bool is_free){

    num_blocks = 0;
    num_bytes = 0;
//    if(order > -1 && order < 11){
//        num_bytes = (num_blocks * ((size_t)(pow(2,order)*128) - sizeof(MallocMetadata)));
//    }
    meta_data_bytes = 0;//sizeof(MallocMetadata) * num_blocks;
    m_list_head = nullptr;
    order_list = order;
    is_free_list = is_free;

}


void MemoryBlocksList::add_new_block(MallocMetadata* data) {

    // Input Validation
    if(!data) return;

    // Update Block Counts & Metadata Size
    num_blocks++;
    meta_data_bytes = sizeof(MallocMetadata) * num_blocks;

    // Calculate Total Bytes (Based on Order)
    num_bytes = 0;
    if(order_list > -1 && order_list < 11){
        num_bytes = (num_blocks * ((size_t)(pow(2,order_list)*128) - sizeof(MallocMetadata)));
    }

    // Set Metadata for the New Block
    data->order = order_list;
    data->is_free = is_free_list;
    data->next = nullptr;
    data->prev = nullptr;

    // Handle First Block Insertion
    if (num_blocks == 1){
        m_list_head = data;
        return;
    }

    // Find Insertion Point (Sorted by Address)
    MallocMetadata* tmp = m_list_head;
    MallocMetadata *prev_tmp = nullptr;
    while( data > tmp && tmp != nullptr){ // Assumes 'tmp < data' compares memory addresses
        prev_tmp = tmp;
        tmp = tmp->next;
    }

    if (tmp != nullptr){
        // Insert in Middle or Beginning
        tmp->prev = data;
        data->next = tmp;
        data->prev = prev_tmp;
        if (prev_tmp){
            prev_tmp ->next = data;
        }
        else{ // New block is the new head
            m_list_head = data;
        }
    } else {
        prev_tmp->next = data;
        data->prev = prev_tmp;
        return;
    }

}


void * MemoryBlocksList::remove_block(MallocMetadata * block) {

    if(block == nullptr) return nullptr;

    num_bytes = 0;


    num_blocks--;
    meta_data_bytes = sizeof(MallocMetadata) * num_blocks;

    if(order_list > -1 && order_list < 11){
        num_bytes = (num_blocks * ((size_t)(pow(2,order_list)*128) - sizeof(MallocMetadata)));
    }


    if (block != m_list_head){
        MallocMetadata * next = block->next;
        MallocMetadata *prev = block->prev;
        prev->next = next;
        if (next){
            next->prev = prev;
        }
        return block;
    } else {
        m_list_head = m_list_head->next;
        if (m_list_head){
            m_list_head->prev = nullptr;
        }
        return block;
    }
}

//void* MemoryBlocksList::find_first_free_block(size_t size){
//
//    MallocMetadata* ptr = globalMemoryBlocksList.m_list_head;
//    while(ptr != NULL){
//        if(ptr->size >= size && ptr->is_free){
//            return ptr;
//        }
//        ptr = ptr->next;
//    }
//    return NULL;
//}


size_t calculateBlockSizeFromOrder(int order)
{
    return (((-1 < order) && (order< 11)))?(size_t)(pow(2,order)*128 ):0;
}

size_t sizeOfThisOrder( int order)
{
    return ((-1 < order) && (order< 11))?calculateBlockSizeFromOrder(order) - sizeof(MallocMetadata):0;
}


int supposedOrderOfBlock(size_t size) {


    int order = 0;
    while (size > sizeOfThisOrder(order) && order <= 10) {
        order++;
    }

    return (order > 10) ? -1 : order;
}


class MemoryArrays{
public:
    MemoryBlocksList freeArray[MAX_ORDER + 1];
    MemoryBlocksList allocArray[MAX_ORDER + 1];
    MemoryBlocksList mmMapedBlocks;

    size_t num_free_blocks;
    size_t num_free_Bytes;
    size_t num_allocated_blocks;
    size_t num_allocated_Bytes;
    size_t num_meta_data_Bytes;
    size_t size_meta_data;

    MemoryArrays();
    void changeStats();
    void* initArray();
    void* allocate(size_t size);
    void* suitsbleBlock(size_t size);
    void* allocateInHeap(MallocMetadata* where, size_t size);

};

MemoryArrays::MemoryArrays()
{
    size_meta_data = sizeof(MallocMetadata);
    mmMapedBlocks = MemoryBlocksList(-1, false);

    for (int i = 0; i < MAX_ORDER + 1; i++){
        freeArray[i] = MemoryBlocksList(i, true);
        allocArray[i] = MemoryBlocksList(i, false);
    }

}

void * MemoryArrays::initArray()
{
    // Get the current break (program's end address)
    void* programBreak = sbrk(0);
    if (programBreak == (void*)-1) {
        return nullptr; // sbrk failed
    }

    // Calculate alignment and offset
    const size_t alignmentSize = 32 * 128 * 1024; // 4MB alignment
    intptr_t addressOffset = alignmentSize - ((intptr_t)programBreak % alignmentSize);

    // Extend the program break to achieve alignment
    void* newProgramBreak = sbrk(addressOffset + alignmentSize);
    if (newProgramBreak == (void*)-1) {
        return nullptr; // sbrk failed
    }

    // Adjust the aligned address
    void* alignedAddress = (char*)newProgramBreak + addressOffset;

    // Create and add memory blocks
    MallocMetadata* blockMetadata = (MallocMetadata*)alignedAddress;
    for (int i = 0; i < 32; ++i) {
        blockMetadata = (MallocMetadata*)((char*)alignedAddress + (i * 128 * 1024));
        freeArray[10].add_new_block(blockMetadata);
    }

    // Update internal states (assuming changeStats is defined elsewhere)
    num_free_blocks = 0, num_free_Bytes = 0, num_allocated_blocks = 0, num_allocated_Bytes = 0, num_meta_data_Bytes = 0;
    for(int i = 0; i < MAX_ORDER + 1; i++){
        num_free_blocks += freeArray[i].num_blocks;
        num_free_Bytes += freeArray[i].num_bytes;
        num_allocated_blocks += (allocArray[i].num_blocks + freeArray[i].num_blocks);
        num_allocated_Bytes += (allocArray[i].num_bytes + freeArray[i].num_bytes);
        num_meta_data_Bytes += (allocArray[i].meta_data_bytes + freeArray[i].meta_data_bytes);
    }
    //// MMMAPBLOCKS contiue later /// CHANGE TALIA
    MallocMetadata* tmp = mmMapedBlocks.m_list_head;
    while (tmp){
        num_allocated_Bytes+= tmp->mm_data_size;
        num_allocated_blocks++;
        num_meta_data_Bytes += size_meta_data;
        tmp = tmp->next;
    }

    // Return a success indicator (non-null value)
    return (void*)1;
}

void MemoryArrays::changeStats() {

    size_t t_num_free_blocks = 0, t_num_free_Bytes = 0, t_num_allocated_blocks = 0, t_num_allocated_Bytes = 0, t_num_meta_data_Bytes = 0;
    for(int i = 0; i < MAX_ORDER + 1; i++){
        t_num_free_blocks += freeArray[i].num_blocks;
        t_num_free_Bytes += freeArray[i].num_bytes;
        t_num_allocated_blocks += (allocArray[i].num_blocks + freeArray[i].num_blocks);
        t_num_allocated_Bytes += (allocArray[i].num_bytes + freeArray[i].num_bytes);
        t_num_meta_data_Bytes += (allocArray[i].meta_data_bytes + freeArray[i].meta_data_bytes);
    }
    //// MMMAPBLOCKS contiue later /// CHANGE TALIA
    MallocMetadata* tmp = mmMapedBlocks.m_list_head;
    while (tmp != nullptr){
        t_num_allocated_Bytes+= tmp->mm_data_size;
        t_num_allocated_blocks++;
        t_num_meta_data_Bytes += size_meta_data;
        tmp = tmp->next;
    }
    num_free_blocks = t_num_free_blocks, num_free_Bytes = t_num_free_Bytes, num_allocated_blocks = t_num_allocated_blocks, num_allocated_Bytes = t_num_allocated_Bytes, num_meta_data_Bytes = t_num_meta_data_Bytes;
}

void* MemoryArrays::suitsbleBlock(size_t size) {
    int order = supposedOrderOfBlock(size);
    if(order == -1) return NULL;

    while(order <= MAX_ORDER){
        if(freeArray[order].m_list_head != nullptr){
            return freeArray[order].m_list_head;
        } else{
            order ++;
        }
    }
    return nullptr;
}



void*  MemoryArrays::allocateInHeap(MallocMetadata* where,size_t size){


    freeArray[where->order].remove_block(where);
    for(int i =  where->order - 1; i >= supposedOrderOfBlock(size); --i){
        freeArray[i].add_new_block((MallocMetadata*)((char*)where + (int)(128 * pow(2, i))));
    }
    allocArray[supposedOrderOfBlock(size)].add_new_block(where);
    changeStats();
    return ((char*)where + size_meta_data);
}

void* MemoryArrays::allocate(size_t size){
    MallocMetadata* where = (MallocMetadata*)suitsbleBlock(size);
    if(!where){

        if(supposedOrderOfBlock(size) != -1){
            /////// if we got here that means that there is no free block that can contain 'size' bytes and size is not big enough for creating a page for it so we return null
            return nullptr;
        }
        /// code for mmap
        void* new_block = mmap(NULL, size  + size_meta_data, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if(new_block == MAP_FAILED){
            return nullptr;
        }
        MallocMetadata * meta = (MallocMetadata*)new_block;
        meta->mm_data_size = size;
        mmMapedBlocks.add_new_block(meta);
        changeStats();
        changeStats();

        return ((char *)meta + size_meta_data);
    }

    return allocateInHeap(where,size);
}



MemoryArrays globalArrays = MemoryArrays();
bool firstCall = true;



size_t blockSize(int order){
    return (size_t)(pow(2, order) * 128 );
}




void* firstCallForSmalloc(size_t size){
    firstCall = false;
    if(!globalArrays.initArray()){
        return NULL;
    }
    if(size == 0 || size > 100000000){
        return NULL;
    }
    return globalArrays.allocate(size);
}


void* smalloc(size_t size){

    if(firstCall){
        return firstCallForSmalloc(size);
    }
    if(size == 0 || size > 100000000){
        return NULL;
    }
    return globalArrays.allocate(size);
}


void* scalloc(size_t num, size_t size){

    void* block = smalloc(num * size);
    if(block == NULL) return NULL;
    memset(block, 0, num * size );
    return (block);
}



void freeMap(MallocMetadata* meta){

    globalArrays.mmMapedBlocks.remove_block(meta);
    munmap(meta, meta->mm_data_size + globalArrays.size_meta_data);
    globalArrays.changeStats();
    return;

}



void freeHeap(MallocMetadata* meta) {
    // Remove the block from the allocated list
    globalArrays.allocArray[meta->order].remove_block(meta);

    MallocMetadata* min_meta = meta;
    int current_order = meta->order;

    // Attempt to merge with free buddies
    bool canMerge = true;
    while (canMerge) {
        // Calculate the buddy's address
        MallocMetadata* buddy_meta = (MallocMetadata*)((uintptr_t)min_meta ^ (size_t)(pow(2, current_order) * 128));

        // Check if merging is possible
        canMerge = (current_order != MAX_ORDER
                    && buddy_meta->is_free
                    && buddy_meta->order == current_order);

        if (canMerge) {
            // Remove buddy from the free list and merge
            globalArrays.freeArray[buddy_meta->order].remove_block(buddy_meta);
            min_meta = (min_meta < buddy_meta) ? min_meta : buddy_meta; // Choose the lower address as the new base
            current_order++;
        }
    }

    // Add the (potentially merged) block to the free list
    globalArrays.freeArray[current_order].add_new_block(min_meta);

    // Update global statistics
    globalArrays.changeStats();
}

void sfree(void* p){

    if(p == NULL) return;
    MallocMetadata* meta = (MallocMetadata*)((char*)p - globalArrays.size_meta_data);
    if(meta->is_free){
        return;
    }
    if(meta->order == -1){
        freeMap(meta);
        return;
    } else {
        freeHeap(meta);
    }
}


void* mapRealloc(void* oldp, size_t size,MallocMetadata* meta){


    if (meta->mm_data_size == size) {
        return oldp;
    }

    void* new_block = mmap(NULL, size + globalArrays.size_meta_data, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (new_block == MAP_FAILED) {
        return nullptr;
    }

    MallocMetadata* new_meta = (MallocMetadata*)new_block;
    new_meta->mm_data_size = size;
    globalArrays.mmMapedBlocks.add_new_block(new_meta);
    globalArrays.mmMapedBlocks.remove_block(meta);
    globalArrays.changeStats();
    memmove(((char*)new_block + globalArrays.size_meta_data), oldp, meta->mm_data_size);
    munmap(meta, (meta->mm_data_size + globalArrays.size_meta_data));
    return ((char*)new_block + globalArrays.size_meta_data);

}


void* canMergeBuddies(void* oldp, size_t size,int new_order,int curr_order,MallocMetadata* meta,MallocMetadata* min_meta){
    min_meta = meta;
    for (int i = curr_order; i < new_order; ++i) {
        MallocMetadata* buddy_meta = (MallocMetadata*)((uintptr_t)min_meta ^ blockSize(i));
        globalArrays.freeArray[buddy_meta->order].remove_block(buddy_meta);
        min_meta = (min_meta < buddy_meta) ? min_meta : buddy_meta;
    }

    globalArrays.allocArray[meta->order].remove_block(meta);
    globalArrays.allocArray[new_order].add_new_block(min_meta);
    globalArrays.changeStats();
    memmove(((char*)min_meta + globalArrays.size_meta_data), oldp, blockSize(curr_order));
    return ((char*)min_meta + globalArrays.size_meta_data);
}

void* heapRealloc(void* oldp, size_t size,MallocMetadata* meta){

    int new_order = supposedOrderOfBlock(size);
    int curr_order = meta->order;

    if (new_order <= meta->order) {
        return oldp;
    }

    MallocMetadata* min_meta = meta;
    bool canMerge = true;

    // Modified loop with explicit condition
    for (int i = curr_order; i < new_order && canMerge; ++i) {
        MallocMetadata* buddy_meta = (MallocMetadata*)((uintptr_t)min_meta ^ calculateBlockSizeFromOrder(i));
        canMerge = (buddy_meta->is_free && buddy_meta->order == i);

        if (canMerge) {
            min_meta = (min_meta < buddy_meta) ? min_meta : buddy_meta;
        }
    }

    if (canMerge) {
        return canMergeBuddies(oldp,size,new_order,curr_order,meta,min_meta);
    } else {
        void* pointer_to_return = smalloc(size);
        memmove(pointer_to_return, oldp, blockSize(curr_order));
        sfree(oldp);
        return pointer_to_return;
    }

}

void* srealloc(void* oldp, size_t size) {
    if (oldp == NULL) {
        return smalloc(size);
    }

    MallocMetadata* meta = (MallocMetadata*)((char*)oldp - globalArrays.size_meta_data);

    if (meta->order == -1) { // mmap'ed block
        return mapRealloc(oldp,size,meta);
    } else {
        return heapRealloc(oldp,size,meta);
    }
}

size_t _num_free_blocks(){
    return globalArrays.num_free_blocks;
}

size_t _num_free_bytes(){
    return globalArrays.num_free_Bytes;
}

size_t _num_allocated_blocks(){
    return globalArrays.num_allocated_blocks;
}

size_t _num_allocated_bytes(){
    return globalArrays.num_allocated_Bytes;
}

size_t _num_meta_data_bytes(){
    return globalArrays.num_meta_data_Bytes;
}

size_t _size_meta_data(){
    return globalArrays.size_meta_data;
}







