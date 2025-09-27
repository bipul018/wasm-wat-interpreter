// This is intentionally not done, this file is supposed to be included only once
// knowingly as part of some 'unity build' (for now)
// The same reason (also lazy) is why no extra includes are done here
// #pragma once

// Type "module"
// Following is the union of the module
typedef struct Module Module;
struct Module {
  Str identifier;
  Type_Slice types;
  // All the unknown children are saved here, which at the beginning, is all of them
  // The data inside these will directly refer to the parser,
  //  So, they will be valid only until then
  Parse_Node_Ptr_Slice unknowns;
};
// I think first of all, you have to parse 'types'
// Because other items, like the 'func' seem to refer to types
// Thus, some other mechanism is needed
// Like, first do some check if compatible, then

int compare_type_idx(const void* a, const void* b){
  const Type* t1 = a;
  const Type* t2 = b;
  if(t1->idx < t2->idx) return -1;
  if(t1->idx == t2->idx) return 0; // this should never actually happen
  return 1;
}

bool parse_module(Alloc_Interface allocr, Parse_Node* root, Module* mod){
  if(!root || !mod || !root->data.data) return false;
  if(strncmp(root->data.data, "module", root->data.count) != 0) return false;
  mod->identifier = root->data;


  Parse_Node_Ptr_Darray unknowns = init_Parse_Node_Ptr_darray(allocr);
  Type_Darray types = init_Type_darray(allocr);

  // First make a duplicate darray
  for_slice(root->children, i){
    Parse_Node* child = root->children.data[i];
    bool res = push_Parse_Node_Ptr_darray(&unknowns, child);
    if(!res) {
      fprintf(stderr, "Couldnt allocate memory!!!\n");
      goto was_error;
    }
  }

  // Now, parse types, if any entry matches, remove it from the 'unknowns'
  for_slice(unknowns, i){
    Parse_Node* child = unknowns.data[i];
    Type typ = {0};
    if(parse_type(allocr, child, &typ)){
      if(!push_Type_darray(&types, typ)){
	fprintf(stderr, "Couldnt allocate memory!!!\n");
	goto was_error;
      }
      (void)downsize_Parse_Node_Ptr_darray(&unknowns, i, 1);
      i--;
    }
  }
  // Sort to ensure all the 'idx' match the types
  qsort(types.data, types.count, sizeof(Type), compare_type_idx);

  // We can just 'take ownership' from a darray to a slice
  mod->unknowns = (Parse_Node_Ptr_Slice){
    .data = unknowns.data,
    .count = unknowns.count,
  };
  // Similar 'transfer of ownership' for other knowns
  mod->types = (Type_Slice){
    .data = types.data,
    .count = types.count
  };
  return true;
 was_error:
  (void)resize_Parse_Node_Ptr_darray(&unknowns, 0);
  (void)resize_Type_darray(&types, 0);
  return false;
}
// Only prints the heads of unknown ones
void try_printing_module(const Module* mod){
  printf("The module identifier: %.*s\n", str_print(mod->identifier));
  printf("The module has %zu type infos: \n", mod->types.count);
  for_slice(mod->types, i){
    printf("%zu: ", i);
    try_printing_type(mod->types.data + i);
  }
  printf("The module has %zu unknowns: ", mod->unknowns.count);
  for_slice(mod->unknowns, i){
    printf("[%zu: %.*s] ", i, str_print(mod->unknowns.data[i]->data));
  }
  printf("\n");
}
