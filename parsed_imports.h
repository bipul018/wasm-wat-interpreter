// This is intentionally not done, this file is supposed to be included only once
// knowingly as part of some 'unity build' (for now)
// The same reason (also lazy) is why no extra includes are done here
// #pragma once


// Type "import"
typedef struct Import Import;
struct Import {
  Str identifier;
  Str mod_name;
  Str self_name;
  enum {
    UNKNOWN_IMPORT_TYPE,
    FUNCTION_IMPORT_TYPE,
    TABLE_IMPORT_TYPE,
    GLOBAL_IMPORT_TYPE,
    MEMORY_IMPORT_TYPE,
  } import_type;
  Str import_type_name; // Maybe this will be optional later
  Parse_Node* import_details; // Polymorphic, so to be used later
  // Unknowns are probably descriptions
  Parse_Node_Ptr_Slice unknowns;
};

DEF_SLICE(Import);
DEF_DARRAY(Import, 1);

bool parse_import(Alloc_Interface allocr, Parse_Node* root, Import* impt){
  if(!root || !impt || !root->data.data) return false;
  if(str_cstr_cmp(root->data, "import") != 0) return false;
  impt->identifier = root->data;

  Parse_Node_Ptr_Darray unknowns = init_Parse_Node_Ptr_darray(allocr);
  Parse_Node_Ptr_Slice children = {
    .data = root->children.data,
    .count = root->children.count,
  };

  // Expect a module name
  if(children.count == 0 ||
     !parse_as_string(slice_first(children), &impt->mod_name)){
    fprintf(stderr, "Expected a string module name, but didnt find any\n");
    goto was_error;
  }
  slice_shrink_front(children, 1);


  // Expect a item name
  if(children.count == 0 ||
     !parse_as_string(slice_first(children), &impt->self_name)){
    fprintf(stderr, "Expected a string name, but didnt find any\n");
    goto was_error;
  }
  slice_shrink_front(children, 1);

  // Expect a node whose name gives the type of import
  if(children.count == 0) {
    fprintf(stderr, "Couldnt find a node with the details of the import\n");
    goto was_error;
  }

  // ?Expect a single child node inside that import for details?
  {
    Parse_Node* dets = slice_first(children);
    if(dets->children.count != 1) {
      // TODO:: Not make this a error later someday
      printfopt("Expected a single child, found %zu\n", dets->children.count);
      goto was_error;
    }
    impt->import_type_name = dets->data;
    if(str_cmp(dets->data, "func") == 0) impt->import_type = FUNCTION_IMPORT_TYPE;
    else if(str_cmp(dets->data, "global") == 0) impt->import_type = GLOBAL_IMPORT_TYPE;
    else if(str_cmp(dets->data, "table") == 0) impt->import_type = TABLE_IMPORT_TYPE;
    else if(str_cmp(dets->data, "memory") == 0) impt->import_type = MEMORY_IMPORT_TYPE;
    else impt->import_type = UNKNOWN_IMPORT_TYPE;

    impt->import_details = dets->children.data[0];

    slice_shrink_front(children, 1);
  }

  // Collect the rest as unknowns
  for_slice(children, i){
    Parse_Node* child = children.data[i];
    if(!push_Parse_Node_Ptr_darray(&unknowns, child)){
      fprintf(stderr, "Couldnt allocate memory !!!\n");
      goto was_error;
    }
  }


  impt->unknowns = (Parse_Node_Ptr_Slice){
    .data = unknowns.data,
    .count = unknowns.count,
  };

  return true;
 was_error:
  (void)resize_Parse_Node_Ptr_darray(&unknowns, 0);
  return false;
}

void try_printing_import(const Import* impt){
  printf("The import identifier: %.*s\n", str_print(impt->identifier));
  printf("The module `%.*s` imports symbol `%.*s` of type `%.*s`(%d)\n",
         str_print(impt->mod_name), str_print(impt->self_name),
         str_print(impt->import_type_name),(int)impt->import_type);

  printf("The import has %zu unknowns: ", impt->unknowns.count);
  for_slice(impt->unknowns, i){
    printf("[%zu: %.*s] ", i, str_print(impt->unknowns.data[i]->data));
  }
  printf("\n");
}
