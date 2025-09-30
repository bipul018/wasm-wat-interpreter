// This is intentionally not done, this file is supposed to be included only once
// knowingly as part of some 'unity build' (for now)
// The same reason (also lazy) is why no extra includes are done here
// #pragma once

// Type "data"

// Webassembly consists of two kinds of data segments,
//   passive segment: The data segment doesnt refer to any actual memory offset
//     and the program will do 'memory.init' to actually initialize using the
//     data id parsed/obtained for that particular data segment
//   active segment: Each active segment consists of 
//     optional memory index, offset into the memory and data to initialize
//     the memory must be initialized at the module initiation time

typedef struct Data Data;
struct Data {
  Str identifier; // TODO:: Later if decided that this is always same, remove it
  u32 idx;
  Parse_Node_Ptr_Slice unknowns;
};

DEF_DARRAY(Data, 1);
DEF_SLICE(Data);

bool parse_data(Alloc_Interface allocr, Parse_Node* root, Data* dat){
  if(!root || !dat || !root->data.data) return false;
  if(str_cstr_cmp(root->data, "data") != 0) return false;
  dat->identifier = root->data;

  // Get a modifiable slice, they will be resliced to be consumed
  Parse_Node_Ptr_Slice children = {
    .data = root->children.data,
    .count = root->children.count,
  };

  Parse_Node_Ptr_Darray unknowns = init_Parse_Node_Ptr_darray(allocr);

  // The first entry must be a index
  if(children.count == 0 || // idx must exist
     !parse_as_wasm_index(slice_first(children), &dat->idx)){
    fprintf(stderr, "Invalid type node");
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

  dat->unknowns = (Parse_Node_Ptr_Slice){
    .data = unknowns.data,
    .count = unknowns.count,
  };
  return true;

 was_error:
  (void)resize_Parse_Node_Ptr_darray(&unknowns, 0);
  return false;
}
void try_printing_data(const Data* dat){
  printf("The data section identifier: %.*s\n", str_print(dat->identifier));
  printf("The data section index is %u.\n",
	 (unsigned)dat->idx);
  printf("The data has %zu unknowns: ", dat->unknowns.count);
  for_slice(dat->unknowns, i){
    printf("[%zu: %.*s] ", i, str_print(dat->unknowns.data[i]->data));
  }
  printf("\n");
}

