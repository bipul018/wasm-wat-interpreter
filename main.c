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

// Actual 'parsing'


// TODO:: In the following helper functions, maybe add more error reporting
// Ensures that the node has no children also 
bool parse_as_wasm_index(Parse_Node* node, u32* output){
  if(!node || node->children.count != 0) return false;
  Str input = node->data;
  if(input.count == 0) return false;
  if(input.data[0] != ';') return false;
  input = str_slice(input, 1, input.count);
  char* endptr = input.data;
  // TODO:: Is this safe?
  long long v = strtoll(endptr, &endptr, 0);
  if(endptr == (void*)input.data || v < 0) return false;
  input = str_slice(input, (u8*)endptr-input.data, input.count);
  if(input.count != 1 || input.data[0] != ';') return false;
  *output = (u32) v;
  return true;
}

bool parse_as_type_index(Parse_Node* typ_idx, u32* out_idx){
  // There must be exactly 1 child
  if(!typ_idx || typ_idx->children.count != 1) return false;
  if(str_cstr_cmp(typ_idx->data, "type") != 0) return false;
  // Verify that the node is with one leaf 
  Parse_Node* child = typ_idx->children.data[0];
  if(!child || child->children.count != 0) return false;
  Str num = child->data;
  if(num.count == 0) return false;
  char* endptr = num.data;
  // TODO:: Is this safe?
  long long v = strtoll(endptr, &endptr, 0);
  num = str_slice(num, (u8*)endptr-num.data, num.count);
  // There should only be the number in the data field, this means, `num` is depleted
  if(num.data || v < 0) return false;
  *out_idx = (u32) v;
  return true;
}


DEF_SLICE(Parse_Node_Ptr);

#include "parsed_type.h"
#include "parsed_func.h"

#include "parsed_module.h"

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

  Module main_module = {0};
  if(!parse_module(allocr, parser.root, &main_module)){
    printf("Couldnt parse the main module!!!\n");
    return 1;
  }
  try_printing_module(&main_module);
  
  free_parse_info(&parser);
  return 0;
}


