#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#define UTIL_INCLUDE_ALL
#include "../c-utils/util_headers.h"
#define slice_last(slice) slice_inx((slice), ((slice).count-1))
#define slice_first(slice) slice_inx((slice), 0)


DEF_SLICE(u8);
DEF_DARRAY(u8, 8);
typedef u8_Slice Str;
typedef const char* Cstr;
typedef u8_Darray Str_Builder;
DEF_SLICE(Str);
DEF_DARRAY(Str, 8);
Str str_slice(Str str, size_t begin, size_t end){
  if(nullptr == str.data || begin >= end || end > str.count) return (Str){ 0 };
  return (Str){ .data = str.data + begin, .count = end - begin };
}
int str_str_cmp(Str str1, Str str2){
  const size_t l = _min(str1.count, str2.count);
  int v = strncmp(str1.data, str2.data, l);
  if(v != 0) return v;
  return (int)(str1.count - l) - (int)(str2.count - l);
}
int str_cstr_cmp(Str str1, Cstr str2){
  return str_str_cmp(str1, (Str){.data=(void*)str2,.count=strlen(str2)});
}
int cstr_str_cmp(Cstr str1, Str str2){
  return str_str_cmp((Str){.data=(void*)str1,.count=strlen(str1)},str2);
}

#define slice_shrink_front(slice_iden, amt)	\
  do{						\
    if((slice_iden).count < amt) {		\
      (slice_iden).data = nullptr;		\
      (slice_iden).count = 0;			\
      break;					\
    }						\
    (slice_iden).count -= amt;			\
    (slice_iden).data += amt;			\
  }while(0)
  

#define str_print(str) ((int)(str).count), (str).data
#include "lines_and_files.h"
#include "parse_node.h"
#include "parse_into_trees.h"
#include "parsed_utils.h"

#include "parsed_type.h"
#include "parsed_func.h"
#include "parsed_exports.h" 
#include "parsed_module.h"

// Try to run a given function
// Assume all the functions are ordered correctly ?

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
    printf("\n");					\
  }while(0)

// A lazily built memory for wasm ?
// Idea: Each memory index will be mapped to pages that can be lazily
//   allocated and realized on read or write operation?
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
  found_this_page:
  }
  return true;
}

