// This is intentionally not done, this file is supposed to be included only once
// knowingly as part of some 'unity build' (for now)
// The same reason (also lazy) is why no extra includes are done here
// #pragma once

// Bidirectional tree structure
typedef struct Parse_Node Parse_Node;
typedef Parse_Node* Parse_Node_Ptr;
DEF_DARRAY(Parse_Node_Ptr, 16);
struct Parse_Node {
  Parse_Node* parent;
  Parse_Node_Ptr_Darray children;
  Str data;
};

// create a node
Parse_Node* parse_node_create(Alloc_Interface allocr, Str data){
  Parse_Node* node = alloc_mem(allocr, sizeof(Parse_Node), alignof(Parse_Node));
  if(nullptr == node) return node;
  *node = (Parse_Node){
    .data = data,
    .children = init_Parse_Node_Ptr_darray(allocr),
  };
  return node;
}

void parse_node_destroy(Parse_Node* node){
  if(!node) return;
  Alloc_Interface allocr = node->children.allocr;
  for_slice(node->children, inx){
    parse_node_destroy(slice_inx(node->children, inx));
  }
  (void)resize_Parse_Node_Ptr_darray(&node->children, 0);
  free_mem(allocr, node);
}

// add child and return it
Parse_Node* parse_node_append_child(Parse_Node* parent, Parse_Node* child){
  assert(parent != nullptr && child != nullptr && child->parent == nullptr);
  child->parent = parent;

  bool res = push_Parse_Node_Ptr_darray(&parent->children, child);
  if(!res) { parse_node_destroy(child); return nullptr; }

  return child;
}
Parse_Node* parse_node_new_child(Alloc_Interface allocr, Parse_Node* parent, Str data){
  assert(parent != nullptr);
  Parse_Node* ch = parse_node_create(allocr, data);
  return parse_node_append_child(parent, ch);
}

// move to parent : Just do ->parent
// move to nth child : Just do slice_inx(->children, inx)
Parse_Node* parse_node_child(Parse_Node* node, size_t inx){
  if(!node || !node->children.data || inx >= node->children.count) return nullptr;
  return node->children.data[inx];
}
// Returns the index of the self wrt the parent, -1 if root node, -2 if error
s32 parse_node_which_child(Parse_Node* node){
  if(!node) return -2;
  Parse_Node* parent = node->parent;
  if(nullptr == parent) return -1;
  for_slice(parent->children, inx){
    if(parent->children.data[inx] == node){
      return inx;
    }
  }
  return -2; // Maybe assert in the future
}
Parse_Node* parse_node_nth_sibling(Parse_Node* node, size_t inx){
  if(!node) return nullptr;
  Parse_Node* parent = node->parent;
  if(!parent || !parent->children.data || inx >= parent->children.count)
    return nullptr;
  return slice_inx(parent->children, inx);
}
// move to nth sibling : Just do slice_inx(->parent->children,inx)
// go to next sibling
Parse_Node* parse_node_next_sibling(Parse_Node* node){
  // If there is no parent or next sibling, it returns nullptr
  assert(nullptr != node);
  s32 inx = parse_node_which_child(node);
  if(inx < 0) return nullptr;
  // Will return nullptr if last sibling anyway
  return parse_node_nth_sibling(node, inx+1);
}
// go to previous sibling
Parse_Node* parse_node_prev_sibling(Parse_Node* node){
  // If there is no parent or previous sibling, it returns nullptr
  assert(nullptr != node);
  s32 inx = parse_node_which_child(node);
  if(inx <= 0) return nullptr; // Case of first sibling is handled here
  return parse_node_nth_sibling(node, inx-1);
}

// An iterator now, is just a pointer to the node in
// Depth first iteration
Parse_Node* parse_node_iter_next_df(Parse_Node* node){
  // If possible, go to first child
  if(node->children.count != 0) {
    return slice_inx(node->children, 0);
  } else {
    // Else, go to next sibling
    while(node != nullptr){
      Parse_Node* ns = parse_node_next_sibling(node);
      if(ns != nullptr) return ns;
      // Else, go to parent's sibling and so on
      node = node->parent;
    }
    // all iterated
  }
  return nullptr;
}

#define for_each_parse_node_df(name, head)	\
  for(Parse_Node* name = (head);		\
      name != nullptr;				\
      name = parse_node_iter_next_df(name))

// Breadth first iteration
// LEFT TODO WHEN INTERESTED OR NEEDED

void parse_node_iter_run_demo(Alloc_Interface allocr){
  // Make a dummy parse tree, and iterate it
  Parse_Node* root = parse_node_create(allocr, (Str){.data = "root"});
  Parse_Node* node = root;
  {
    (void)parse_node_new_child(allocr, node, (Str){.data = "child1"});
    (void)parse_node_new_child(allocr, node, (Str){.data = "child2"});
    (void)parse_node_new_child(allocr, node, (Str){.data = "child3"});
    (void)parse_node_new_child(allocr, node, (Str){.data = "child4"});
    (void)parse_node_new_child(allocr, node, (Str){.data = "child5"});
    {
      Parse_Node* node = root->children.data[1];
      (void)parse_node_new_child(allocr, node, (Str){.data = "child2_1"});
      (void)parse_node_new_child(allocr, node, (Str){.data = "child2_2"});      
    }
    {
      Parse_Node* node = root->children.data[3];
      (void)parse_node_new_child(allocr, node, (Str){.data = "child4_1"});
      (void)parse_node_new_child(allocr, node, (Str){.data = "child4_2"});      
    }
    {
      Parse_Node* node = root->children.data[4];
      (void)parse_node_new_child(allocr, node, (Str){.data = "child5_1"});
      {
	Parse_Node* node = root->children.data[4]->children.data[0];
	(void)parse_node_new_child(allocr, node, (Str){.data = "child5_1_1"});
	{
	  Parse_Node* node = root->children.data[4]->children.data[0]->children.data[0];
	  (void)parse_node_new_child(allocr, node, (Str){.data = "child5_1_1_1"});
	  (void)parse_node_new_child(allocr, node, (Str){.data = "child5_1_1_2"});
	}
      }
    }
    {
      node = root->children.data[0];
      (void)parse_node_new_child(allocr, node, (Str){.data = "child1_1"});
    }
  }
  
  printf("Children in root = %zu\n", root->children.count);
  for_each_parse_node_df(node, root){
    printf("Node %p = %s, children = %zu\n", node, node->data.data, node->children.count);
  }
  parse_node_destroy(root);
}
