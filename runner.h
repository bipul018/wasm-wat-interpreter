// This is intentionally not done, this file is supposed to be included only once
// knowingly as part of some 'unity build' (for now)
// The same reason (also lazy) is why no extra includes are done here
// #pragma once


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

typedef struct Exec_Context Exec_Context;

// All arguments are supposed to be found from the stack directly
typedef u64 (Wasm_Fxn_Ptr)(Alloc_Interface, Exec_Context*, void*);
typedef struct Wasm_Fxn Wasm_Fxn;
struct Wasm_Fxn {
  Wasm_Fxn_Ptr* fptr;
  void* data;  
};
DEF_SLICE(Wasm_Fxn);
struct Exec_Context {
  const Module* mod;
  u64_Darray stk;
  u64_Darray blk_stk;

  Memory_Region mem;
  Wasm_Fxn_Slice fxns;
};

Wasm_Fxn_Ptr wasm_fxn_executor; // Used to make calls to a wasm fxn, expects param in stk
Wasm_Fxn_Ptr dummy_import_fxn; // To be used everywhere if import not fo

// TODO:: figure out some other way of resolving the imports maybe
Exec_Context init_exec_context(Alloc_Interface allocr, const Module* mod){
  // Allocate the memory region, stk, blk_stk
  Exec_Context cxt = {
    .mod=mod,
    .stk={.allocr=allocr},
    .blk_stk={.allocr=allocr},
  };
  cxt.mem = memory_rgn_init(allocr);

  // Initialize the data segments
  for_slice(mod->data_sections, i){
    printf("Initializing the memory with data section %zu...\n", i);
    if(!load_data_to_memory(&cxt.mem, mod->data_sections.data+i)){
      printf("   ... Loading data section at %zu failed!!!\n", i);
    }
  }

  // Analyze the imports and fill up fxn slice
  size_t fxn_cnt = mod->funcs.count;
  for_slice(mod->imports, i){
    if(mod->imports.data[i].import_type = FUNCTION_IMPORT_TYPE) fxn_cnt++;
  }
  cxt.fxns = SLICE_ALLOC(allocr, Wasm_Fxn, fxn_cnt);
  assert(cxt.fxns.data);

  fxn_cnt = 0;
  for_slice(mod->imports, i){
    if(mod->imports.data[i].import_type = FUNCTION_IMPORT_TYPE){
      // Manually fill them for now
      slice_inx(cxt.fxns, fxn_cnt).fptr = dummy_import_fxn;
      slice_inx(cxt.fxns, fxn_cnt).data = mod->imports.data+i;
      fxn_cnt++;
    }
  }
  for_slice(mod->funcs, i){
    slice_inx(cxt.fxns, fxn_cnt).fptr = wasm_fxn_executor;
    // TODO:: Make this feed into another fxn that takes not just Func*
    //        but also other metadata like 'trace/step' 
    slice_inx(cxt.fxns, fxn_cnt).data = mod->funcs.data + i;
    fxn_cnt++;    
  }

  return cxt;
}

void deinit_exec_context(Alloc_Interface allocr, Exec_Context* cxt){
  if(!cxt) return;
  SLICE_FREE(allocr, cxt->fxns);
  (void)resize_u64_darray(&cxt->stk, 0);
  (void)resize_u64_darray(&cxt->blk_stk, 0);
  memory_rgn_deinit(&cxt->mem);
  (*cxt) = (Exec_Context){0};
}

u64 dummy_import_fxn(Alloc_Interface allocr, Exec_Context* cxt, void* data){
  (void)allocr;
  (void)cxt;
  const Import* impt = (const Import*)data;
  fprintf(stderr, "The function `%.*s` of module `%.*s` has not yet been imported!!!\n",
	  str_print(impt->self_name), str_print(impt->mod_name));
  return 0;
}

