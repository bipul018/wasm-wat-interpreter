// This is intentionally not done, this file is supposed to be included only once
// knowingly as part of some 'unity build' (for now)
// The same reason (also lazy) is why no extra includes are done here
// #pragma once

Str read_entire_file(Alloc_Interface allocr, Cstr filename){
  FILE* file = fopen(filename, "rb");
  assert(file != nullptr);
  int ret = fseek(file, 0, SEEK_END);
  assert(0 == ret);
  size_t size = ftell(file);
  ret = fseek(file, 0, SEEK_SET); // can aso rewind() otherwise
  assert(0 == ret);

  Str out = SLICE_ALLOC(allocr, u8, size+1);
  assert(nullptr != out.data);

  size_t rsz = fread(out.data, 1, size, file);
  assert(rsz == size);

  slice_inx(out, size) = 0;

  return out;
}

// Line number and char positions
DEF_SLICE(u32);
DEF_DARRAY(u32, 4);

u32_Slice parse_lines(Alloc_Interface allocr, Str file){

  u32_Slice out = {0};
  for(int turn = 0; turn < 2; turn++){
    size_t ln = 0;
    for_slice(file, inx){
      if('\n' == slice_inx(file, inx)){
	if(turn != 0) { slice_inx(out, ln) = inx; }
	ln++;

      }
    }
    if(turn == 0){
      out = SLICE_ALLOC(allocr, u32, ln+1);
      assert(nullptr != out.data);
    } else {
      slice_inx(out, ln) = file.count;
    }
  }
  return out;
}

// ln is based on 0 indexing 
Str get_line(Str file, u32_Slice line_nums, u32 ln){
    u32 lp0 = ((ln == 0)?0:(slice_inx(line_nums, ln-1)+1));
    u32 lp1 = slice_inx(line_nums,ln);
    Str line = str_slice(file, lp0, lp1);
    return line;
}

typedef struct Cursor Cursor;
struct Cursor {
  u32 line;
  u32 offset;
};


// Newline character counted as part of old line, overflow line is last line
Cursor get_line_pos(u32_Slice line_nums, size_t filepos){
  Cursor pos = {0};
  for_slice(line_nums, i){
    if(filepos <= slice_inx(line_nums, i)) {
      pos.line = i;
      goto find_off;
    }
  }
  pos.line = line_nums.count-1;
 find_off:
  pos.offset = filepos - ((pos.line == 0)?0:((u32)slice_inx(line_nums, pos.line-1)+1));
  return pos;
}