void memory_rgn_read(Memory_Region* mem, size_t off, size_t size, void* out){
  // First ensure that those memory region exist
  // TODO:: Assert false or do something else maybe?
  if(!out || !memory_rgn_ensure(mem, off, size)) return;
  // Then iterate over them and try to read somehow
  Memory_Page_Iter iter = memory_rgn_iter_init(off, size);
  while(memory_rgn_iter(&iter)){
    Memory_Page* page = memory_rgn_get_page(mem, iter.page_inx);
    assert(page);
    memcpy(out, page->data+iter.page_off, iter.page_size);
    out += iter.page_size;
  }
}
void memory_rgn_write(Memory_Region* mem, size_t off, size_t size, const void* in){
  // First ensure that those memory region exist
  // TODO:: Assert false or do something else maybe?
  if(!in || !memory_rgn_ensure(mem, off, size)) return;

  // Then iterate over them and try to write somehow
  Memory_Page_Iter iter = memory_rgn_iter_init(off, size);
  while(memory_rgn_iter(&iter)){
    Memory_Page* page = memory_rgn_get_page(mem, iter.page_inx);
    assert(page);
    memcpy(page->data+iter.page_off, in, iter.page_size);
    in += iter.page_size;
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


typedef struct Wasm_Data Wasm_Data;
struct Wasm_Data {
  enum {
    WASM_DATA_I32,
    WASM_DATA_U32,
    WASM_DATA_F32,
    WASM_DATA_U64,
  } tag;
  union{
    s32 di32;
    u32 du32;
    f32 df32;
    u64 du64;
  };
};
const Cstr wasm_names[] = {
  [WASM_DATA_I32] = "i32",
  [WASM_DATA_U32] = "u32",
  [WASM_DATA_F32] = "f32",
  [WASM_DATA_U64] = "u64",
};

void run_sample(Alloc_Interface allocr, Module* mod){
  // Memory leaks are happening here
  u64_Darray stk = init_u64_darray(allocr);
  

#define pushstk(v)					\
  do{							\
    if(!push_u64_darray(&stk, (v))){			\
      fprintf(stderr, "Couldnt push to stack\n");	\
      return;						\
    }							\
  }while(0)

#define popstk(v)							\
  do{									\
    if(stk.count == 0){							\
      fprintf(stderr, "Couldnt pop argument, stack underflow\n");	\
      return;								\
    }									\
    (v) = slice_last(stk);						\
    if(!pop_u64_darray(&stk, 1)){					\
      fprintf(stderr, "Couldnt pop argument, memory error\n");		\
      return;								\
    }									\
  }while(0)
  


  printf_stk(stk, "The current stack is : ");

  // Choose a fxn
  Cstr fxn = "add";
  // List the args
  Wasm_Data args[] = {
    {.tag = WASM_DATA_I32, .di32 = 35},
    {.tag = WASM_DATA_I32, .di32 = 34},
  };
  // Find the entry of the fxn : first find index, then use fxn
  size_t finx = 0;
  for_slice(mod->exports, i){
    const Export expt = slice_inx(mod->exports, i);
    if(str_cstr_cmp(expt.name, fxn) == 0 &&
       expt.export_type == FUNCTION_EXPORT_TYPE){
      finx = i;
      goto found_the_fxn;
    }
  }
  fprintf(stderr, "Couldnt find any function export named %s\n", fxn);
  return;
 found_the_fxn:
  if(finx >= mod->funcs.count) {
    fprintf(stderr, "Couldnt find any data for function %s at index %zu\n", fxn, finx);
    return;
  }
  const Func* func = mod->funcs.data + finx;
  // Validate the parameter count and return type
  if(func->param_idents.count != _countof(args)){
    fprintf(stderr, "Trying to feed %zu args for a function of %zuarity\n",
	    _countof(args), func->param_idents.count);
    return;
  }
  for_slice(func->param_idents, i){
    Str pt = slice_inx(func->param_idents, i);
    if(cstr_str_cmp(wasm_names[args[i].tag], pt) != 0){
      fprintf(stderr, "Expected arg %zu to be of type `%.*s`, found `%s`\n",
	      i, str_print(pt), wasm_names[args[i].tag]);
      return;
    }
  }
  // TODO:: Care about the result ?
  // Go through the opcodes?
  for_slice(func->opcodes, i){
    printf_stk(stk, "At opcode %zu : ", i);
    const Str op = slice_inx(func->opcodes, i);
    if(str_cstr_cmp(op, "local.get") == 0){
      i++; // maybe verify that its not ended yet ??
      u64 inx;
      if(!parse_as_u64(slice_inx(func->opcodes, i), &inx)){
	// TODO:: Also print location of source code
	fprintf(stderr, "Expected %zu-th opcode to be index, found `%.*s`\n",
		i, str_print(slice_inx(func->opcodes, i)));
	return;
      }
      // See if the index exists
      if(inx >= _countof(args)){
	// TODO:: Also print location of source code
	fprintf(stderr, "Tried to access %zu-th arg when only %zu args are present\n",
		inx, _countof(args));
	return;
      }
      // Arguments are validated already
      pushstk(args[inx].du64);
    } else if (str_cstr_cmp(op, "i32.sub") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = p1.di32 - p0.di32;
      pushstk(r.du64);
    } else if (str_cstr_cmp(op, "i32.const") == 0){
      i++; // maybe verify that its not ended yet ??
      s64 v;
      if(!parse_as_s64(slice_inx(func->opcodes, i), &v)){
	// TODO:: Also print location of source code
	fprintf(stderr, "Expected %zu-th opcode to be index, found `%.*s`\n",
		i, str_print(slice_inx(func->opcodes, i)));
	return;
      }
      Wasm_Data p = {0};
      p.di32 = v;
      pushstk(p.du64);
    } else {
      fprintf(stderr, "TODO:: Implement opcode `%.*s`\n", str_print(op));
      return;
    }
  }

  

#undef pushstk
#undef popstk

  (void)resize_u64_darray(&stk, 0);
}

int main(void){
  Alloc_Interface allocr = gen_std_allocator();
  //parse_node_iter_run_demo(allocr);

  Parse_Info parser = init_parse_info(allocr, "hello.wat");

  printf("Printing the file by lines: \n");
  for_slice(parser.line_nums, ln){
    Str line = get_line(parser.file, parser.line_nums, ln);
    printf("%4d: %.*s\n", (int)ln+1, str_print(line));
  }

  u32_Slice poses = SLICE_FROM_ARRAY(u32, ((u32[]){0, 4, 6, 7, 8, 18, 499, 819, 731, 732, 733}));

  // for_slice(poses, i){
  //   Cursor pos = get_line_pos(parser.line_nums, poses.data[i]);
  //   printf("Char at %u is at line %u, offset %u\n",poses.data[i],pos.line+1,pos.offset);
  // }

  if(parse_brackets(&parser)){
    printf("Yes, we could parse %s into a parse tree!!!\n", parser.fname);
  } else {
    printf("Oh no! we could not parse %s\n", parser.fname);
    return 1;
  }

  Module main_module = {0}; // TODO:: itsy bitsy memory leak
  if(!parse_module(allocr, parser.root, &main_module)){
    printf("Couldnt parse the main module!!!\n");
    return 1;
  }
  try_printing_module(&main_module);

  run_memory_page_sample(allocr);

  //run_sample(allocr, &main_module);

  

  free_parse_info(&parser);
  return 0;
}


