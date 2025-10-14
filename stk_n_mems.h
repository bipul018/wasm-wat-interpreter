// This is intentionally not done, this file is supposed to be included only once
// knowingly as part of some 'unity build' (for now)
// The same reason (also lazy) is why no extra includes are done here
// #pragma once

// Nothing is type safe
DEF_SLICE(u64);
DEF_DARRAY(u64, 16); // Maybe initialize this according to module parameters later
// Only prints as u64 array
void print_stack(u64_Slice stk){
  printf("[ ");
  for_slice(stk, i){
    printf("%llu ", (unsigned long long)slice_inx(stk, i));
  }
  printf(" ]");
}
#define printf_stk(stk, ...)				\
  do{							\
    printf(__VA_ARGS__);				\
    print_stack(_Generic(stk,				\
			 u64_Slice: (stk),		\
			 u64_Darray: (u64_Slice){	\
			   .data=(stk).data,		\
			   .count=(stk).count,		\
			 }));				\
  }while(0)

// A lazily built memory for wasm ?
// Idea: Each memory index will be mapped to pages that can be lazily
//   allocated and realized on read or write operation?
// Turns out this is the page size of wasm , 64KB
#define MEMORY_PAGE_SIZE_POWER 16
#define MEMORY_PAGE_SIZE (1 << MEMORY_PAGE_SIZE_POWER) // Ensure that its always power of two
// Ensure that you never ever not access this without reference
// TODO:: Find out if this is a good strategy??
typedef struct Memory_Page Memory_Page;
struct Memory_Page {
  size_t inx; // The `page_index` for this memory page, not actual offset
  u8 data[MEMORY_PAGE_SIZE];
};
typedef Memory_Page* Memory_Page_Ptr;
DEF_SLICE(Memory_Page_Ptr);
DEF_DARRAY(Memory_Page_Ptr,1);
typedef struct Memory_Region Memory_Region;
struct Memory_Region {
  Memory_Page_Ptr_Darray pages;
  // For cache/optimization purposes :
  //   really just conceptual, to show that this struct
  //   can contain some metadata if needed
  size_t latest_page;
};

Memory_Region memory_rgn_init(Alloc_Interface allocr){
  return (Memory_Region){ .pages = init_Memory_Page_Ptr_darray(allocr) };
}

void memory_rgn_deinit(Memory_Region* mem){
  if(!mem) return;
  for_slice(mem->pages, i){
    Memory_Page* page = slice_inx(mem->pages, i);
    free_mem(mem->pages.allocr, page);
  }
  (void)resize_Memory_Page_Ptr_darray(&mem->pages, 0);
  *mem = (Memory_Region){0};
}

// Utility to get the number of memory pages required
size_t memory_rgn_num_pages(size_t off, size_t size){
  if(size == 0) return 0;
  size_t inx_start = _align_down(off, MEMORY_PAGE_SIZE)/MEMORY_PAGE_SIZE;
  size_t inx_end = _align_up(off+size,MEMORY_PAGE_SIZE)/MEMORY_PAGE_SIZE;
  return (inx_end-inx_start);
}

// utility to 'get' the memory page given the offset
Memory_Page* memory_rgn_get_page(Memory_Region* mem, size_t page_inx){
  // Assumes that the memory page exists always
  if(mem->pages.count && page_inx == slice_inx(mem->pages, mem->latest_page)->inx){
    return slice_inx(mem->pages, mem->latest_page);
  }
  for_slice(mem->pages, i){
    if(slice_inx(mem->pages, i)->inx == page_inx){
      mem->latest_page = i;
      return slice_inx(mem->pages, i);
    }
  }
  return nullptr;
}

typedef struct Memory_Page_Iter Memory_Page_Iter;
struct Memory_Page_Iter {
  size_t iteration, pages, curr_off, size, page_inx, page_off, page_size;
};

Memory_Page_Iter memory_rgn_iter_init(size_t off, size_t size){
  Memory_Page_Iter res = {
    .iteration = 0,
    .pages = memory_rgn_num_pages(off, size),
    .curr_off = off,
    .size = size,
    .page_inx = _align_down(off, MEMORY_PAGE_SIZE)/MEMORY_PAGE_SIZE,
    .page_off = off - _align_down(off, MEMORY_PAGE_SIZE),
  };
  res.page_size = _min(size, MEMORY_PAGE_SIZE-res.page_off);
  return res;
}
bool memory_rgn_iter(Memory_Page_Iter* mem_iter){
  // Whether to end or not is determined by iteration number
  if(mem_iter->iteration >= mem_iter->pages) return false;
  mem_iter->iteration++;
  // Now, set the options so as they are valid
  //   we need, page index, page offset and page size to accurately represent the
  //   ongoing iteration, everything else is ignorable
  if(mem_iter->iteration != 1) {
    mem_iter->size -= MEMORY_PAGE_SIZE-mem_iter->page_off;
    mem_iter->curr_off += MEMORY_PAGE_SIZE;

    mem_iter->page_inx++;
    mem_iter->page_off = 0;
    mem_iter->page_size = _min(mem_iter->size, MEMORY_PAGE_SIZE);
  }
  return true;
}

