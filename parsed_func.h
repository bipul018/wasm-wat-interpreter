// This is intentionally not done, this file is supposed to be included only once
// knowingly as part of some 'unity build' (for now)
// The same reason (also lazy) is why no extra includes are done here
// #pragma once

// Func "func" : something used directly inside the module
// Tries to follow the behavior of the module
typedef struct Func Func;
struct Func {
  Str identifier;
  u32 type_idx; // Used to later index into by the parent to do whatever
  // Currently assuming all parameter types are of leaf node types
  // Following three things represent only the types, access by index
  //Str_Slice param_idents; // TODO:: intern all these identifiers someday
  Str result_iden; // TODO:: also intern this
  Str_Slice local_vars; // ?Maybe this also consists of the parameters?
  size_t param_count; // The first this many items of local_vars are actually parameters

  // There should be no unknowns here (I think)
  Str_Slice opcodes;
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

  Str_Darray local_vars = {.allocr=allocr};
  Str_Darray opcodes = init_Str_darray(allocr);
  Parse_Node_Ptr_Darray unknowns = init_Parse_Node_Ptr_darray(allocr);

  // Get a modifiable slice, they will be resliced to be consumed
  Parse_Node_Ptr_Slice children = {
    .data = root->children.data,
    .count = root->children.count,
  };

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
    for_slice(param_node->children, i){
      Parse_Node* ch = slice_inx(param_node->children, i);
      if(ch->children.count != 0){
	fprintf(stderr, "Expected leaf node as param, found %zu grandchild of %.*s\n",
		ch->children.count, str_print(ch->data));
	goto was_error;
      }
      if(!push_Str_darray(&local_vars, ch->data)){
	fprintf(stderr, "Could not allocate!!!\n");
	goto was_error;
      }
    }
    slice_shrink_front(children, 1);
  }
  func->param_count = local_vars.count;

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

  // Parse the local vars, they are also optional
  if(children.count != 0 &&
     str_cstr_cmp(slice_first(children)->data, "local") == 0){
    Parse_Node* param_node = slice_first(children);
    for_slice(param_node->children, i){
      Parse_Node* ch = slice_inx(param_node->children, i);
      if(ch->children.count != 0){
	fprintf(stderr, "Expected leaf node as param, found %zu grandchild of %.*s\n",
		ch->children.count, str_print(ch->data));
	goto was_error;
      }
      if(!push_Str_darray(&local_vars, ch->data)){
	fprintf(stderr, "Could not allocate!!!\n");
	goto was_error;
      }
    }
    slice_shrink_front(children, 1);
  }

  // As long as there are leaves, record them
  bool normal = true;
  for_slice(children, i){
    Parse_Node* child = children.data[i];
    if(normal){
      if(child->children.count != 0){
	normal = false;
      } else {
	if(!push_Str_darray(&opcodes, child->data)){
	  fprintf(stderr, "Couldnt allocate memory !!!\n");
	  goto was_error;
	}
      }
    }
    if(!normal){
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
  func->opcodes = (Str_Slice){
    .data = opcodes.data,
    .count = opcodes.count
  };
  func->local_vars = (Str_Slice){
    .data = local_vars.data,
    .count = local_vars.count,
  };
  // Similar 'transfer of ownership' for other knowns
  return true;
 was_error:
  (void)resize_Str_darray(&local_vars, 0);
  (void)resize_Str_darray(&opcodes, 0);
  (void)resize_Parse_Node_Ptr_darray(&unknowns, 0);
  if(local_vars.data) SLICE_FREE(allocr, local_vars);
  return false;
}
// Only prints the heads of unknown ones
void try_printing_func(const Func* func){
  printf("The func identifier: %.*s\n", str_print(func->identifier));
  printf("The func is of type indexed %u\n", (unsigned)func->type_idx);
  // Printing the parameters and return types and local vars in a pretty from
  printf("( ");
  for_slice(func->local_vars, i){
    if(i >= func->param_count) break;
    printf("%.*s ", str_print(func->local_vars.data[i]));
  }
  printf(" ) -> %.*s { ", str_print(func->result_iden));
  for_slice(func->local_vars, i){
    if(i < func->param_count) continue;
    printf("%.*s ", str_print(func->local_vars.data[i]));
  }
  printf(" }\n");

  
  printf("The function consists of %zu opcodes\n", func->opcodes.count);
  printf("The func has %zu unknowns: ", func->unknowns.count);
  for_slice(func->unknowns, i){
    printf("[%zu: %.*s] ", i, str_print(func->unknowns.data[i]->data));
  }
  printf("\n");
}