typedef struct Wasm_Data Wasm_Data;
struct Wasm_Data {
  enum {
    WASM_DATA_VOID=0, // Cannot actually use this to store items
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
  [WASM_DATA_VOID] = "void",
};
DEF_SLICE(Wasm_Data);

Wasm_Data wasm_data_by_type(const Str type_name){
  Wasm_Data d = {0};
  for_range(size_t, i, 0, _countof(wasm_names)){
    if(str_cstr_cmp(type_name, wasm_names[i]) == 0){
      d.tag = i;
    }
  }
  return d;
}

u64 exec_wasm_fxn(Alloc_Interface allocr, Exec_Context* cxt, size_t finx){

  // Find if finx exists and extract the data
  if(finx >= cxt->fxns.count){
    fprintf(stderr, "Expected to call %zu-th fxn when only %zu fxns are registered\n",
	    finx, cxt->fxns.count);
    return 0;
  }
  Wasm_Fxn fxn = slice_inx(cxt->fxns, finx);

  // If so, first record the stack sizes to later validate
  const u64 stkn = cxt->stk.count;
  const u64 blkn = cxt->blk_stk.count;

  // TODO:: Also extract the type information of the function to know about the parameter and result maybe later 

  u64 cycles = fxn.fptr(allocr, cxt, fxn.data);
  
  if(cycles == 0){
    fprintf(stderr, "Failed to call the function at %zu\n", finx);
    return 0;
  }
  if(cxt->blk_stk.count < blkn){
    fprintf(stderr, "Blocks/Labels stack violation at function %zu\n", finx);
    return 0;
  }
  // TODO:: Fix this hardcoded 5 item limit by properly analyzing types
  if((5 + cxt->stk.count) < blkn){
    fprintf(stderr, "Value stack violation (likely) at function %zu\n", finx);
    return 0;
  }
  return cycles;
}

u64 run_wasm_opcodes(Alloc_Interface allocr, Exec_Context* cxt, const Str_Slice opcodes, Wasm_Data_Slice vars);
// Expect that at call, Func* is resolved into from the index
// Also expect that the stack violation checking is done at the call site directly
//      for uniformity, use the type information at the call site instead of param_count
u64 wasm_fxn_executor(Alloc_Interface allocr, Exec_Context* cxt, void* data){
  const Func* func = (const Func*)data;
  // Expect all arguments being present in the stk, so validate that
  if(func->param_count > cxt->stk.count){
    fprintf(stderr, "Trying to call a function of %zu-arity when the stack"
	    "contains only %zu items\n",
	    func->param_count, cxt->stk.count);
    return 0;
  }

  // Allocate for the local variables
  Wasm_Data_Slice vars = SLICE_ALLOC(allocr, Wasm_Data,
					   func->local_vars.count);
  if(func->local_vars.count > 0 && !vars.data){
    fprintf(stderr, "Couldnt allocate!!!\n");
    return 0;
  }
  // Initialize the arguments and pop from stack
  for_slice(func->local_vars, i) // Initing types
    slice_inx(vars,i) = wasm_data_by_type(slice_inx(func->local_vars, i));

  for_range(size_t, i, 0, func->param_count){
    const size_t stk_inx = cxt->stk.count - func->param_count + i;
    slice_inx(vars,i).du64=slice_inx(cxt->stk, stk_inx);
  }
  if(!pop_u64_darray(&cxt->stk, func->param_count)){
    fprintf(stderr, "Couldnt allocate!!!\n");
    return 0;
  }

  // Call the fxn
  const u64 calld_cyc = run_wasm_opcodes(allocr, cxt, func->opcodes, vars);
  if(func->opcodes.count != 0 && calld_cyc == 0){
    // TODO:: Make this more informative
    fprintf(stderr, "Failure in calling a function\n");
    return 0;
  }
  // Free local variables
  SLICE_FREE(allocr, vars);
  return calld_cyc + 1; 
}

u64 run_wasm_opcodes(Alloc_Interface allocr, Exec_Context* cxt, const Str_Slice opcodes, Wasm_Data_Slice vars){
  u64_Darray* stk = &cxt->stk;
  u64_Darray* blk_stk = &cxt->blk_stk;
  Memory_Region* mem = &cxt->mem;
#define pushstk(v)					\
  do{							\
    if(!push_u64_darray(stk, (v))){			\
      fprintf(stderr, "Couldnt push to stack\n");	\
      return 0;						\
    }							\
  }while(0)
#define popstk(v)							\
  do{									\
    if(stk->count == 0){							\
      fprintf(stderr, "Couldnt pop argument, stack underflow\n");	\
      return 0;								\
    }									\
    (v) = slice_last(*stk);						\
    if(!pop_u64_darray(stk, 1)){					\
      fprintf(stderr, "Couldnt pop argument, memory error\n");		\
      return 0;								\
    }									\
  }while(0)
  
  u64 cycles = 0;

  printf_stk(*stk, "Running function, the current stack is : ");
  printf("\nThe initial memory is : \n    ");
  memory_rgn_dump(mem);
  printf("\n");

  // TODO:: Care about the result ?
  // Go through the opcodes?
  for_slice(opcodes, i){
    const Str op = slice_inx(opcodes, i);
    if(str_cstr_cmp(op, "local.get") == 0){
      // TODO:: Find if there is some better way of validating types
      i++; // maybe verify that its not ended yet ??
      u64 inx;
      if(!parse_as_u64(slice_inx(opcodes, i), &inx)){
	// TODO:: Also print location of source code
	fprintf(stderr, "Expected %zu-th opcode to be index, found `%.*s`\n",
		i, str_print(slice_inx(opcodes, i)));
	return 0;
      }
      // See if the index exists
      if(inx >= vars.count){
	// TODO:: Also print location of source code
	fprintf(stderr, "Tried to access %zu-th var when only %zu vars are present\n",
		inx, vars.count);
	return 0;
      }
      // Arguments are validated already
      pushstk(slice_inx(vars, inx).du64);
    } else if (str_cstr_cmp(op, "local.tee") == 0){
      // TODO:: Find if theres some better way of validating types
      i++; // maybe verify that its not ended yet ??
      u64 inx;
      if(!parse_as_u64(slice_inx(opcodes, i), &inx)){
	// TODO:: Also print location of source code
	fprintf(stderr, "Expected %zu-th opcode to be index, found `%.*s`\n",
		i, str_print(slice_inx(opcodes, i)));
	return 0;
      }
      // See if the index exists
      if(inx >= vars.count){
	// TODO:: Also print location of source code
	fprintf(stderr, "Tried to access %zu-th var when only %zu vars are present\n",
		inx, vars.count);
	return 0;
      }
      // Arguments are validated already
      // Get the value from stack without popping and push it
      slice_inx(vars,inx).du64 = slice_last(*stk); // TODO:: Maybe implement actual local variable
    } else if (str_cstr_cmp(op, "local.set") == 0){
      // TODO:: Find if theres some better way of validating types
      i++; // maybe verify that its not ended yet ??
      u64 inx;
      if(!parse_as_u64(slice_inx(opcodes, i), &inx)){
	// TODO:: Also print location of source code
	fprintf(stderr, "Expected %zu-th opcode to be index, found `%.*s`\n",
		i, str_print(slice_inx(opcodes, i)));
	return 0;
      }
      // See if the index exists
      if(inx >= vars.count){
	// TODO:: Also print location of source code
	fprintf(stderr, "Tried to access %zu-th var when only %zu vars are present\n",
		inx, vars.count);
	return 0;
      }
      // Arguments are validated already
      // Get the value from stack and then pop it
      popstk(slice_inx(vars,inx).du64);
    } else if (str_cstr_cmp(op, "i32.const") == 0){
      i++; // maybe verify that its not ended yet ??
      s64 v;
      if(!parse_as_s64(slice_inx(opcodes, i), &v)){
	// TODO:: Also print location of source code
	fprintf(stderr, "Expected %zu-th opcode to be index, found `%.*s`\n",
		i, str_print(slice_inx(opcodes, i)));
	return 0;
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
      Str oparg = slice_inx(opcodes, i);
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
	  return 0;
	}
	// Adding the constant offset 
	offset += off;
      } else {
	i--; 
      }

      // This offset is the data
      s32 data = 69;
      memory_rgn_read(mem, offset, sizeof(data), &data);
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
      Str oparg = slice_inx(opcodes, i);
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
	  return 0;
	}
	// Adding the constant offset 
	offset += off;
      } else {
	i--; 
      }