// Ensures that the memory range is available
bool memory_rgn_ensure(Memory_Region* mem, size_t off, size_t size){
  Memory_Page_Iter iter = memory_rgn_iter_init(off, size);
  while(memory_rgn_iter(&iter)){
    // Search for the page_inx in mem
    for_slice(mem->pages, i){
      Memory_Page* pg = slice_inx(mem->pages, i);
      if(pg->inx == iter.page_inx){
	goto found_this_page;
      }
    }
    // Didnot find that page, need to allocate it
    Memory_Page* mem_page = alloc_mem(mem->pages.allocr, sizeof(Memory_Page),
				      alignof(Memory_Page));
    if(!mem_page) return false;
    if(!push_Memory_Page_Ptr_darray(&mem->pages, mem_page)){
      free_mem(mem->pages.allocr, mem_page);
      return false;
    }
    // TODO:: Maybe memset some values ?
    mem_page->inx = iter.page_inx;
    memset(mem_page->data, 0xcc, MEMORY_PAGE_SIZE);
  found_this_page:
  }
  return true;
}

bool memory_rgn_read(Memory_Region* mem, size_t off, size_t size, void* out){
  // First ensure that those memory region exist
  // TODO:: Assert false or do something else maybe?
  if(!out || !memory_rgn_ensure(mem, off, size)) return false;
  // Then iterate over them and try to read somehow
  Memory_Page_Iter iter = memory_rgn_iter_init(off, size);
  while(memory_rgn_iter(&iter)){
    Memory_Page* page = memory_rgn_get_page(mem, iter.page_inx);
    if(!page) return false;
    memcpy(out, page->data+iter.page_off, iter.page_size);
    out += iter.page_size;
  }
  return true;
}
bool memory_rgn_write(Memory_Region* mem, size_t off, size_t size, const void* in){
  // First ensure that those memory region exist
  // TODO:: Assert false or do something else maybe?
  if(!in || !memory_rgn_ensure(mem, off, size)) return false;

  // Then iterate over them and try to write somehow
  Memory_Page_Iter iter = memory_rgn_iter_init(off, size);
  while(memory_rgn_iter(&iter)){
    Memory_Page* page = memory_rgn_get_page(mem, iter.page_inx);
    if(!page) return false;
    memcpy(page->data+iter.page_off, in, iter.page_size);
    in += iter.page_size;
  }
  return true;
}

// strlen but for the memory pages
size_t memory_rgn_strlen(Memory_Region* mem, const size_t off){
  size_t len = 0;
  u8 ch;
  size_t pinx = _align_down(off, MEMORY_PAGE_SIZE)/MEMORY_PAGE_SIZE;
  size_t poff = off - pinx * MEMORY_PAGE_SIZE;
  while(true){
    Memory_Page* page = memory_rgn_get_page(mem, pinx);
    if(!page) break; // maybe error?
    size_t page_slen = strnlen(page->data+poff, MEMORY_PAGE_SIZE-poff);
    len += page_slen;
    if(page_slen < MEMORY_PAGE_SIZE-poff) break;

    pinx++;
    poff=pinx*MEMORY_PAGE_SIZE;
  }
  return len;
}

Str memory_rgn_cstrdup(Alloc_Interface allocr, Memory_Region* mem, const size_t off){
  u8_Slice str = SLICE_ALLOC(allocr, u8, memory_rgn_strlen(mem, off)+1);
  if(!str.data){
    return (u8_Slice){0};
  }
  if(!memory_rgn_read(mem, off, str.count, str.data)){
    SLICE_FREE(allocr, str);
    return (u8_Slice){0};
  }
  slice_inx(str, str.count-1) = 0;
  //str.count--;
  return str;
}

// TODO:: Test this
void memory_rgn_memset(Memory_Region* mem, const u32 off, const u32 len, u8 v){
  // TODO:: Implement traps and trap when memory fault
  Memory_Page_Iter iter = memory_rgn_iter_init(off, len);
  while(memory_rgn_iter(&iter)){
    Memory_Page* page = memory_rgn_get_page(mem, iter.page_inx);
    if(!page) {
      // TODO:: Memory fault later, for now skip
    } else {
      memset(page->data+iter.page_off, v, iter.page_size);
    }
  }
}

