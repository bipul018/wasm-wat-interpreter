#include <stddef.h>
#include <stdio.h>
#include <ctype.h>

#define UTIL_INCLUDE_ALL
#include "../c-utils/util_headers.h"
#define slice_last(slice) slice_inx((slice), ((slice).count-1))
#define slice_first(slice) slice_inx((slice), 0)


DEF_SLICE(u8);
DEF_DARRAY(u8, 8);
typedef u8_Slice Str;
typedef const char* Cstr;
typedef u8_Darray Str_Builder;
DEF_SLICE(Str);
DEF_DARRAY(Str, 8);
Str str_slice(Str str, size_t begin, size_t end){
  if(nullptr == str.data || begin >= end || end > str.count) return (Str){ 0 };
  return (Str){ .data = str.data + begin, .count = end - begin };
}
int str_str_cmp(Str str1, Str str2){
  const size_t l = _min(str1.count, str2.count);
  int v = strncmp(str1.data, str2.data, l);
  if(v != 0) return v;
  return (int)(str1.count - l) - (int)(str2.count - l);
}
int str_cstr_cmp(Str str1, Cstr str2){
  return str_str_cmp(str1, (Str){.data=(void*)str2,.count=strlen(str2)});
}
int cstr_str_cmp(Cstr str1, Str str2){
  return str_str_cmp((Str){.data=(void*)str1,.count=strlen(str1)},str2);
}
Str cstr_to_str(Cstr str){
  return (Str){.data = (void*)str, .count = strlen(str)};
}
bool match_str_prefix(Str haystack, Str needle){
  return str_str_cmp(needle, str_slice(haystack, 0, needle.count)) == 0;
}
bool match_str_suffix(Str haystack, Str needle){
  if(haystack.count<needle.count) return false;
  return str_str_cmp(needle, str_slice(haystack, haystack.count-needle.count, haystack.count)) == 0;
}

#define slice_shrink_front(slice_iden, amt)	\
  do{						\
    if((slice_iden).count < amt) {		\
      (slice_iden).data = nullptr;		\
      (slice_iden).count = 0;			\
      break;					\
    }						\
    (slice_iden).count -= amt;			\
    (slice_iden).data += amt;			\
  }while(0)
  

#define str_print(str) ((int)(str).count), (str).data
#include "lines_and_files.h"
#include "parse_node.h"
#include "parse_into_trees.h"
#include "parsed_utils.h"

#include "parsed_type.h"
#include "parsed_func.h"
#include "parsed_exports.h" 
#include "parsed_data.h"
#include "parsed_module.h"

// Try to run a given function
// Assume all the functions are ordered correctly ?
#include "stk_n_mems.h"

// Load a partiucular data section
bool load_data_to_memory(Memory_Region* mem, const Data* data_sec){
  assert(mem && data_sec && data_sec->offset_expr /* Only active data sections */);

  // Parse the offset expression
  const Parse_Node* off_expr = data_sec->offset_expr;
  if(str_cstr_cmp(off_expr->data, "i32.const") != 0){
    fprintf(stderr, "Expected data section offset expression to be of `i32.const`"
	    ", found `%.*s` instead\n", str_print(off_expr->data));
    return false;
  }
  if(off_expr->children.count != 1 ||
     slice_first(off_expr->children)->children.count != 0){
    fprintf(stderr, "Expected a data section offset expression only consisting a"
	    "single i32.const value, found `%.*s` instead",
	    str_print(slice_first(off_expr->children)->data));
    return false;
  }
  const Str num = slice_first(off_expr->children)->data;
  s64 off;
  // TODO:: Find out if >= 0 checking is good,
  //        The type is 'i32' but offset should be positive no?
  if(!parse_as_s64(num, &off) || off < 0){
    fprintf(stderr, "Couldnt parse `%.*s` as a unsigned integer offset value\n",
	    str_print(num));
    return false;
  }

  if(!memory_rgn_write(mem, off, data_sec->raw_bytes.count, data_sec->raw_bytes.data)){
    return false;
  }
  return true;
}


