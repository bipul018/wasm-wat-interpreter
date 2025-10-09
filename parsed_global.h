// This is intentionally not done, this file is supposed to be included only once
// knowingly as part of some 'unity build' (for now)
// The same reason (also lazy) is why no extra includes are done here
// #pragma once


// Type "global"
typedef struct Global Global;
struct Global {
  Str identifier;
  // Global type can be either `type` or `(mut type)`
  bool is_mut;
  Parse_Node* valtype;
  Parse_Node* expr;
  
  Parse_Node_Ptr_Slice unknowns;
};

DEF_SLICE(Global);
DEF_DARRAY(Global, 1);

bool parse_global(Alloc_Interface allocr, Parse_Node* root, Global* glbl){
  if(!root || !glbl || !root->data.data) return false;
  if(str_cstr_cmp(root->data, "global") != 0) return false;
  glbl->identifier = root->data;

  Parse_Node_Ptr_Darray unknowns = init_Parse_Node_Ptr_darray(allocr);
  Parse_Node_Ptr_Slice children = {
    .data = root->children.data,
    .count = root->children.count,
  };

  // Expect the first element to be the type
  if(children.count == 0){
    fprintf(stderr, "Expected type of global, found none\n");
    goto was_error;
  }
  {
    glbl->is_mut = false;
    Parse_Node* node = slice_first(children);
    if(node->children.count == 1){
      if(cstr_str_cmp("mut", node->data) == 0){
	node = slice_first(node->children);
	glbl->is_mut = true;
      }
    }
    glbl->valtype = node;
  }
  slice_shrink_front(children, 1);

  // Expect the next item to be an expression for the value contained
  //        check if at least one child is there
  if(children.count == 0 ||
     slice_first(children)->children.count < 1){
    fprintf(stderr, "Expected an expression to intialize the global with\n");
    goto was_error;
  }
  glbl->expr = slice_first(children);
  slice_shrink_front(children, 1);
  
  for_slice(children, i){
    Parse_Node* child = children.data[i];
    if(!push_Parse_Node_Ptr_darray(&unknowns, child)){
      fprintf(stderr, "Couldnt allocate memory !!!\n");
      goto was_error;
    }
  }

  // Now collect unknowns
  glbl->unknowns = (Parse_Node_Ptr_Slice){
    .data = unknowns.data,
    .count = unknowns.count,
  };

  return true;
 was_error:
  (void)resize_Parse_Node_Ptr_darray(&unknowns, 0);
  return false;
}

void try_printing_global(const Global* glbl){
  printf("The global identifier: %.*s\n", str_print(glbl->identifier));
  printf("This global variable is %s of type %.*s\n",
	 (glbl->is_mut?"mutable":"immutable"),str_print(glbl->valtype->data));
  printf("Expression for variable : (%.*s %.*s ...)\n", str_print(glbl->expr->data),
	 // At least one child is there, its been ensured
	 str_print(slice_first(glbl->expr->children)->data));
  printf("The global has %zu unknowns: ", glbl->unknowns.count);
  for_slice(glbl->unknowns, i){
    printf("[%zu: %.*s] ", i, str_print(glbl->unknowns.data[i]->data));
  }
  printf("\n");
}
