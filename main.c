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
  if(strncmp(typ_idx->data.data, "type", typ_idx->data.count) != 0) return false;
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


// Func "func" : something used directly inside the module
// Tries to follow the behavior of the module
typedef struct Func Func;
struct Func {
  Str identifier;
  u32 idx; // Preferably used in the parent container to store in a linear array
  u32 type_idx; // Used to later index into by the parent to do whatever
  // Parse_Node* func_data; // Not yet parsed, but something to be referred
  // There should be no unknowns here
  Parse_Node_Ptr_Slice unknowns;
};
DEF_DARRAY(Func, 1);
DEF_SLICE(Func);
// Parsing a func will require the type array
// At least the func's type will be referred to there for now
bool parse_func(Alloc_Interface allocr, Parse_Node* root, Func* func){
  if(!root || !func || !root->data.data) return false;
  if(strncmp(root->data.data, "func", root->data.count) != 0) return false;
  func->identifier = root->data;


  Parse_Node_Ptr_Darray unknowns = init_Parse_Node_Ptr_darray(allocr);

  // Get a modifiable slice, they will be resliced to be consumed
  Parse_Node_Ptr_Slice children = {
    .data = root->children.data,
    .count = root->children.count,
  };

  // The first entry must be own index
  if(children.count == 0 || // idx must exist
     !parse_as_wasm_index(slice_first(children), &func->idx)){
    fprintf(stderr, "No function index found");
    goto was_error;
  }
  slice_shrink_front(children, 1);

  // Now the type index must be found
  if(children.count == 0 ||
     !parse_as_type_index(slice_first(children), &func->type_idx)) {
    fprintf(stderr, "No type index found\n");
    goto was_error;
  }
  slice_shrink_front(children, 1);

  for_slice(children, i){
    Parse_Node* child = children.data[i];
    {
      if(!push_Parse_Node_Ptr_darray(&unknowns, child)){
	fprintf(stderr, "Couldnt allocate memory !!!\n");
	goto was_error;
      }
    }
  }
  
  // We can just 'take ownership' from a darray to a slice
  func->unknowns = (Parse_Node_Ptr_Slice){
    .data = unknowns.data,
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
  printf("The func[%u] is of type indexed %u\n",
	 (unsigned)func->idx, (unsigned)func->type_idx);
  printf("The func has %zu unknowns: ", func->unknowns.count);
  for_slice(func->unknowns, i){
    printf("[%zu: %.*s] ", i, str_print(func->unknowns.data[i]->data));
  }
  printf("\n");
}


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