typedef struct Wasm_Data Wasm_Data;
struct Wasm_Data {
  enum {
    WASM_DATA_I32,
    WASM_DATA_U32,
    WASM_DATA_F32,
    WASM_DATA_U64,
  } tag;
  union{
    s32 di32;
    u32 du32;
    f32 df32;
    u64 du64;
  };
};
const Cstr wasm_names[] = {
  [WASM_DATA_I32] = "i32",
  [WASM_DATA_U32] = "u32",
  [WASM_DATA_F32] = "f32",
  [WASM_DATA_U64] = "u64",
};
DEF_SLICE(Wasm_Data);

void run_sample(Alloc_Interface allocr, Module* mod){
  // Memory leaks are happening here
  u64_Darray stk = init_u64_darray(allocr);
  Memory_Region mem = memory_rgn_init(allocr);
  // TODO:: Wasm standard says that you can implement all the stacks
  //        in the same single stack and its better (also simple)
  //        For now, there is no interaction between various stacks (so mine's simpler)
  //        When those interactions start to happen, might need to revisit all these?
  u64_Darray blk_stk = {.allocr=allocr};

#define pushstk(v)					\
  do{							\
    if(!push_u64_darray(&stk, (v))){			\
      fprintf(stderr, "Couldnt push to stack\n");	\
      return;						\
    }							\
  }while(0)

#define popstk(v)							\
  do{									\
    if(stk.count == 0){							\
      fprintf(stderr, "Couldnt pop argument, stack underflow\n");	\
      return;								\
    }									\
    (v) = slice_last(stk);						\
    if(!pop_u64_darray(&stk, 1)){					\
      fprintf(stderr, "Couldnt pop argument, memory error\n");		\
      return;								\
    }									\
  }while(0)
  

  for_slice(mod->data_sections, i){
    printf("Initializing the memory with data section %zu...\n", i);
    if(!load_data_to_memory(&mem, mod->data_sections.data+i)){
      printf("   ... Loading data section at %zu failed!!!\n", i);
    }
  }

  // Choose a fxn
  //Cstr fxn = "abs_diff";
  Cstr fxn = "pick_branch";

  // Find the entry of the fxn : first find index, then use fxn
  size_t finx = 0;
  for_slice(mod->exports, i){
    const Export expt = slice_inx(mod->exports, i);
    if(str_cstr_cmp(expt.name, fxn) == 0 &&
       expt.export_type == FUNCTION_EXPORT_TYPE){
      finx = expt.export_idx;
      goto found_the_fxn;
    }
  }
  fprintf(stderr, "Couldnt find any function export named %s\n", fxn);
  return;
 found_the_fxn:
  if(finx >= mod->funcs.count) {
    fprintf(stderr, "Couldnt find any data for function %s at index %zu\n", fxn, finx);
    return;
  }
  const Func* func = mod->funcs.data + finx;

  // Instead of forming a predetermined list of args,
  // Need to allocate an array of data representing each of
  //   local variables (including parameters) then initialize those
  //   representing parameters with the arguments
  Wasm_Data_Slice vars = SLICE_ALLOC(allocr, Wasm_Data, func->local_vars.count);
  // Validate and initialize the parameters
  {
    // Localized arguments to not leak them 
    Wasm_Data args[] = {
    {.tag = WASM_DATA_I32, .di32 = 351},
    {.tag = WASM_DATA_I32, .di32 = 420},
    };

    if(func->param_count != _countof(args)){
      fprintf(stderr, "Trying to feed %zu args for a function of %zuarity\n",
    	    _countof(args), func->param_count);
      return;
    }
    
    for_slice(func->local_vars, i){
      if(i >= func->param_count) break;
      Str pt = slice_inx(func->local_vars, i);
      if(cstr_str_cmp(wasm_names[args[i].tag], pt) != 0){
	fprintf(stderr, "Expected arg %zu to be of type `%.*s`, found `%s`\n",
		i, str_print(pt), wasm_names[args[i].tag]);
	return;
      }
      slice_inx(vars, i) = args[i];
    }
  }

  
  
  printf_stk(stk, "The current stack is : ");
  printf("\nThe initial memory is : \n    ");
  memory_rgn_dump(&mem);
  printf("\n");

  // TODO:: Care about the result ?
  // Go through the opcodes?
  for_slice(func->opcodes, i){
    const Str op = slice_inx(func->opcodes, i);
    if(str_cstr_cmp(op, "local.get") == 0){
      // TODO:: Find if there is some better way of validating types
      i++; // maybe verify that its not ended yet ??
      u64 inx;
      if(!parse_as_u64(slice_inx(func->opcodes, i), &inx)){
	// TODO:: Also print location of source code
	fprintf(stderr, "Expected %zu-th opcode to be index, found `%.*s`\n",
		i, str_print(slice_inx(func->opcodes, i)));
	return;
      }
      // See if the index exists
      if(inx >= vars.count){
	// TODO:: Also print location of source code
	fprintf(stderr, "Tried to access %zu-th var when only %zu vars are present\n",
		inx, vars.count);
	return;
      }
      // Arguments are validated already
      pushstk(slice_inx(vars, inx).du64);
    } else if (str_cstr_cmp(op, "local.tee") == 0){
      // TODO:: Find if theres some better way of validating types
      i++; // maybe verify that its not ended yet ??
      u64 inx;
      if(!parse_as_u64(slice_inx(func->opcodes, i), &inx)){
	// TODO:: Also print location of source code
	fprintf(stderr, "Expected %zu-th opcode to be index, found `%.*s`\n",
		i, str_print(slice_inx(func->opcodes, i)));
	return;
      }
      // See if the index exists
      if(inx >= vars.count){
	// TODO:: Also print location of source code
	fprintf(stderr, "Tried to access %zu-th var when only %zu vars are present\n",
		inx, vars.count);
	return;
      }
      // Arguments are validated already
      // Get the value from stack without popping and push it
      slice_inx(vars,inx).du64 = slice_last(stk); // TODO:: Maybe implement actual local variable
    } else if (str_cstr_cmp(op, "i32.const") == 0){
      i++; // maybe verify that its not ended yet ??
      s64 v;
      if(!parse_as_s64(slice_inx(func->opcodes, i), &v)){
	// TODO:: Also print location of source code
	fprintf(stderr, "Expected %zu-th opcode to be index, found `%.*s`\n",
		i, str_print(slice_inx(func->opcodes, i)));
	return;
      }
      Wasm_Data p = {0};
      p.di32 = v;
      pushstk(p.du64);
    } else if (str_cstr_cmp(op, "i32.load") == 0){
      // In this case, lets pop stack value first before seeing arguments
      Wasm_Data base = {0};
      popstk(base.du64);
      s64 offset = base.di32; //The memory address to load from

      i++;
      Str oparg = slice_inx(func->opcodes, i);
      // Parse the offset, for now, dont expect alignment
      Cstr name = "offset=";
      // Expect this at the first, then parse
      Str pref = str_slice(oparg, 0, strlen(name));
      // The offset=<>;alignmeht=<>; might be optional, so, for now
      //  If the next op name doesnt begin with offset=, treat it as missing
      if(cstr_str_cmp(name, pref) == 0){
	oparg = str_slice(oparg, strlen(name), oparg.count);
	u64 off;
	if(!parse_as_u64(oparg, &off)){
	  fprintf(stderr, "Cannot process anything other than `offset=<u32>`"
		  " for i32.load, found `%.*s`\n", str_print(pref));
	  return;
	}
	// Adding the constant offset 
	offset += off;
      } else {
	i--; 
      }

      // This offset is the data
      s32 data = 69;
      memory_rgn_read(&mem, offset, sizeof(data), &data);
      // Now push it
      base = (Wasm_Data){0};
      base.di32 = data;
      pushstk(base.du64);
    } else if (str_cstr_cmp(op, "i32.store") == 0){
      // During storage, the stack at top contains the data, then the
      //   dynamic offset where to store, other format of the instruction is same
      //   so, copy-pasting is almost okay for store and load
      Wasm_Data to_store = {0};
      popstk(to_store.du64);
      
      // In this case, lets pop stack value first before seeing arguments
      Wasm_Data base = {0};
      popstk(base.du64);
      s64 offset = base.di32; //The memory address to store to

      i++;
      Str oparg = slice_inx(func->opcodes, i);
      // Parse the offset, for now, dont expect alignment
      Cstr name = "offset=";
      // Expect this at the first, then parse
      Str pref = str_slice(oparg, 0, strlen(name));
      // The offset=<>;alignmeht=<>; might be optional, so, for now
      //  If the next op name doesnt begin with offset=, treat it as missing
      if(cstr_str_cmp(name, pref) == 0){
	oparg = str_slice(oparg, strlen(name), oparg.count);
	u64 off;
	if(!parse_as_u64(oparg, &off)){
	  fprintf(stderr, "Cannot process anything other than `offset=<u32>`"
		  " for i32.store, found `%.*s`\n", str_print(pref));
	  return;
	}
	// Adding the constant offset 
	offset += off;
      } else {
	i--; 
      }

      // Now simply store the data 
      (void)memory_rgn_write(&mem, offset, sizeof(to_store.di32), &to_store.di32);
    } else if (str_cstr_cmp(op, "i32.sub") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = p1.di32 - p0.di32;
      pushstk(r.du64);
    } else if (str_cstr_cmp(op, "i32.add") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = p1.di32 + p0.di32;
      pushstk(r.du64);
    } else if (str_cstr_cmp(op, "i32.shl") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = p1.di32 << p0.di32; // TODO:: Find out if the expected behavior matches
      pushstk(r.du64);
    } else if (str_cstr_cmp(op, "i32.shr_s") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = p1.di32 >> p0.di32; // TODO:: Find out if the expected behavior matches
      pushstk(r.du64);
    } else if (str_cstr_cmp(op, "i32.xor") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = p1.di32 ^ p0.di32; // TODO:: Find out if the expected behavior matches
      pushstk(r.du64);
    } else if (str_cstr_cmp(op, "i32.mul") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = p1.di32 * p0.di32;
      pushstk(r.du64);
    } else if (str_cstr_cmp(op, "i32.gt_s") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = (s32)(p1.di32 > p0.di32);
      pushstk(r.du64);
    } else if (str_cstr_cmp(op, "i32.lt_s") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = (s32)(p1.di32 < p0.di32);
      pushstk(r.du64);
    } else if (str_cstr_cmp(op, "select") == 0){
      Wasm_Data comp = {0}, val2 = {0};
      popstk(comp.du64); popstk(val2.du64);
      if(comp.di32==0) slice_last(stk) = val2.du64;
    } else if (str_cstr_cmp(op, "block") == 0){
      // TODO:: It seems that it also takes in 'blocktype' as another argument
      if(!push_u64_darray(&blk_stk, i)){
	fprintf(stderr, "Couldnt push to block stack\n");
	return;
      }
    } else if (str_cstr_cmp(op, "br_if") == 0){
      // The first argument is the number of labels(blocks) to pop
      i++; // maybe verify that its not ended yet ??
      // Now, pop from the stack 
      Wasm_Data comp;
      popstk(comp.du64);
      if(comp.di32!=0){
	u64 v;
	if(!parse_as_u64(slice_inx(func->opcodes, i), &v)){
	  // TODO:: Also print location of source code
	  fprintf(stderr, "Expected %zu-th opcode to be index, found `%.*s`\n",
		  i, str_print(slice_inx(func->opcodes, i)));
	  return;
	}

	if(v >= blk_stk.count){
	  fprintf(stderr, "Expected to break out of %zu-th block, when only %zu are present\n", v+1, blk_stk.count);
	  return;	  
	}

	i = slice_inx(blk_stk, blk_stk.count-v-1);
	// Now do the jump operation
	if(!pop_u64_darray(&blk_stk, v+1)){
	  fprintf(stderr, "Couldnt pop from block stack\n");
	  return;
	}
	// TODO:: See if break operations can be done by other instruction types
	if(str_cstr_cmp(slice_inx(func->opcodes, i), "block") != 0){
	  fprintf(stderr, "Only supports 'block' type labels, found '%.*s'\n",
		  str_print(slice_inx(func->opcodes, i)));
	  return;
	}
	// Now, search in pairs until you get the 'end'
	size_t blk_count = 0;
	do{
	  const Str opcode = slice_inx(func->opcodes, i);
	  if(str_cstr_cmp(opcode, "block") == 0) blk_count++;
	  if(str_cstr_cmp(opcode, "end") == 0) blk_count--;
	} while(blk_count != 0 && (i++) < func->opcodes.count);
	if(blk_count != 0){
	  fprintf(stderr, "Encountered %zu unmatched blocks\n", blk_count);
	  return;
	}
      }
    } else if (str_cstr_cmp(op, "end") == 0){
      // Just an ignorable instruction if encountered separately
    } else if (str_cstr_cmp(op, "return") == 0){
      break; // Maybe there is a better solution
    } else {
      fprintf(stderr, "TODO:: Implement opcode `%.*s`\n", str_print(op));
      return;
    }

    printf_stk(stk, "After opcode %zu (%.*s) : ",
	       i, str_print(op));
    printf("  { ");
    for_slice(vars, i){
      const Wasm_Data d = slice_inx(vars,i);
      printf("%s(", wasm_names[d.tag]);
      switch(d.tag){
      case WASM_DATA_I32: {printf("%ld", (long)d.di32); break;}
      case WASM_DATA_U32: {printf("%lu", (unsigned long)d.du32); break;}
      case WASM_DATA_F32: {printf("%f", (float)d.df32); break;}
      case WASM_DATA_U64: {printf("%llu", (long long unsigned)d.du64); break;}
      }
      printf(") ");
    }
    printf("}\n");

    if(match_str_suffix(op, cstr_to_str("load")) || 
       match_str_suffix(op, cstr_to_str("store"))){
      printf("    ");
      memory_rgn_dump(&mem);
      printf("\n");
    }
    
  }
  printf_stk(stk, "After function execution : ");
  printf("  { ");
  for_slice(vars, i){
    const Wasm_Data d = slice_inx(vars,i);
    printf("%s(", wasm_names[d.tag]);
    switch(d.tag){
    case WASM_DATA_I32: {printf("%ld", (long)d.di32); break;}
    case WASM_DATA_U32: {printf("%lu", (unsigned long)d.du32); break;}
    case WASM_DATA_F32: {printf("%f", (float)d.df32); break;}
    case WASM_DATA_U64: {printf("%llu", (long long unsigned)d.du64); break;}
    }
    printf(") ");
  }
  printf("}\n");
  memory_rgn_dump(&mem);
  printf("\n");
  // Printing the return value also
  if(func->result_iden.count > 0){
    if(stk.count == 0) {
      printf("Expected a return value, but the stack was found to be empty!!!\n");
    } else {
      Wasm_Data d = {.du64 = slice_last(stk)};
      for_range(size_t, i, 0, _countof(wasm_names)){
	if(str_cstr_cmp(func->result_iden, wasm_names[i]) == 0)
	  d.tag = i;
      }
      printf("Return value: %s(", wasm_names[d.tag]);
      switch(d.tag){
      case WASM_DATA_I32: {printf("%ld", (long)d.di32); break;}
      case WASM_DATA_U32: {printf("%lu", (unsigned long)d.du32); break;}
      case WASM_DATA_F32: {printf("%f", (float)d.df32); break;}
      case WASM_DATA_U64: {printf("%llu", (long long unsigned)d.du64); break;}
      }
      printf(") \n");
    }
  }
	
      

  

#undef pushstk
#undef popstk

  (void)resize_u64_darray(&stk, 0);
}

