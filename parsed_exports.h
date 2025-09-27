// This is intentionally not done, this file is supposed to be included only once
// knowingly as part of some 'unity build' (for now)
// The same reason (also lazy) is why no extra includes are done here
// #pragma once


// Type "export"
typedef struct Export Export;
struct Export {
  Str identifier;
  Str name;
  enum {
    MEMORY_EXPORT_TYPE,
    FUNCTION_EXPORT_TYPE,
    UNKNOWN_EXPORT_TYPE,
  } export_type;
  Str export_type_name; // Maybe this will be optional later
  u32 export_idx;
  // Unknowns are probably descriptions
  Parse_Node_Ptr_Slice unknowns;
};

DEF_SLICE(Export);
DEF_DARRAY(Export, 1);

bool parse_export(Alloc_Interface allocr, Parse_Node* root, Export* exprt){
  if(!root || !exprt || !root->data.data) return false;
  if(str_cstr_cmp(root->data, "export") != 0) return false;
  exprt->identifier = root->data;

  Parse_Node_Ptr_Darray unknowns = init_Parse_Node_Ptr_darray(allocr);
  Parse_Node_Ptr_Slice children = {
    .data = root->children.data,
    .count = root->children.count,
  };

  // First expect the name as string literal (ignore escaping)
  if(children.count == 0 ||
     !parse_as_string(slice_first(children), &exprt->name)){
    fprintf(stderr, "Expected a string name, but didnt find any\n");
    goto was_error;
  }
  slice_shrink_front(children, 1);

  // Then expect a child with (<type> index)
  if(children.count == 0) {
    fprintf(stderr, "Couldnt a node with the location of export\n");
    goto was_error;
  }

  {
    Parse_Node* loc = slice_first(children);
    if(loc->children.count != 1) {
      fprintf(stderr, "Expected a single children, found %zu\n", loc->children.count);
      goto was_error;
    }
    Parse_Node* idx = slice_first(loc->children);
    char* endptr = idx->data.data;
    // TODO:: Is this safe?
    long long v = strtoll(endptr, &endptr, 0);
    if((void*)endptr != (idx->data.data + idx->data.count) || v < 0){
      fprintf(stderr, "Expected unsigned integer index, found `%.*s`\n", str_print(idx->data));
      goto was_error;
    }
    exprt->export_idx = v;

    exprt->export_type_name = loc->data;
    if(str_cstr_cmp(loc->data, "memory") == 0) exprt->export_type = MEMORY_EXPORT_TYPE;
    else if(str_cstr_cmp(loc->data, "func") == 0) exprt->export_type = FUNCTION_EXPORT_TYPE;
    else exprt->export_type = UNKNOWN_EXPORT_TYPE;

    slice_shrink_front(children, 1);
  }
  for_slice(children, i){
    Parse_Node* child = children.data[i];
    if(!push_Parse_Node_Ptr_darray(&unknowns, child)){
      fprintf(stderr, "Couldnt allocate memory !!!\n");
      goto was_error;
    }
  }

  // Now collect unknowns
  exprt->unknowns = (Parse_Node_Ptr_Slice){
    .data = unknowns.data,
    .count = unknowns.count,
  };

  return true;
 was_error:
  (void)resize_Parse_Node_Ptr_darray(&unknowns, 0);
  return false;
}

void try_printing_export(const Export* expt){
  printf("The export identifier: %.*s\n", str_print(expt->identifier));
  printf("The symbol `%.*s` exports item of type `%.*s`(%d) with index %lu\n",
	 str_print(expt->name), str_print(expt->export_type_name),
	 (int)expt->export_type, (unsigned long)expt->export_idx);

  printf("The export has %zu unknowns: ", expt->unknowns.count);
  for_slice(expt->unknowns, i){
    printf("[%zu: %.*s] ", i, str_print(expt->unknowns.data[i]->data));
  }
  printf("\n");
}