      // Now simply store the data 
      (void)memory_rgn_write(mem, offset, sizeof(to_store.di32), &to_store.di32);
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
    } else if (str_cstr_cmp(op, "i32.gt_u") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      // TODO:: Need to ensure that this is alright
      r.di32 = (s32)((u32)p1.di32 > (u32)p0.di32);
      pushstk(r.du64);
    } else if (str_cstr_cmp(op, "i32.ge_s") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = (s32)(p1.di32 >= p0.di32);
      pushstk(r.du64);
    } else if (str_cstr_cmp(op, "i32.lt_s") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = (s32)(p1.di32 < p0.di32);
      pushstk(r.du64);
    } else if (str_cstr_cmp(op, "select") == 0){
      Wasm_Data comp = {0}, val2 = {0};
      popstk(comp.du64); popstk(val2.du64);
      if(comp.di32==0) slice_last(*stk) = val2.du64;
    } else if (str_cstr_cmp(op, "block") == 0){
      // TODO:: It seems that it also takes in 'blocktype' as another argument
      if(!push_u64_darray(blk_stk, i)){
	fprintf(stderr, "Couldnt push to block/loop stack\n");
	return 0;
      }
    } else if (str_cstr_cmp(op, "loop") == 0){
      // TODO:: It seems that it also takes in 'blocktype' as another argument
      if(!push_u64_darray(blk_stk, i)){
	fprintf(stderr, "Couldnt push to block/loop stack\n");
	return 0;
      }
    } else if (match_str_prefix(op, cstr_to_str("br"))) {
      // The first argument is the number of labels(blocks) to pop
      i++; // maybe verify that its not ended yet ??

      // Test against various br types
      bool to_jmp = 0;
      if (str_cstr_cmp(op, "br") == 0){
	to_jmp = true;
      } else if(str_cstr_cmp(op, "br_if")==0){
	Wasm_Data comp;
	popstk(comp.du64);
	if(comp.di32!=0) to_jmp=true;
      }
      
      if(to_jmp){
	u64 v;
	if(!parse_as_u64(slice_inx(opcodes, i), &v)){
	  // TODO:: Also print location of source code
	  fprintf(stderr, "Expected %zu-th opcode to be index, found `%.*s`\n",
		  i, str_print(slice_inx(opcodes, i)));
	  return 0;
	}

	if(v >= blk_stk->count){
	  fprintf(stderr, "Expected to break out of %zu-th block, when only %zu are present\n", v+1, blk_stk->count);
	  return 0;	  
	}

	i = slice_inx(*blk_stk, blk_stk->count-v-1);
	// Now do the jump operation
	if(!pop_u64_darray(blk_stk, v+1)){
	  fprintf(stderr, "Couldnt pop from block stack\n");
	  return 0;
	}
	// TODO:: See if break operations can be done by other instruction types
	const Str label_op = slice_inx(opcodes, i);
	if((str_cstr_cmp(label_op, "block") != 0) && (str_cstr_cmp(label_op, "loop") != 0)) {
	  fprintf(stderr, "Only supports 'block' and 'loop' type labels, found '%.*s'\n",
		  str_print(label_op));
	  return 0;
	}
	// For 'loop' type blocks, you dont need to go to the end block
	//     but you need to re-push the loop index
	if(str_cstr_cmp(label_op, "loop") == 0){
	  if(!push_u64_darray(blk_stk, i)){
	    fprintf(stderr, "Couldnt push to block/loop stack\n");
	    return 0;
	  }
	} else {
	  // Now, search in pairs until you get the 'end'
	  size_t blk_count = 0;
	  do{
	    const Str opcode = slice_inx(opcodes, i);
	    if(str_cstr_cmp(opcode, "block") == 0) blk_count++;
	    if(str_cstr_cmp(opcode, "loop") == 0) blk_count++;
	    if(str_cstr_cmp(opcode, "end") == 0) blk_count--;
	  } while(blk_count != 0 && (i++) < opcodes.count);
	  // On reaching the final 'end' value, the blk_count will be 0
	  // And 'i' will point to the 'end' opcode, but the for loop will go to next
	  // value and thus, 'end' wont be encountered otherwise
	  if(blk_count != 0){
	    fprintf(stderr, "Encountered %zu unmatched blocks\n", blk_count);
	    return 0;
	  }
	}
      }
    } else if (str_cstr_cmp(op, "end") == 0){
      // Might need to pop from the stack
      if(!pop_u64_darray(blk_stk, 1)){
	fprintf(stderr, "Couldnt pop from block stack\n");
	return 0;
      }
    } else if (str_cstr_cmp(op, "call") == 0){
      // The next argument is the function index
      i++; // maybe verify that its not ended yet ??
      u64 finx;
      if(!parse_as_u64(slice_inx(opcodes, i), &finx)){
	// TODO:: Also print location of source code
	fprintf(stderr, "Expected %zu-th opcode to be index, found `%.*s`\n",
		i, str_print(slice_inx(opcodes, i)));
	return 0;
      }
      // Call the function
      const u64 calld_cyc = exec_wasm_fxn(allocr, cxt, finx);
      if(calld_cyc == 0){
	fprintf(stderr, "Failure in calling the function at %zu\n", finx);
	return 0;
      }
      cycles += calld_cyc;
    } else if (str_cstr_cmp(op, "return") == 0){
      break; // Maybe there is a better solution
    } else {
      fprintf(stderr, "TODO:: Implement opcode `%.*s`\n", str_print(op));
      return 0;
    }
    cycles++;
    printf_stk(*stk, "After opcode %zu (%.*s) : ",
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
      memory_rgn_dump(mem);
      printf("\n");
    }
    
  }
  printf("Processed %zu instructions \n", cycles);
  printf_stk(*stk, "After function execution : ");
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
#undef pushstk
#undef popstk
  return cycles;
}


