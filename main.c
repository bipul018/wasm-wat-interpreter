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
Str str_slice(Str str, size_t begin, size_t end){
  if(nullptr == str.data || begin >= end || end > str.count) return (Str){ 0 };
  return (Str){ .data = str.data + begin, .count = end - begin };
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

// Ensures that the node has no children also 
bool parse_as_wasm_index(Parse_Node* node, u32* output){
  if(!node || node->children.count != 0) return false;
  Str input = node->data;
  if(input.count == 0) return false;
  if(input.data[0] != ';') return false;
  input = str_slice(input, 1, input.count);
  char* endptr = input.data;
  long long v = strtoll(endptr, &endptr, 0);
  if(endptr == (void*)input.data || v < 0) return false;
  input = str_slice(input, (u8*)endptr-input.data, input.count);
  if(input.count != 1 || input.data[0] != ';') return false;
  *output = (u32) v;
  return true;
}

DEF_SLICE(Parse_Node_Ptr);

#include "parsed_type.h"

/*
// Func "func" : something used directly inside the module
// Tries to follow the behavior of the module
typedef struct Func Func;
struct Func {
  Str identifier;
  u32 idx; // Preferably used in the parent container to store in a linear array
  // Parse_Node* func_data; // Not yet parsed, but something to be referred
  // There should be no unknowns here
  Parse_Node_Ptr_Slice unknowns;
};
DEF_DARRAY(Func, 1);
DEF_SLICE(Func);
// Parsing a func will require the type array
// At least the func's type will be referred to there for now
bool parse_func(Alloc_Interface allocr, Parse_Node* root, Func* func, Type_Slice types){
  if(!root || !func || !root->data.data) return false;
  if(strncmp(root->data.data, "func", root->data.count) != 0) return false;
  func->identifier = root->data;

  Parse_Node_Ptr_Darray unknowns = init_Parse_Node_Ptr_darray(allocr);
  // First make a duplicate darray
  for_slice(root->children, i){
    Parse_Node* child = root->children.data[i];
    bool res = push_Parse_Node_Ptr_darray(&unknowns, child);
    if(!res) {
      fprintf(stderr, "Couldnt allocate memory!!!\n");
      goto was_error;
    }
  }

  // Now try to parse items from the duplicate array
  u32 idx;
  if(unknowns.count == 0 || // idx must exist
     slice_first(unknowns)->children.count != 0 || // no children in idx node
     !parse_as_wasm_index(slice_first(unknowns)->data, &idx)){
    fprintf(stderr, "Invalid func node");
    goto was_error;
  }
  (void)downsize_Parse_Node_Ptr_darray(&unknowns, 0, 1);
  func->idx = idx;

  // Insert the next parse node as the func data
  if(unknowns.count == 0) {
    fprintf(stderr, "No func data found");
    goto was_error;
  }
  func->func_data = slice_first(unknowns);
  (void)downsize_Parse_Node_Ptr_darray(&unknowns, 0, 1);
  
  // We can just 'take ownership' from a darray to a slice
  func->unknowns = (Parse_Node_Ptr_Slice){
    .data = unknowns.data,p
    .count = unknowns.count,
  };
  // Similar 'transfer of ownership' for other knowns
  return true;
 was_error:
  (void)resize_Parse_Node_Ptr_darray(&unknowns, 0);
  return false;
}
// Only prints the heads of unknown ones
void try_printing_func(const Func* func){
  printf("The func identifier: %.*s\n", str_print(func->identifier));
  printf("The func index is %u and is a `%.*s`.\n",
	 (unsigned)func->idx, str_print(func->func_data->data));
  printf("The func has %zu unknowns: ", func->unknowns.count);
  for_slice(func->unknowns, i){
    printf("[%zu: %.*s] ", i, str_print(func->unknowns.data[i]->data));
  }
  printf("\n");
}
*/

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


