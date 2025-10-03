// This is intentionally not done, this file is supposed to be included only once
// knowingly as part of some 'unity build' (for now)
// The same reason (also lazy) is why no extra includes are done here
// #pragma once

typedef struct Parse_Info Parse_Info;
struct Parse_Info {
  Alloc_Interface allocr;
  Cstr fname; //Duplicated using allocr
  Str file;
  u32_Slice line_nums;
  Cursor curr_pos; // TODO:: use this
  Parse_Node* root;
};
Parse_Info init_parse_info(Alloc_Interface allocr, Cstr filename){
  Parse_Info parser = {.allocr=allocr};
  {
    char* fname = alloc_mem(parser.allocr, strlen(filename)+1, 1);
    assert(nullptr != fname);
    strcpy(fname, filename);
    parser.fname = fname;
  }
  parser.file = read_entire_file(parser.allocr, parser.fname);
  printf("After reading file %s, found size : %zu\n",
	 parser.fname, parser.file.count);
  parser.line_nums = parse_lines(parser.allocr, parser.file);
  printf("Number of lines in the file %s is %zu",
	 parser.fname, parser.line_nums.count);
  return parser;
}
void free_parse_info(Parse_Info* parser){
  SLICE_FREE(parser->allocr, parser->line_nums);
  SLICE_FREE(parser->allocr, parser->file);
  free_mem(parser->allocr, (void*)parser->fname);
  *parser = (Parse_Info){0};
}
Str skip_whitespace(Str input){
  size_t inx = 0;
  for(; slice_inx(input,inx) && isspace(slice_inx(input, inx)); inx++);
  return str_slice(input, inx, input.count);
}
bool parse_identifier_or_str(Str* curr_file,Str* out_data){
  size_t node_end = 1;
  u8 ch = slice_first(*curr_file);
  if(ch == '"'){
    // If string, also blindingly collect till closing
    while(slice_inx(*curr_file, node_end) &&
	  slice_inx(*curr_file, node_end) != '"') node_end++;
    // String must have closed properly
    // TODO:: Later report error here with line number
    if(slice_inx(*curr_file, node_end) != '"') return false;
    node_end++; // Also include the end quotes
  } else {
    // Else, blindingly collect till whitespace
    // except for brackets
    // Brackets can occur here anywhere, need to not include them in such case
    while(slice_inx(*curr_file, node_end) &&
	  slice_inx(*curr_file, node_end) != '(' &&
	  slice_inx(*curr_file, node_end) != ')' &&
	  !isspace(slice_inx(*curr_file, node_end))) node_end++;
  }
  *out_data = str_slice(*curr_file, 0, node_end);
  *curr_file = str_slice(*curr_file, node_end, curr_file->count);
  return true;
}
Str skip_comment(Str input){
  // Considering that 'input' is skipped whitespace and valid atoms/identifiers
  if(match_str_prefix(input, cstr_to_str(";;"))){
    size_t inx = 0;
    for_slice(input, i){
      inx = i;
      if(slice_inx(input, i) == '\n') break;
    }
    // Here is where you test if multiline commenting by escaping newline is done
    if(slice_inx(input, inx) != '\n'){
      // The input has exhausted
      return str_slice(input, inx, input.count);
    }
    return str_slice(input, inx+1, input.count);
  }
  return input;
}
Str skip_ws_and_comment(Str input){
  return skip_whitespace(skip_comment(skip_whitespace(input)));
}
Parse_Node* parse_brackets_(Parse_Info* parser, Str* curr_file){
  // Expect a bracket open, else return false

#define dump_error_and_ret()			\
  do{									\
    size_t off = curr_file->data - parser->file.data;			\
    Cursor pos = get_line_pos(parser->line_nums ,off);			\
    Str line = get_line(parser->file, parser->line_nums, pos.line);	\
    printf("Error during parsing at file %s, line %d, offset %d:\n"	\
	   "%.*s\n%*c~~~~~~\n",						\
	   parser->fname, pos.line+1, pos.offset, str_print(line),	\
	   pos.offset, '^');						\
    return nullptr;							\
  }while(0)

  *curr_file = skip_ws_and_comment(*curr_file);
  if(slice_first(*curr_file) != '(') dump_error_and_ret();
  *curr_file = str_slice(*curr_file, 1, curr_file->count);

  // First expect an identifer(data)
  Str head_data = {0};
  *curr_file = skip_ws_and_comment(*curr_file);
  if(!parse_identifier_or_str(curr_file, &head_data)) dump_error_and_ret();
  // TODO:: Report error or assert before returning

  // Now create the node
  Parse_Node* head = parse_node_create(parser->allocr, head_data);
  if(!head) dump_error_and_ret();

  // Now in a loop expect nodes until closing bracket outside of string
  // Keep appending all the nodes to the 'node'
  while(true){
    *curr_file = skip_ws_and_comment(*curr_file);
    // Exit when bracket end
    const u8 ch = slice_first(*curr_file);
    if(ch == 0) goto error_happened;
    if(ch == ')'){
      *curr_file = str_slice(*curr_file, 1, curr_file->count);
      break;
    }
    // If bracket open, recursively call
    if(ch == '('){
      Parse_Node* child = parse_brackets_(parser, curr_file);
      if(!child || !parse_node_append_child(head, child)) goto error_happened;
      // TODO:: Report error or assert before returning
    } else {
      // Parse an idetifier or a string literal now
      Str node_data = {0};
      if(!parse_identifier_or_str(curr_file, &node_data)) goto error_happened;
      // TODO:: Report error or assert
      if(!parse_node_new_child(parser->allocr, head, node_data)){
	goto error_happened; // TODO:: Report error or assert
      }
    }
  }
  return head;
 error_happened:
  parse_node_destroy(head);
  dump_error_and_ret();
}

// Try to generate a parse tree out of string
bool parse_brackets(Parse_Info* parser){
  // Will parse all brackets except inside quote pairs ?
  // Need to implement escaping ?
  bool str_escaping = false;
  assert(nullptr == parser->root);
  
  Str temp_file = parser->file;
  Parse_Node* node = parse_brackets_(parser, &temp_file);
  if(!node) return false;
  parser->root = node;

  return true;
}

