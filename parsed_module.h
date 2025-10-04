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
  Func_Slice funcs; // Something else will have to build the final fxn list
  Export_Slice exports;
  Import_Slice imports; // Something else will have to build the final fxn list
  Data_Slice data_sections;
  // All the unknown children are saved here, which at the beginning, is all of them
  // The data inside these will directly refer to the parser,
  //  So, they will be valid only until then
  Parse_Node_Ptr_Slice unknowns;
};
// I think first of all, you have to parse 'types'
// Because other items, like the 'func' seem to refer to types
// Thus, some other mechanism is needed
// Like, first do some check if compatible, then

int compare_type_idx(const void* a, const void* b, void* idx_offset){
  const uptr off = (uptr)idx_offset;

  const size_t* idx1 = (size_t*)((u8*)a + off);
  const size_t* idx2 = (size_t*)((u8*)b + off);  

  if(*idx1 < *idx2) return -1;
  if(*idx1 == *idx2) return 0; // this should never actually happen
  return 1;
}

// Lets parse the module like we dont care about other ones, so a 'standard' way

bool parse_module(Alloc_Interface allocr, Parse_Node* root, Module* mod){
  if(!root || !mod || !root->data.data) return false;
  if(str_cstr_cmp(root->data, "module") != 0) return false;
  mod->identifier = root->data;

  // Simply copy the entities if you can parse the entity
  // And convert into a slice from the dynamic array

  // Collect the rest into an unknown array

  Parse_Node_Ptr_Darray unknowns      = {.allocr=allocr};
  Type_Darray           types         = {.allocr=allocr};
  Func_Darray           funcs         = {.allocr=allocr};
  Export_Darray         exports       = {.allocr=allocr};
  Import_Darray         imports       = {.allocr=allocr};
  Data_Darray           data_sections = {.allocr=allocr};

  for_slice(root->children, i){
    Parse_Node* child = root->children.data[i];
    Type typ = {0};
    Func func = {0};
    Export expt = {0};
    Import impt = {0};
    Data dat = {0};
    if(parse_type(allocr, child, &typ)){
      if(!push_Type_darray(&types, typ)){
        fprintf(stderr, "Couldnt allocate memory !!!\n");
        goto was_error;
      }
    }
    else if(parse_func(allocr, child, &func)){
      if(!push_Func_darray(&funcs, func)){
        fprintf(stderr, "Couldnt allocate memory !!!\n");
        goto was_error;
      }
    }
    else if(parse_export(allocr, child, &expt)){
      if(!push_Export_darray(&exports, expt)){
        fprintf(stderr, "Couldnt allocate memory !!!\n");
        goto was_error;
      }
    }
    else if(parse_import(allocr, child, &impt)){
      if(!push_Import_darray(&imports, impt)){
        fprintf(stderr, "Couldnt allocate memory !!!\n");
        goto was_error;
      }
    }
    else if(parse_data(allocr, child, &dat)){
      if(!push_Data_darray(&data_sections, dat)){
        fprintf(stderr, "Couldnt allocate memory !!!\n");
        goto was_error;
      }
    }
    else{
      if(!push_Parse_Node_Ptr_darray(&unknowns, child)){
        fprintf(stderr, "Couldnt allocate memory !!!\n");
        goto was_error;
      }
    }
  }

  // Reuse the darrays directly as slices
  mod->types = (Type_Slice){.data = types.data, .count = types.count};
  mod->funcs = (Func_Slice){.data = funcs.data, .count = funcs.count};
  mod->unknowns = (Parse_Node_Ptr_Slice){.data = unknowns.data, .count = unknowns.count};
  mod->exports = (Export_Slice){.data=exports.data, .count=exports.count};
  mod->imports = (Import_Slice){.data=imports.data, .count=imports.count};
  mod->data_sections = (Data_Slice){.data=data_sections.data, .count=data_sections.count};

  return true;
 was_error:
  (void)resize_Import_darray(&imports, 0);
  (void)resize_Export_darray(&exports, 0);
  (void)resize_Parse_Node_Ptr_darray(&unknowns, 0);
  (void)resize_Type_darray(&types, 0);
  (void)resize_Func_darray(&funcs, 0);
  (void)resize_Data_darray(&data_sections, 0);
  return false;
}

// Only prints the heads of unknown ones
void try_printing_module(const Module* mod){
  printf("The module identifier: %.*s\n", str_print(mod->identifier));
  printf("The module has %zu exports: \n", mod->exports.count);
  for_slice(mod->exports, i){
    printf("%zu: ", i);
    try_printing_export(mod->exports.data + i);
  }
  printf("The module has %zu imports: \n", mod->imports.count);
  for_slice(mod->imports, i){
    printf("%zu: ", i);
    try_printing_import(mod->imports.data + i);
  }
  printf("The module has %zu type infos: \n", mod->types.count);
  for_slice(mod->types, i){
    printf("%zu: ", i);
    try_printing_type(mod->types.data + i);
  }
  printf("The module has %zu data sections: \n", mod->data_sections.count);
  for_slice(mod->data_sections, i){
    printf("%zu: ", i);
    try_printing_data(mod->data_sections.data + i);
  }  
  printf("The module has %zu functions: \n", mod->funcs.count);
  for_slice(mod->funcs, i){
    printf("%zu: ", i);
    try_printing_func(mod->funcs.data + i);
  }
  printf("The module has %zu unknowns: ", mod->unknowns.count);
  for_slice(mod->unknowns, i){
    printf("[%zu: %.*s] ", i, str_print(mod->unknowns.data[i]->data));
  }
  printf("\n");
}