int main(void){
  Alloc_Interface allocr = gen_std_allocator();
  //parse_node_iter_run_demo(allocr);

  Parse_Info parser = init_parse_info(allocr, "hello.wat");

  printf("Printing the file by lines: \n");
  for_slice(parser.line_nums, ln){
    Str line = get_line(parser.file, parser.line_nums, ln);
    printf("%4d: %.*s\n", (int)ln+1, str_print(line));
  }

  u32_Slice poses = SLICE_FROM_ARRAY(u32, ((u32[]){0, 4, 6, 7, 8, 18, 499, 819, 731, 732, 733}));

  // for_slice(poses, i){
  //   Cursor pos = get_line_pos(parser.line_nums, poses.data[i]);
  //   printf("Char at %u is at line %u, offset %u\n",poses.data[i],pos.line+1,pos.offset);
  // }

  if(parse_brackets(&parser)){
    printf("Yes, we could parse %s into a parse tree!!!\n", parser.fname);
  } else {
    printf("Oh no! we could not parse %s\n", parser.fname);
    return 1;
  }

  //for_each_parse_node_df(node, parser.root){
  //  printf("Node %p = %.*s, children = %zu\n", node, str_print(node->data), node->children.count);
  //}

  Module main_module = {0}; // TODO:: itsy bitsy memory leak
  if(!parse_module(allocr, parser.root, &main_module)){
    printf("Couldnt parse the main module!!!\n");
    return 1;
  }
  try_printing_module(&main_module);

  //run_memory_page_sample(allocr);

  run_sample(allocr, &main_module);

  

  free_parse_info(&parser);
  return 0;
}