int compare_memory_pages(const void* a, const void* b){
  const Memory_Page** pa = (void*)a;
  const Memory_Page** pb = (void*)b;
  return compare_type_idx(*pa, *pb, offsetof(Memory_Page, inx));
}
void memory_rgn_dump(Memory_Region* mem){
  // Go sequentially in pages?
  Alloc_Interface allocr = gen_std_allocator(); // TODO:: Remove this
  Memory_Page_Ptr_Slice dup = make_copy_Memory_Page_Ptr_slice
    (allocr, (Memory_Page_Ptr_Slice){.data=mem->pages.data, mem->pages.count});
  if(!dup.data) return;
  qsort(dup.data, dup.count, sizeof(dup.data[0]), compare_memory_pages);

  for_slice(dup, i){
    printf("Memory page %zu (%zx-%zx): ", dup.data[i]->inx,
	   dup.data[i]->inx * MEMORY_PAGE_SIZE,
	   (1+dup.data[i]->inx) * MEMORY_PAGE_SIZE);
    
    Memory_Page* page = dup.data[i];
    for_range(size_t, i, 0, MEMORY_PAGE_SIZE){
      const u8 byt = page->data[i];
      // Collect the indexes with the same byte,
      size_t count = 0;
      while((i+count) < MEMORY_PAGE_SIZE && byt == page->data[i+count]) count++;
      i += count - 1;
      if(isgraph(byt)) printf("%c", byt);
      else printf("%02x", byt);
      if(count > 1){
	printf("(x%zu) ", count);
      } else {
	printf(" ");
      }
    }
    printf("\n");
  }
}

// Sample run for memory page
void run_memory_page_sample(Alloc_Interface allocr){
  printf("------------------ Memory Region Tests ---------------\n");

  printf("     Testing some memory region calculation\n");
  const size_t qs[][2] = {
    {0,0}, {0,1}, {MEMORY_PAGE_SIZE-1,1}, {0,MEMORY_PAGE_SIZE-1},
    {0,MEMORY_PAGE_SIZE}, {MEMORY_PAGE_SIZE/2,MEMORY_PAGE_SIZE},
    {MEMORY_PAGE_SIZE/2,MEMORY_PAGE_SIZE+1},
    {MEMORY_PAGE_SIZE,0},{MEMORY_PAGE_SIZE,1},{MEMORY_PAGE_SIZE,2},
    {MEMORY_PAGE_SIZE-1,0},{MEMORY_PAGE_SIZE-1,1},{MEMORY_PAGE_SIZE-1,2},
  };
  for_range(size_t, i, 0, _countof(qs)){
    printf("For off = %zu, size = %zu, memory count = %zu\n",
  	   qs[i][0], qs[i][1], memory_rgn_num_pages(qs[i][0], qs[i][1]));
  }


  Memory_Region mem = memory_rgn_init(allocr);

#define print_pages()							\
  do{									\
    printf("There are %zu pages: [ ", mem.pages.count);			\
    for_slice(mem.pages, i) printf("%zu ", slice_inx(mem.pages, i)->inx); \
    printf(" ]\n");							\
  }while(0)
#define ensure_pages(off, sz)						\
  do{									\
    printf("Ensuring memory from %zu ... %zu(%zu)\n", (size_t)(off), (size_t)((sz)+(off)),(size_t)(sz)); \
    if(memory_rgn_ensure(&mem, (off), (sz))) printf("         --Ensured\n"); \
    else printf("         --Could not ensure\n");			\
    print_pages();							\
  }while(0)


  print_pages();
  //ensure_pages(0, 1);
  //ensure_pages(0, MEMORY_PAGE_SIZE);
  //ensure_pages(0, MEMORY_PAGE_SIZE+1);
  ensure_pages(MEMORY_PAGE_SIZE*10+1000, MEMORY_PAGE_SIZE-100);
  ensure_pages(MEMORY_PAGE_SIZE*50+1000, MEMORY_PAGE_SIZE*2-999);

  u8 buffer[1024] = {0};

#define write_to_pages(str, offset)					\
  do{									\
    printf("Writing `%s` at %zu(from page %zu)...\n",			\
	   (str), (size_t)(offset),					\
	   (size_t)_align_down((size_t)(offset),MEMORY_PAGE_SIZE)/MEMORY_PAGE_SIZE); \
    memory_rgn_write(&mem, (offset), strlen((str)), (str));		\
    print_pages();							\
  }while(0)
  
  write_to_pages("hello, world!!!", 100);

#define read_from_pages(off, len)					\
  do{									\
    printf("Reading %zu bytes from %zu (page %zu)...\n",		\
	   (size_t)(len), (size_t)(off),				\
	   (size_t)_align_down((size_t)(off),MEMORY_PAGE_SIZE)/MEMORY_PAGE_SIZE); \
    memory_rgn_read(&mem, off, len, buffer);				\
    printf("[ ");							\
    for_range(size_t, i, 0, len) {					\
      if(isgraph(buffer[i])) printf("%c ", buffer[i]);			\
      else printf("%02x ", buffer[i]);					\
    }									\
    printf(" ]\n");							\
    print_pages();							\
  }while(0)

  read_from_pages(95, 30);

  read_from_pages(MEMORY_PAGE_SIZE*2-10, 20);

  write_to_pages("Wassap, page 1 and page 2 ?", MEMORY_PAGE_SIZE*5-10);
  read_from_pages(MEMORY_PAGE_SIZE*5-15, 35);

#undef ensure_pages
#undef print_pages
  memory_rgn_deinit(&mem);
  printf("------------------------------------------------------\n");
}
// TODO:: Test across pages str reading, str len, memset, memcpy