// Calls a function in wasm (returns the no of instructions processed)
// TODO:: allocr might become unnecessary
u64 call_func_(Alloc_Interface allocr, const Module* mod, const Func* func, u64_Darray* stk, u64_Darray* blk_stk, Memory_Region* mem, Wasm_Data_Slice vars){
#define pushstk(v)					\
  do{							\
    if(!push_u64_darray(stk, (v))){			\
      fprintf(stderr, "Couldnt push to stack\n");	\
      return 0;						\
    }							\
  }while(0)
#define popstk(v)							\
  do{									\
    if(stk->count == 0){							\
      fprintf(stderr, "Couldnt pop argument, stack underflow\n");	\
      return 0;								\
    }									\
    (v) = slice_last(*stk);						\
    if(!pop_u64_darray(stk, 1)){					\
      fprintf(stderr, "Couldnt pop argument, memory error\n");		\
      return 0;								\
    }									\
  }while(0)
  
  u64 cycles = 0;

  printf_stk(*stk, "Running function, the current stack is : ");
  printf("\nThe initial memory is : \n    ");
  memory_rgn_dump(mem);
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
	return 0;
      }
      // See if the index exists
      if(inx >= vars.count){
	// TODO:: Also print location of source code
	fprintf(stderr, "Tried to access %zu-th var when only %zu vars are present\n",
		inx, vars.count);
	return 0;
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
	return 0;
      }
      // See if the index exists
      if(inx >= vars.count){
	// TODO:: Also print location of source code
	fprintf(stderr, "Tried to access %zu-th var when only %zu vars are present\n",
		inx, vars.count);
	return 0;
      }
      // Arguments are validated already
      // Get the value from stack without popping and push it
      slice_inx(vars,inx).du64 = slice_last(*stk); // TODO:: Maybe implement actual local variable
    } else if (str_cstr_cmp(op, "local.set") == 0){
      // TODO:: Find if theres some better way of validating types
      i++; // maybe verify that its not ended yet ??
      u64 inx;
      if(!parse_as_u64(slice_inx(func->opcodes, i), &inx)){
	// TODO:: Also print location of source code
	fprintf(stderr, "Expected %zu-th opcode to be index, found `%.*s`\n",
		i, str_print(slice_inx(func->opcodes, i)));
	return 0;
      }
      // See if the index exists
      if(inx >= vars.count){
	// TODO:: Also print location of source code
	fprintf(stderr, "Tried to access %zu-th var when only %zu vars are present\n",
		inx, vars.count);
	return 0;
      }
      // Arguments are validated already
      // Get the value from stack and then pop it
      popstk(slice_inx(vars,inx).du64);
    } else if (str_cstr_cmp(op, "i32.const") == 0){
      i++; // maybe verify that its not ended yet ??
      s64 v;
      if(!parse_as_s64(slice_inx(func->opcodes, i), &v)){
	// TODO:: Also print location of source code
	fprintf(stderr, "Expected %zu-th opcode to be index, found `%.*s`\n",
		i, str_print(slice_inx(func->opcodes, i)));
	return 0;
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
	  return 0;
	}
	// Adding the constant offset 
	offset += off;
      } else {
	i--; 
      }

      // This offset is the data
      s32 data = 69;
      memory_rgn_read(mem, offset, sizeof(data), &data);
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
	  return 0;
	}
	// Adding the constant offset 
	offset += off;
      } else {
	i--; 
      }

      // Now simply store the data 
      (void)memory_rgn_write(mem, offset, sizeof(to_store.di32), &to_store.di32);
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
    } else if (str_cstr_cmp(op, "i32.gt_u") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      // TODO:: Need to ensure that this is alright
      r.di32 = (s32)((u32)p1.di32 > (u32)p0.di32);
      pushstk(r.du64);
    } else if (str_cstr_cmp(op, "i32.ge_s") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = (s32)(p1.di32 >= p0.di32);
      pushstk(r.du64);
    } else if (str_cstr_cmp(op, "i32.lt_s") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = (s32)(p1.di32 < p0.di32);
      pushstk(r.du64);
    } else if (str_cstr_cmp(op, "select") == 0){
      Wasm_Data comp = {0}, val2 = {0};
      popstk(comp.du64); popstk(val2.du64);
      if(comp.di32==0) slice_last(*stk) = val2.du64;
    } else if (str_cstr_cmp(op, "block") == 0){
      // TODO:: It seems that it also takes in 'blocktype' as another argument
      if(!push_u64_darray(blk_stk, i)){
	fprintf(stderr, "Couldnt push to block/loop stack\n");
	return 0;
      }
    } else if (str_cstr_cmp(op, "loop") == 0){
      // TODO:: It seems that it also takes in 'blocktype' as another argument
      if(!push_u64_darray(blk_stk, i)){
	fprintf(stderr, "Couldnt push to block/loop stack\n");
	return 0;
      }
    } else if (match_str_prefix(op, cstr_to_str("br"))) {
      // The first argument is the number of labels(blocks) to pop
      i++; // maybe verify that its not ended yet ??

      // Test against various br types
      bool to_jmp = 0;
      if (str_cstr_cmp(op, "br") == 0){
	to_jmp = true;
      } else if(str_cstr_cmp(op, "br_if")==0){
	Wasm_Data comp;
	popstk(comp.du64);
	if(comp.di32!=0) to_jmp=true;
      }
      
      if(to_jmp){
	u64 v;
	if(!parse_as_u64(slice_inx(func->opcodes, i), &v)){
	  // TODO:: Also print location of source code
	  fprintf(stderr, "Expected %zu-th opcode to be index, found `%.*s`\n",
		  i, str_print(slice_inx(func->opcodes, i)));
	  return 0;
	}

	if(v >= blk_stk->count){
	  fprintf(stderr, "Expected to break out of %zu-th block, when only %zu are present\n", v+1, blk_stk->count);
	  return 0;	  
	}

	i = slice_inx(*blk_stk, blk_stk->count-v-1);
	// Now do the jump operation
	if(!pop_u64_darray(blk_stk, v+1)){
	  fprintf(stderr, "Couldnt pop from block stack\n");
	  return 0;
	}
	// TODO:: See if break operations can be done by other instruction types
	const Str label_op = slice_inx(func->opcodes, i);
	if((str_cstr_cmp(label_op, "block") != 0) && (str_cstr_cmp(label_op, "loop") != 0)) {
	  fprintf(stderr, "Only supports 'block' and 'loop' type labels, found '%.*s'\n",
		  str_print(label_op));
	  return 0;
	}
	// For 'loop' type blocks, you dont need to go to the end block
	//     but you need to re-push the loop index
	if(str_cstr_cmp(label_op, "loop") == 0){
	  if(!push_u64_darray(blk_stk, i)){
	    fprintf(stderr, "Couldnt push to block/loop stack\n");
	    return 0;
	  }
	} else {
	  // Now, search in pairs until you get the 'end'
	  size_t blk_count = 0;
	  do{
	    const Str opcode = slice_inx(func->opcodes, i);
	    if(str_cstr_cmp(opcode, "block") == 0) blk_count++;
	    if(str_cstr_cmp(opcode, "loop") == 0) blk_count++;
	    if(str_cstr_cmp(opcode, "end") == 0) blk_count--;
	  } while(blk_count != 0 && (i++) < func->opcodes.count);
	  // On reaching the final 'end' value, the blk_count will be 0
	  // And 'i' will point to the 'end' opcode, but the for loop will go to next
	  // value and thus, 'end' wont be encountered otherwise
	  if(blk_count != 0){
	    fprintf(stderr, "Encountered %zu unmatched blocks\n", blk_count);
	    return 0;
	  }
	}
      }
    } else if (str_cstr_cmp(op, "call") == 0){
      // The next argument is the function index
      i++; // maybe verify that its not ended yet ??
      u64 finx;
      if(!parse_as_u64(slice_inx(func->opcodes, i), &finx)){
	// TODO:: Also print location of source code
	fprintf(stderr, "Expected %zu-th opcode to be index, found `%.*s`\n",
		i, str_print(slice_inx(func->opcodes, i)));
	return 0;
      }

      // Find if the 'finx' function exists
      if(finx >= mod->funcs.count){
	fprintf(stderr, "No function exists at index %zu\n", finx);
	return 0;
      }
      // Get the number of arguments it expects and ensure that stk contains them
      const Func* calld = mod->funcs.data + finx;
      if(calld->param_count > stk->count){
	fprintf(stderr, "Trying to call a function of %zu-arity when the stack"
		"contains only %zu items\n",
		calld->param_count, stk->count);
	return 0;
      }

      // Allocate a slice for the local variables and fill the arguments
      Wasm_Data_Slice calld_vars = SLICE_ALLOC(allocr, Wasm_Data,
					       calld->local_vars.count);
      if(calld->local_vars.count > 0 && !calld_vars.data){
	fprintf(stderr, "Couldnt allocate!!!\n");
	return 0;
      }
      for_slice(calld->local_vars, i) // Initing types
	slice_inx(calld_vars,i) = wasm_data_by_type(slice_inx(calld->local_vars, i));
      for_range(size_t, i, 0, calld->param_count){
	const size_t stk_inx = stk->count - calld->param_count + i;
	slice_inx(calld_vars,i).du64=slice_inx(*stk, stk_inx);
      }
      if(!pop_u64_darray(stk, calld->param_count)){
	fprintf(stderr, "Couldnt allocate!!!\n");
	return 0;
      }

      // Record the current block label count and the stack count (as a stk frame ptr)
      const size_t prev_blk_cnt = blk_stk->count;
      const size_t prev_stk_cnt = stk->count;

      // Call the function
      u64 calld_cyc = call_func_(allocr, mod, calld, stk, blk_stk, mem, calld_vars);
      if(calld->opcodes.count != 0 && calld_cyc == 0){
	fprintf(stderr, "Failure in calling the function at %zu\n", finx);
	return 0;
      }
      cycles += calld_cyc;

      // Check if any stack violation has occured wrt if ay returned value
      //   was expected or not
      if(blk_stk->count < prev_blk_cnt){
	fprintf(stderr,"Label stack violation has occured, expected >=%zu, found %zu\n",
		prev_blk_cnt, blk_stk->count);
	return 0;
      }
      blk_stk->count = prev_blk_cnt;
      if(stk->count !=
	 (prev_stk_cnt +
	  (calld->result_iden.count != 0))){ // Only 1 return type maybe
	fprintf(stderr,"Value stack violation has occured, expected >=%zu, found %zu\n",
		prev_stk_cnt, stk->count);
	return 0;
      }
      // Free the local variables
      SLICE_FREE(allocr, calld_vars);
    } else if (str_cstr_cmp(op, "end") == 0){
      // Might need to pop from the stack
      if(!pop_u64_darray(blk_stk, 1)){
	fprintf(stderr, "Couldnt pop from block stack\n");
	return 0;
      }
    } else if (str_cstr_cmp(op, "return") == 0){
      break; // Maybe there is a better solution
    } else {
      fprintf(stderr, "TODO:: Implement opcode `%.*s`\n", str_print(op));
      return 0;
    }
    cycles++;
    printf_stk(*stk, "After opcode %zu (%.*s) : ",
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
      memory_rgn_dump(mem);
      printf("\n");
    }
    
  }
  printf("Processed %zu instructions \n", cycles);
  printf_stk(*stk, "After function execution : ");
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
#undef pushstk
#undef popstk
  return cycles;
}


