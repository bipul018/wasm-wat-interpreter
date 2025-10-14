// This is intentionally not done, this file is supposed to be included only once
// knowingly as part of some 'unity build' (for now)
// The same reason (also lazy) is why no extra includes are done here
// #pragma once

// Actual 'parsing'

// TODO:: In the following helper functions, maybe add more error reporting
bool parse_as_type_index(Parse_Node* typ_idx, u32* out_idx){
  // There must be exactly 1 child
  if(!typ_idx || typ_idx->children.count != 1) return false;
  if(str_cstr_cmp(typ_idx->data, "type") != 0) return false;
  // Verify that the node is with one leaf 
  Parse_Node* child = typ_idx->children.data[0];
  if(!child || child->children.count != 0) return false;
  Str num = child->data;
  if(num.count == 0) return false;
  char* endptr = num.data;
  // TODO:: Is this safe?
  long long v = strtoll(endptr, &endptr, 0);
  num = str_slice(num, (u8*)endptr-num.data, num.count);
  // There should only be the number in the data field, this means, `num` is depleted
  if(num.data || v < 0) return false;
  *out_idx = (u32) v;
  return true;
}

// Does not do escaping
bool parse_as_string(Parse_Node* str_node, Str* out_str){
  // There must be no child
  if(!out_str || str_node->children.count != 0) return false;
  if(str_node->data.count <= 1) return false;
  if(slice_first(str_node->data) != '"') return false;
  if(slice_last(str_node->data) != '"') return false;
  *out_str = str_slice(str_node->data, 1, str_node->data.count-1);
  return true;
}

// Parse as u64 integer
bool parse_as_u64(Str str, u64* out){
  char* endptr = str.data;
  unsigned long long v = strtoull(endptr, &endptr, 0);
  if((void*)endptr != (str.data + str.count)){
    //fprintf(stderr, "Expected unsigned integer index, found `%.*s`\n",
    // str_print(str));
    return false;
  }
  *out = v;
  return true;
}
// Parse as s64 integer
bool parse_as_s64(Str str, s64* out){
  char* endptr = str.data;
  long long v = strtoll(endptr, &endptr, 0);
  if((void*)endptr != (str.data + str.count)){
    //fprintf(stderr, "Expected signed integer, found `%.*s`\n",
    // str_print(str));
    return false;
  }
  *out = v;
  return true;
}
// Parse as f64 integer
bool parse_as_f64(Str str, f64* out){
  char* endptr = str.data;
  // TODO:: Is this safe?
  long double v = strtold(endptr, &endptr);
  if((void*)endptr != (str.data + str.count)){
    return false;
  }
  *out = v;
  return true;
}
DEF_SLICE(Parse_Node_Ptr);
