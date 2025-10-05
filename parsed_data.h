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
  Parse_Node* offset_expr; // TODO:: Only supports active data sections for now
  Str raw_bytes; // TODO:: Left to parse the data for backslashes
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
  Str_Builder raw_bytes = {.allocr=allocr};

  // For now, expect a node with 1 child for offset 
  if(children.count == 0 ||
     slice_first(children)->children.count != 1){
     //slice_first(children).children.data[0].children.count != 0){
    fprintf(stderr, "Expected an offset type containing some expression\n");
    goto was_error;
  }
  dat->offset_expr = slice_first(children);
  slice_shrink_front(children, 1);

  // For now, if more children exist and its of vaild string type,
  //   treat it as data (no multiple strings for now)
  if(children.count != 0 &&
     slice_first(children)->children.count == 0){
    // TODO:: Turn this into a separate utility function
    Str escaped_bytes = {0};
    if(parse_as_string(slice_first(children), &escaped_bytes)){
      for_slice(escaped_bytes, i){
	u8 out;
	if(slice_inx(escaped_bytes, i) == '\\'){
	  i++;
	  if(i >= escaped_bytes.count) {
	    (void)resize_u8_darray(&raw_bytes, 0);
	    goto done_with_string_literal;
	  }
	  u8 in = slice_inx(escaped_bytes, i);
	  if(in == '"') out = '"';
	  else if(in == '\'') out = '\''; 
	  else if(in == '\\') out = '\\';
	  else if(in == 'n') out = '\n';
	  else if(in == 'r') out = '\r';
	  else if(in == 't') out = '\t';
	  else if(isxdigit(in) &&
		  (i+1)<escaped_bytes.count &&
		  isxdigit(slice_inx(escaped_bytes, i+1))) {
	    // TODO:: Support capital hex chars also later
	    u32 v1 = isdigit(in)?(in - '0'):(in-'a');
	    i++;
	    u8 in2 = slice_inx(escaped_bytes, i);
	    u32 v2 = isdigit(in2)?(in2-'0'):(in2-'a');

	    out = (v1 << 4) | v2;
	  }
	  // TODO:: Not implemented \u{hexbytes} yet
	  else {
	    (void)resize_u8_darray(&raw_bytes, 0);
	    goto done_with_string_literal;	    
	  }
	} else { out = slice_inx(escaped_bytes, i); }
	if(!push_u8_darray(&raw_bytes, out)){
	  fprintf(stderr, "Couldnt allocate memory!!!\n");
	  goto was_error;
        }
      }
      slice_shrink_front(children, 1);
    }
  }
 done_with_string_literal:


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
  dat->raw_bytes = (Str){
    .data = raw_bytes.data,
    .count = raw_bytes.count,
  };
  return true;

 was_error:
  (void)resize_u8_darray(&raw_bytes, 0);
  (void)resize_Parse_Node_Ptr_darray(&unknowns, 0);
  return false;
}
void try_printing_data(const Data* dat){
  printf("The data section identifier: %.*s\n", str_print(dat->identifier));
  printf("The data section is of active type with the offset type being %.*s\n",
          str_print(dat->offset_expr->data));
  printf("The data to initialize is of %zu length\n", dat->raw_bytes.count);
  printf("The data bytes are: ");
  printf("[ ");							
  for_slice(dat->raw_bytes, i){
    if(isgraph(dat->raw_bytes.data[i])) printf("%c ", dat->raw_bytes.data[i]);
    else printf("%02x ", dat->raw_bytes.data[i]);
  }
  printf(" ]\n");						        

  printf("The data has %zu unknowns: ", dat->unknowns.count);
  for_slice(dat->unknowns, i){
    printf("[%zu: %.*s] ", i, str_print(dat->unknowns.data[i]->data));
  }
  printf("\n");
}