void run_sample_(Alloc_Interface allocr, Module* mod){
  // Memory leaks are happening here
  u64_Darray stk = init_u64_darray(allocr);
  Memory_Region mem = memory_rgn_init(allocr);
  // TODO:: Wasm standard says that you can implement all the stacks
  //        in the same single stack and its better (also simple)
  //        For now, there is no interaction between various stacks (so mine's simpler)
  //        When those interactions start to happen, might need to revisit all these?
  u64_Darray blk_stk = {.allocr=allocr};

  // Initialize data section
  for_slice(mod->data_sections, i){
    printf("Initializing the memory with data section %zu...\n", i);
    if(!load_data_to_memory(&mem, mod->data_sections.data+i)){
      printf("   ... Loading data section at %zu failed!!!\n", i);
    }
  }
  // Find the desired exported function

  //Cstr fxn = "abs_diff";
  //Cstr fxn = "pick_branch";
  //Cstr fxn = "fibo";
  Cstr fxn = "fibo_rec";

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

  // Prepare the arguments
  // Create the local variables array

  // Instead of forming a predetermined list of args,
  // Need to allocate an array of data representing each of
  //   local variables (including parameters) then initialize those
  //   representing parameters with the arguments
  Wasm_Data_Slice vars = SLICE_ALLOC(allocr, Wasm_Data, func->local_vars.count);
  // Validate and initialize the parameters
  {
    // Localized arguments to not leak them 
    Wasm_Data args[] = {
      //{.tag = WASM_DATA_I32, .di32 = 351},
      //{.tag = WASM_DATA_I32, .di32 = 420},
      {.tag = WASM_DATA_I32, .di32 = 7},
    };

    if(func->param_count != _countof(args)){
      fprintf(stderr, "Trying to feed %zu args for a function of %zuarity\n",
    	    _countof(args), func->param_count);
      return;
    }
    
    for_slice(func->local_vars, i) // Initing types
      slice_inx(vars,i) = wasm_data_by_type(slice_inx(func->local_vars, i));

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

  // Call the function
  const size_t blk_count = blk_stk.count;
  const u64 cycles = call_func_(allocr, mod, func, &stk, &blk_stk, &mem, vars);
  if(cycles == 0){
    fprintf(stderr, "Failure in calling the function at finx\n");
    return;
  }

  // Extract the returned value (if any)
  Wasm_Data d = {.tag=WASM_DATA_VOID};
  if(func->result_iden.count > 0){
    if(stk.count == 0) {
      printf("Expected a return value, but the stack was found to be empty!!!\n");
    } else {
      d.du64 = slice_last(stk);
      for_range(size_t, i, 0, _countof(wasm_names)){
	if(str_cstr_cmp(func->result_iden, wasm_names[i]) == 0)
	  d.tag = i;
      }
    }
  }

  // Free vars
  SLICE_FREE(allocr, vars);

  // Free blk_stk or maybe reset the blk_stk just in case
  blk_stk.count = blk_count;

  // Dump the memory maybe
  memory_rgn_dump(&mem);
  printf("\n");
  // Free stk and mem
  (void)resize_u64_darray(&stk, 0);
  memory_rgn_deinit(&mem);

  // Printing the return value also
  printf("Return value: %s(", wasm_names[d.tag]);
  switch(d.tag){
  case WASM_DATA_I32: {printf("%ld", (long)d.di32); break;}
  case WASM_DATA_U32: {printf("%lu", (unsigned long)d.du32); break;}
  case WASM_DATA_F32: {printf("%f", (float)d.df32); break;}
  case WASM_DATA_U64: {printf("%llu", (long long unsigned)d.du64); break;}
  case WASM_DATA_VOID: break;
  }
  printf(") \n");
  printf("Processed %zu instructions\n", cycles);
}

void run_sample(Alloc_Interface allocr, Module* mod){
  Exec_Context exec_cxt = init_exec_context(allocr, mod);
  // Find the desired exported function

  //Cstr fxn = "abs_diff";
  //Cstr fxn = "pick_branch";
  //Cstr fxn = "fibo";
  Cstr fxn = "fibo_rec";

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
  Wasm_Data args[] = {
    //{.tag = WASM_DATA_I32, .di32 = 351},
    //{.tag = WASM_DATA_I32, .di32 = 420},
    {.tag = WASM_DATA_I32, .di32 = 7},
  };
  for_range(size_t, i, 0, _countof(args)){
    if(!push_u64_darray(&exec_cxt.stk, args[i].du64)){
      fprintf(stderr, "Couldnt allocate!!!\n");
      return;
    }
  }
  u64 cycles = exec_wasm_fxn(allocr, &exec_cxt, finx);

  if(cycles == 0){
    fprintf(stderr, "Failure during execution of %s\n", fxn);
    return;
  }

  printf("It took %zu cycles to complete execution of function %s\n", (size_t)cycles, fxn);
  // TODO:: Also print the initial arguments and the final state

  deinit_exec_context(allocr, &exec_cxt);
}
