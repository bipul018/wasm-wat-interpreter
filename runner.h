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

// Only for initial testing
Wasm_Fxn_Ptr wasm_extern_printint;
Wasm_Fxn_Ptr wasm_extern_printhex;

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
      // Hardcode the 'printint' fxn for now
      if(str_cstr_cmp(mod->imports.data[i].self_name, "printint") == 0){
	slice_inx(cxt.fxns, fxn_cnt).fptr = wasm_extern_printint;
      }
      // Hardcode the 'printhex' fxn for now
      else if(str_cstr_cmp(mod->imports.data[i].self_name, "printhex") == 0){
	slice_inx(cxt.fxns, fxn_cnt).fptr = wasm_extern_printhex;
      }
      else {
	// Manually fill them for now
	slice_inx(cxt.fxns, fxn_cnt).fptr = dummy_import_fxn;
	slice_inx(cxt.fxns, fxn_cnt).data = mod->imports.data+i;
      }
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

u64 wasm_extern_printint(Alloc_Interface allocr, Exec_Context* cxt, void* d){
  (void)allocr;
  (void)d;

  if(cxt->stk.count < 1){
    fprintf(stderr, "Expected 1 argument, found the value stack to be empty\n");
    return 0;
  }

  Wasm_Data v = {.du64 = slice_last(cxt->stk)};
  (void)pop_u64_darray(&cxt->stk, 1);

  printf("\n-----------------------\n%d\n----------------------\n",
	 (int)v.di32);
  return 1;
}
u64 wasm_extern_printhex(Alloc_Interface allocr, Exec_Context* cxt, void* d){
  (void)allocr;
  (void)d;

  if(cxt->stk.count < 1){
    fprintf(stderr, "Expected 1 argument, found the value stack to be empty\n");
    return 0;
  }

  Wasm_Data v = {.du64 = slice_last(cxt->stk)};
  (void)pop_u64_darray(&cxt->stk, 1);

  printf("\n-----------------------\n%x\n----------------------\n",
	 (int)v.di32);
  return 1;
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
  if((5 + cxt->stk.count) < stkn){
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


// Operation 1: find matching end block
//    starting block can be block/loop/if/else
//    during scan though, only block/loop/if are looked for
//    end blocks are end/else, so might need to do twice for if/else block pair
// Returns -1 on failure?
static s64 find_end_block(Str_Slice opcodes, size_t inx){
  s64 cnt = 0;
  Str op = slice_inx(opcodes, inx);
  if(str_cstr_cmp(op, "block") != 0 &&
     str_cstr_cmp(op, "loop") != 0 &&
     str_cstr_cmp(op, "if") != 0 &&
     str_cstr_cmp(op, "else") != 0){
    return -1;
  }
  cnt++; inx++;
  for(; inx < opcodes.count; inx++){
    op = slice_inx(opcodes, inx);
    if(str_cstr_cmp(op, "block") == 0 ||
       str_cstr_cmp(op, "loop") == 0 ||
       str_cstr_cmp(op, "if") == 0) cnt++;
    if(str_cstr_cmp(op, "end") == 0) cnt--;
    // If this is the 'last else' block, you break
    // Otherwise, just dont count it
    if(str_cstr_cmp(op, "else") == 0 && cnt == 1) cnt--;
    if(cnt == 0) break;
  }
  if(cnt != 0) return -1;
  return inx; // Returns the index of the end block, need to do +1 for continuation
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
    } else if (str_cstr_cmp(op, "i32.and") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = p1.di32 & p0.di32;
      pushstk(r.du64);
    } else if (str_cstr_cmp(op, "i32.div_u") == 0){
      Wasm_Data p_divr = {0}, p_divd = {0}, r = {0};
      popstk(p_divr.du64); popstk(p_divd.du64);
      // TODO:: Find out more about these traps
      if(p_divr.di32 == 0) {
	fprintf(stderr, "Division by 0 attempted\n");
	return 0;
      }
      // TODO:: Figure out what the deal with '_u' is actually
      r.di32 = (s32)((u32)p_divd.di32 / (u32)p_divr.di32);
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
    } else if (str_cstr_cmp(op, "i32.le_s") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = (s32)(p1.di32 <= p0.di32);
      pushstk(r.du64);
    } else if (str_cstr_cmp(op, "i32.eqz") == 0){
      Wasm_Data p = {0},  r = {0};
      popstk(p.du64);
      r.di32 = (s32)(p.di32 == 0);
      pushstk(r.du64);
    } else if (str_cstr_cmp(op, "select") == 0){
      Wasm_Data comp = {0}, val2 = {0};
      popstk(comp.du64); popstk(val2.du64);
      if(comp.di32==0) slice_last(*stk) = val2.du64;
    } else if (str_cstr_cmp(op, "block") == 0){
      // TODO:: It seems that it also takes in 'blocktype' as another argument
      s64 end = find_end_block(opcodes, i);
      if(end < 0){
	fprintf(stderr, "Unmatched block found for opcode %zu\n", i);
	return 0;
      }
      if(!push_u64_darray(blk_stk, (u64)end)){
	fprintf(stderr, "Couldnt push to block/loop stack\n");
	return 0;
      }
    } else if (str_cstr_cmp(op, "loop") == 0){
      // TODO:: It seems that it also takes in 'blocktype' as another argument
      if(!push_u64_darray(blk_stk, i)){
	fprintf(stderr, "Couldnt push to block/loop stack\n");
	return 0;
      }
    } else if (str_cstr_cmp(op, "if") == 0){
      // TODO:: It seems that it also takes in 'blocktype' as another argument
      Wasm_Data comp;
      popstk(comp.du64);

      // Find the end of the block first
      const s64 end1 = find_end_block(opcodes, i);
      if(end1 < 0){
	fprintf(stderr, "Unmatched block found for opcode %zu(%.*s)\n",
		i, str_print(slice_inx(opcodes, i)));
	return 0;
      }

      // For != 0 condition, continue but skip to the 'end'
      // For == 0 condition, if there is 'else' block, continue to that
      //                     else, go to 'end' directly
      s64 end2 = end1;
      if(str_cstr_cmp(slice_inx(opcodes, end1), "else") == 0){
	end2 = find_end_block(opcodes, end1);
	if(end2 < 0){
	  fprintf(stderr, "Unmatched block found for opcode %zu(%.*s)\n",
		  end1, str_print(slice_inx(opcodes, i)));
	  return 0;
	}
      }

      if(comp.di32 != 0){
	// Execute if block, after jump to 'end2'
	if(!push_u64_darray(blk_stk, (u64)end2)){
	  fprintf(stderr, "Couldnt push to block/loop stack\n");
	  return 0;
	}
      } else if(end1 == end2) {
	// Directly jump past the 'end' 
	i = end1;
      } else {
	// Jump to the 'end1', set label to 'end2'
	if(!push_u64_darray(blk_stk, (u64)end2)){
	  fprintf(stderr, "Couldnt push to block/loop stack\n");
	  return 0;
	}
	i = end1;
      }
    } else if (str_cstr_cmp(op, "else") == 0){
      // TODO:: Always synchronize the behavior with the if block
      // else when encountered normally, should indicate the end of a 'if' part
      //  so, simply behave as a break opcode for the top block marker
      i = slice_inx(*blk_stk, blk_stk->count-1);
      if(!pop_u64_darray(blk_stk, 1)){
	fprintf(stderr, "Couldnt pop from block stack\n");
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
	// Simply jump to the label, and pop upto before the label
	// If 'loop' type, also push the label again

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
	// For 'loop' type blocks, you need to re-push the loop index
	if(str_cstr_cmp(label_op, "loop") == 0){
	  if(!push_u64_darray(blk_stk, i)){
	    fprintf(stderr, "Couldnt push to block/loop stack\n");
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

#include <raylib.h>

#define errnret(retval, ...)			\
  do{						\
    fprintf(stderr, __VA_ARGS__);		\
    return (retval);				\
  }while(0)


u64 init_window(Alloc_Interface, Exec_Context* cxt, void* data){
  Cstr window_name = (Cstr)data;
  printf("--------------- window name: `%s`  ----------\n", window_name);
  // Two int32 args
  if(cxt->stk.count < 2) errnret(0, "Window initialization needs 2 args (w,h)\n");
  Wasm_Data dw, dh = {0};
  dh.du64 = slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");
  dw.du64 = slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");
  
  printf("--------------- window sizes: %dx%d  ----------\n", dw.di32, dh.di32);
  InitWindow(dw.di32, dh.di32, window_name);
  SetTargetFPS(120);
  printf("-------------------------- called the window creation fxn ----------\n");
  return 1;
}

u64 begin_drawing(Alloc_Interface, Exec_Context* cxt, void* data){
  (void)cxt;
  (void)data;
  BeginDrawing();
  return 1;
}
u64 end_drawing(Alloc_Interface, Exec_Context* cxt, void* data){
  (void)cxt;
  (void)data;
  EndDrawing();
  return 1;
}
u64 clear_background(Alloc_Interface, Exec_Context* cxt, void* data){
  // One u32 arg
  if(cxt->stk.count < 1) errnret(0, "Window initialization needs 1 arg: hex background color\n");
  Wasm_Data col = {0};
  col.du64 = slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");

  Color* c = (Color*)&col.du32;

  ClearBackground(*c);
  return 1;
}
u64 window_should_close(Alloc_Interface, Exec_Context* cxt, void* data){
  (void)data;
  Wasm_Data v = {0};
  v.di32=WindowShouldClose();

  if(!push_u64_darray(&cxt->stk, v.du64)) errnret(0, "allocation failure\n");
  return 1;
}
u64 draw_fps(Alloc_Interface, Exec_Context* cxt, void* data){
  // Two int32 args
  if(cxt->stk.count < 2) errnret(0, "Drawing fps needs 2 args, (posx,posy)\n");
  Wasm_Data px={0},py={0},fsz={0};
  px.du64 = slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");
  py.du64 = slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");

  DrawFPS(px.di32, py.di32);
  return 1;
}

void patch_imports(Exec_Context* cxt){
  for_slice(cxt->fxns, i){
    if(slice_inx(cxt->fxns, i).fptr == dummy_import_fxn){
      if(str_cstr_cmp(((Import*)slice_inx(cxt->fxns, i).data)->self_name, "init_window") == 0){
	slice_inx(cxt->fxns, i).fptr = init_window;
	slice_inx(cxt->fxns, i).data = "Sample raylib window";
      }
      else if(str_cstr_cmp(((Import*)slice_inx(cxt->fxns, i).data)->self_name, "begin_drawing") == 0){
	slice_inx(cxt->fxns, i).fptr = begin_drawing;
      }
      else if(str_cstr_cmp(((Import*)slice_inx(cxt->fxns, i).data)->self_name, "end_drawing") == 0){
	slice_inx(cxt->fxns, i).fptr = end_drawing;
      }
      else if(str_cstr_cmp(((Import*)slice_inx(cxt->fxns, i).data)->self_name, "window_should_close") == 0){
	slice_inx(cxt->fxns, i).fptr = window_should_close;
      }
      else if(str_cstr_cmp(((Import*)slice_inx(cxt->fxns, i).data)->self_name, "clear_background") == 0){
	slice_inx(cxt->fxns, i).fptr = clear_background;
      }
      else if(str_cstr_cmp(((Import*)slice_inx(cxt->fxns, i).data)->self_name, "draw_fps") == 0){
	slice_inx(cxt->fxns, i).fptr = draw_fps;
      }
    }
  }
}


void run_sample(Alloc_Interface allocr, Module* mod){
  Exec_Context exec_cxt = init_exec_context(allocr, mod);
  patch_imports(&exec_cxt);
  // Find the desired exported function

  //Cstr fxn = "abs_diff";
  //Cstr fxn = "pick_branch";
  //Cstr fxn = "fibo";
  //Cstr fxn = "fibo_rec";
  //Cstr fxn = "sub";
  //Cstr fxn = "print_sum";
  Cstr fxn = "run_raylib";

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
    //{.tag = WASM_DATA_I32, .di32 = 420},
    //{.tag = WASM_DATA_I32, .di32 = 351},
    {.tag = WASM_DATA_I32, .di32 = 12},
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
