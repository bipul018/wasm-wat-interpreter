#include <stddef.h>
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
Str cstr_to_str(Cstr str){
  return (Str){.data = (void*)str, .count = strlen(str)};
}
bool match_str_prefix(Str haystack, Str needle){
  return str_str_cmp(needle, str_slice(haystack, 0, needle.count)) == 0;
}
bool match_str_suffix(Str haystack, Str needle){
  if(haystack.count<needle.count) return false;
  return str_str_cmp(needle, str_slice(haystack, haystack.count-needle.count, haystack.count)) == 0;
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
#include "parsed_data.h"
#include "parsed_module.h"

// Try to run a given function
// Assume all the functions are ordered correctly ?
#include "stk_n_mems.h"

#include "runner.h"

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

  //for_each_parse_node_df(node, parser.root){
  //  printf("Node %p = %.*s, children = %zu\n", node, str_print(node->data), node->children.count);
  //}

  Module main_module = {0}; // TODO:: itsy bitsy memory leak
  if(!parse_module(allocr, parser.root, &main_module)){
    printf("Couldnt parse the main module!!!\n");
    return 1;
  }
  try_printing_module(&main_module);

  //run_memory_page_sample(allocr);

  run_sample(allocr, &main_module);

  

  free_parse_info(&parser);
  return 0;
}


