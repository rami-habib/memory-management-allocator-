#include <unistd.h>
#include <cstring>


////////////// Declearations

struct MallocMetadata {
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
};



class BlockList{


public:
    MallocMetadata* m_list_head;
    size_t num_free_blocks;
    size_t num_free_Bytes;
    size_t num_allocated_blocks;
    size_t num_allocated_Bytes;
    size_t num_meta_data_Bytes;
    size_t size_meta_data;

    BlockList();
    void _add_new_block(MallocMetadata*);
    void* find_first_free_block(size_t size);
//    void _set_block_as_free(MallocMetadata*);
//    void _set_block_as_allocated(MallocMetadata*);
//    size_t get_block_size(MallocMetadata*);
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
BlockList::BlockList(){
    m_list_head = nullptr;
    num_free_blocks = 0;
    num_free_Bytes = 0;
    num_allocated_blocks = 0;
    num_allocated_Bytes = 0;
    num_meta_data_Bytes = 0;
    size_meta_data = sizeof(MallocMetadata);
}

void BlockList::_add_new_block(MallocMetadata* data) {

    num_allocated_blocks++;
    num_allocated_Bytes += data->size;
    num_meta_data_Bytes += size_meta_data;

    if (num_allocated_blocks == 1){
        m_list_head = data;
        data->is_free = false;
        data->next = nullptr;
        data->prev = nullptr;
        return;
    }

    MallocMetadata* tmp = m_list_head;
    while(tmp->next){
        tmp = tmp->next;
    }
    tmp->next = data;
    data->is_free = false;
    data->next = nullptr;
    data->prev = tmp;
}


BlockList globalBlocklist = BlockList();

void* BlockList::find_first_free_block(size_t size){

    MallocMetadata* ptr = globalBlocklist.m_list_head;
    while(ptr != NULL){
        if(ptr->size >= size && ptr->is_free){
            return ptr;
        }
        ptr = ptr->next;
    }
    return NULL;
}






//////////




void* smalloc(size_t size){

    if(size == 0 || size > 100000000){
        return NULL;
    }

    MallocMetadata* block = (MallocMetadata*)globalBlocklist.find_first_free_block(size);

    if(block != NULL){
        /////////// we found an allocated block that has at least 'size' bytes so we should mark it as allocated
        block->is_free = false;
        globalBlocklist.num_free_blocks--;
        globalBlocklist.num_free_Bytes -= size;

    } else {
        ///// we did not find an allocated block that has at least 'size' bytes so we should create a new MallocMetadata struct
        void* ptr = sbrk(size + globalBlocklist.size_meta_data);
        if(ptr == (void*) -1) return NULL;
        block = (MallocMetadata*)ptr;
        block->size = size;
        globalBlocklist._add_new_block(block);
    }

    return ((char*)block + globalBlocklist.size_meta_data);

}


void* scalloc(size_t num, size_t size){

    void* block = smalloc(num * size);
    if(block == NULL) return NULL;
    //void* dest = block + globalBlocklist.size_meta_data;
    memset(block/*dest*/, 0, num * size ); // the meta data is already skipped in the smalloc return value
    return (block);
}


void sfree(void* p){

    if(p == NULL) return;
    MallocMetadata* meta = (MallocMetadata*)((char*)p - globalBlocklist.size_meta_data);
    if(meta->is_free) return;

    meta->is_free = true;
    globalBlocklist.num_free_blocks++;
    globalBlocklist.num_free_Bytes += meta->size;

}


void* srealloc(void* oldp, size_t size){

    if(oldp == NULL){
        return smalloc(size);
    }
    MallocMetadata* block = (MallocMetadata*)((char*)oldp - globalBlocklist.size_meta_data);

    if(size <= block->size){
        return oldp;
    }

    void* newp = smalloc(size);
    if(newp == NULL) return NULL;
    memmove(newp, oldp, block->size);
    sfree(oldp);
    return newp;
}

size_t _num_free_blocks(){
    return globalBlocklist.num_free_blocks;
}

size_t _num_free_bytes(){
    return globalBlocklist.num_free_Bytes;
}

size_t _num_allocated_blocks(){
    return globalBlocklist.num_allocated_blocks;
}

size_t _num_allocated_bytes(){
    return globalBlocklist.num_allocated_Bytes;
}

size_t _num_meta_data_bytes(){
    return globalBlocklist.num_meta_data_Bytes;
}

size_t _size_meta_data(){
    return globalBlocklist.size_meta_data;
}







