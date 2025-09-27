// This is intentionally not done, this file is supposed to be included only once
// knowingly as part of some 'unity build' (for now)
// The same reason (also lazy) is why no extra includes are done here
// #pragma once

// Type "type" : something used directly inside the module
// Tries to follow the behavior of the module
typedef struct Type Type;
struct Type {
  Str identifier;
  u32 idx; // Preferably used in the parent container to store in a linear array
  Parse_Node* type_data; // Not yet parsed, but something to be referred
  // There should be no unknowns here
  Parse_Node_Ptr_Slice unknowns;
};
DEF_DARRAY(Type, 1);
DEF_SLICE(Type);
bool parse_type(Alloc_Interface allocr, Parse_Node* root, Type* typ){
  if(!root || !typ || !root->data.data) return false;
  if(strncmp(root->data.data, "type", root->data.count) != 0) return false;
  typ->identifier = root->data;

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
    fprintf(stderr, "Invalid type node");
    goto was_error;
  }
  (void)downsize_Parse_Node_Ptr_darray(&unknowns, 0, 1);
  typ->idx = idx;

  // Insert the next parse node as the type data
  if(unknowns.count == 0) {
    fprintf(stderr, "No type data found");
    goto was_error;
  }
  typ->type_data = slice_first(unknowns);
  (void)downsize_Parse_Node_Ptr_darray(&unknowns, 0, 1);
  
  // We can just 'take ownership' from a darray to a slice
  typ->unknowns = (Parse_Node_Ptr_Slice){
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
void try_printing_type(const Type* typ){
  printf("The type identifier: %.*s\n", str_print(typ->identifier));
  printf("The type index is %u and is a `%.*s`.\n",
	 (unsigned)typ->idx, str_print(typ->type_data->data));
  printf("The type has %zu unknowns: ", typ->unknowns.count);
  for_slice(typ->unknowns, i){
    printf("[%zu: %.*s] ", i, str_print(typ->unknowns.data[i]->data));
  }
  printf("\n");
}
