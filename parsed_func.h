// This is intentionally not done, this file is supposed to be included only once
// knowingly as part of some 'unity build' (for now)
// The same reason (also lazy) is why no extra includes are done here
// #pragma once

// Func "func" : something used directly inside the module
// Tries to follow the behavior of the module
typedef struct Func Func;
struct Func {
  Str identifier;
  u32 idx; // Preferably used in the parent container to store in a linear array
  u32 type_idx; // Used to later index into by the parent to do whatever
  // Currently assuming all parameter types are of leaf node types
  Str_Slice param_idents; // TODO:: intern all these identifiers someday
  Str result_iden; // TODO:: also intern this
  
  // There should be no unknowns here (I think)
  Parse_Node_Ptr_Slice unknowns;
};
DEF_DARRAY(Func, 1);
DEF_SLICE(Func);
// Parsing a func will require the type array
// At least the func's type will be referred to there for now
bool parse_func(Alloc_Interface allocr, Parse_Node* root, Func* func){
  if(!root || !func || !root->data.data) return false;
  if(str_cstr_cmp(root->data, "func") != 0) return false;
  func->identifier = root->data;


  Parse_Node_Ptr_Darray unknowns = init_Parse_Node_Ptr_darray(allocr);
  Str_Slice params = {0};

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

  // Parameter list is optional
  
  // TODO:: Later remove this parameter and result parsing so that
  //        type definition can also reuse the logic for parsing
  // Parse the parameter list
  if(children.count != 0 &&
     str_cstr_cmp(slice_first(children)->data, "param") == 0){
    Parse_Node* param_node = slice_first(children);
    params = SLICE_ALLOC(allocr, Str, param_node->children.count);
    if(!params.data){
      fprintf(stderr, "Could not allocate\n");
      goto was_error;
    }
    for_slice(param_node->children, i){
      Parse_Node* ch = slice_inx(param_node->children, i);
      if(ch->children.count != 0){
	fprintf(stderr, "Expected leaf node as param, found %zu grandchild of %.*s\n",
		ch->children.count, str_print(ch->data));
	goto was_error;
      }
      slice_inx(params, i) = ch->data;
    }
    slice_shrink_front(children, 1);
  }

  // Now parse the return type
  if(children.count != 0 &&
     str_cstr_cmp(slice_first(children)->data, "result") == 0){
    // TODO:: find if return type can have multiple values
    Parse_Node* param_node = slice_first(children);
    if(param_node->children.count != 1){
      fprintf(stderr, "Expected exactly 1 child in result node, found %zu\n",
	      param_node->children.count);
      goto was_error;
    }
    Parse_Node* leaf = slice_inx(param_node->children,0);
    if(leaf->children.count != 0){
      fprintf(stderr, "Expected a leaf node as result type, found %zu grandkids\n",
	      leaf->children.count);
      goto was_error;
    }
    func->result_iden = leaf->data;
    slice_shrink_front(children, 1);
  }

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
  func->param_idents = params;
  // Similar 'transfer of ownership' for other knowns
  return true;
 was_error:
  (void)resize_Parse_Node_Ptr_darray(&unknowns, 0);
  if(params.data) SLICE_FREE(allocr, params);
  return false;
}
// Only prints the heads of unknown ones
void try_printing_func(const Func* func){
  printf("The func identifier: %.*s\n", str_print(func->identifier));
  printf("The func[%u] is of type indexed %u\n",
	 (unsigned)func->idx, (unsigned)func->type_idx);
  printf("The func takes %zu parameters of types : ", func->param_idents.count);
  for_slice(func->param_idents, i){
    printf("%.*s ", str_print(func->param_idents.data[i]));
  }
  printf("And returns %.*s\n", str_print(func->result_iden));
  printf("The func has %zu unknowns: ", func->unknowns.count);
  for_slice(func->unknowns, i){
    printf("[%zu: %.*s] ", i, str_print(func->unknowns.data[i]->data));
  }
  printf("\n");
}
