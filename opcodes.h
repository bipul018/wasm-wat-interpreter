// This is intentionally not done, this file is supposed to be included only once
// knowingly as part of some 'unity build' (for now)
// The same reason (also lazy) is why no extra includes are done here
// #pragma once

#define OPCODE_LIST(X)					\
  X(OPCODE_I32_ADD	,"i32.add"	)		\
    X(OPCODE_I32_CONST	,"i32.const"	)		\
    X(OPCODE_LOCAL_GET	,"local.get"	)		\
    X(OPCODE_LOCAL_TEE	,"local.tee"	)		\
    X(OPCODE_LOCAL_SET	,"local.set"	)		\
    X(OPCODE_END	,"end"		)		\
    X(OPCODE_BLOCK	,"block"	)		\
    X(OPCODE_LOOP	,"loop"		)		\
    X(OPCODE_IF		,"if"		)		\
    X(OPCODE_ELSE	,"else"		)		\
    X(OPCODE_BR_IF      ,"br_if"        )		\
    X(OPCODE_I32_LOAD8_U,"i32.load8_u"  )		\
    X(OPCODE_I32_NE     ,"i32.ne"       )		\
    X(OPCODE_I32_MUL    ,"i32.mul"      )		\
    X(OPCODE_I32_EQ     ,"i32.eq"       )		\
    X(OPCODE_I32_LOAD8_S,"i32.load8_s"  )		\
  X(OPCODE_F64_ADD    ,"f64.add"      )			\
  X(OPCODE_F64_LOAD   ,"f64.load"     )			\
  X(OPCODE_I32_SHL    ,"i32.shl"      )			\
  X(OPCODE_I32_LOAD    ,"i32.load"      )		\
  X(OPCODE_F64_CONVERT_I32_S, "f64.convert_i32_s")	\
  X(OPCODE_F64_CONST, "f64.const")			\
  X(OPCODE_F64_LT, "f64.lt")				\
  X(OPCODE_CALL, "call")				\
  X(OPCODE_F64_STORE, "f64.store")			\
  X(OPCODE_GLOBAL_GET, "global.get")			\
  X(OPCODE_I32_AND, "i32.and")				\
  X(OPCODE_I32_STORE, "i32.store")			\
  X(OPCODE_F64_DIV, "f64.div")				\
  X(OPCODE_SELECT, "select")				\
  X(OPCODE_F64_NEG, "f64.neg")				\
  X(OPCODE_BR, "br")					\
  X(OPCODE_GLOBAL_SET, "global.set")			\
  X(OPCODE_I64_CONST, "i64.const")			\
    X(OPCODE_MEMORY_FILL, "memory.fill")		\
    X(OPCODE_MEMORY_COPY, "memory.copy")		\
    X(OPCODE_I32_SUB, "i32.sub")			\
    X(OPCODE_I32_SHR_S, "i32.shr_s")
  

#define OPCODE_GET_ENUM(a, b) a, 

typedef struct Opcode Opcode;
struct Opcode {
  enum{
    OPCODE_UNKNOWN,// To be used for all the unknown opcodes/literals
    OPCODE_LIST(OPCODE_GET_ENUM) OPCODES_COUNT
  } id;
  Str name;
};
DEF_SLICE(Opcode);



#define OPCODE_GET_ARRAY_ENTRY(a, b) {.id = a, .name=cstr_to_str(b)},
Opcode_Slice pre_process_opcodes(Alloc_Interface allocr, Str_Slice raw_opcodes){
  const Opcode opcode_name_maps[OPCODES_COUNT] = {
    OPCODE_LIST(OPCODE_GET_ARRAY_ENTRY)
  };

  Opcode_Slice opcodes = SLICE_ALLOC(allocr, Opcode, raw_opcodes.count);
  if(!opcodes.data) return opcodes;
  for_slice(raw_opcodes, i){
    Str raw_op = slice_inx(raw_opcodes, i);
    Opcode op = {.name=raw_op, .id=OPCODE_UNKNOWN};
    for_range(long, j, 0, _countof(opcode_name_maps)){
      //printf("Comparing opcode `%.*s` with `%.*s`\n",
      //str_print(raw_op), str_print(opcode_name_maps[j].name));
      if(str_cmp(raw_op, opcode_name_maps[j].name) == 0){
	op.id=opcode_name_maps[j].id;
	break;
      }
    }
    //if(str_cmp(raw_op, "i32.add") == 0) { op.id = OPCODE_I32_ADD; }
    slice_inx(opcodes, i) = op;
  }
  //assert(false);
  return opcodes;
}


// Opcode counter

typedef struct Opcode_Count Opcode_Count;
struct Opcode_Count {
  Opcode op;
  int count;
};
DEF_DARRAY(Opcode_Count, 100);

Opcode_Count_Darray init_opcode_counter(Alloc_Interface allocr){
  const Opcode opcode_name_maps[OPCODES_COUNT] = {
    OPCODE_LIST(OPCODE_GET_ARRAY_ENTRY)
  };

  Opcode_Count_Darray cntr = {.allocr = allocr};
  (void)resize_Opcode_Count_darray(&cntr, OPCODES_COUNT);
  for_range(int, i, 0, _countof(opcode_name_maps)){
    slice_inx(cntr, opcode_name_maps[i].id) = (Opcode_Count){
      .op = opcode_name_maps[i],
      .count = 0,
    };
  }
  slice_inx(cntr, OPCODE_UNKNOWN).op.name = cstr_to_str("unknown");
  return cntr;
}

u64 op_with_id = 0;
void add_opcode_count(Opcode_Count_Darray* cntr, Opcode op){
  assert(cntr);
  // Here, increment count based on 'id'
  slice_inx(*cntr, op.id).count++;
  // If the id was unknown, then only try to find/push it
  if(op.id != OPCODE_UNKNOWN) {
    op_with_id++;
    return;
  }
  for_range(size_t, i, OPCODES_COUNT, cntr->count){
    if(str_cmp(cntr->data[i].op.name, op.name) == 0){
      cntr->data[i].count++;
      return;
    }
  }
  (void)push_Opcode_Count_darray(cntr, (Opcode_Count){.op=op, .count=1});
}

int opcode_count_cmpr(const void* a, const void* b){
  return ((const Opcode_Count*)b)->count - ((const Opcode_Count*)a)->count;
}
void dump_opcode_counts(Opcode_Count_Darray cntr){
  qsort(cntr.data, cntr.count, sizeof(cntr.data[0]), opcode_count_cmpr);
  printf("\n------- Opcode call counts -------");
  for_slice(cntr, i){
    if((i % 3) == 0) printf("\n");
    printf("%.*s->%d\t\t", str_print(cntr.data[i].op.name), cntr.data[i].count);
  }
  printf("\n----------------------------------\n");
  printf("******** %zu op with id was found*********\n", (size_t)op_with_id);
}


// A cache for find_end_block
#define FIND_END_BLOCK_CACHE_SIZE 128
typedef struct Find_End_Block_Cache Find_End_Block_Cache;
struct Find_End_Block_Cache {
  u32 inx; // Actually inx+1, using 0 as 'invalid' cache
  s32 val;
};

// Operation 1: find matching end block
//    starting block can be block/loop/if/else
//    during scan though, only block/loop/if are looked for
//    end blocks are end/else, so might need to do twice for if/else block pair
// Returns -1 on failure?
static s64 find_end_block(Opcode_Slice opcodes, size_t inx,
			  Find_End_Block_Cache cache[FIND_END_BLOCK_CACHE_SIZE]){

  Find_End_Block_Cache* cache_entry = &cache[(1+inx) % FIND_END_BLOCK_CACHE_SIZE];
  if(cache_entry->inx == (1+inx)) return cache_entry->val;
  cache_entry->inx = 1+inx;
  cache_entry->val = -1;

  s64 cnt = 0;
  Opcode op = slice_inx(opcodes, inx);
  if(op.id != OPCODE_BLOCK &&
     op.id != OPCODE_LOOP &&
     op.id != OPCODE_IF &&
     op.id != OPCODE_ELSE){
    goto return_from_fxn;
  }
  cnt++; inx++;
  for(; inx < opcodes.count; inx++){
    op = slice_inx(opcodes, inx);
    if(op.id == OPCODE_BLOCK||
       op.id == OPCODE_LOOP||
       op.id == OPCODE_IF) cnt++;
    if(op.id == OPCODE_END) cnt--;
    // If this is the 'last else' block, you break
    // Otherwise, just dont count it
    if(op.id == OPCODE_ELSE && cnt == 1) cnt--;
    if(cnt == 0) break;
  }
  if(cnt != 0) {
    goto return_from_fxn;
  }
  
  cache_entry->val = inx; // Returns the index of the end block, need to do +1 for continuation
 return_from_fxn:
  return cache_entry->val;
}
// A function to help handle block stack and its items
u64 visit_new_blk(u64_Darray* stk, size_t opn, u16 paramn, u16 retn){
  // Combines the stk location (if enough) and opcode number and returns
  //  the thing needed to push onto the block stack
  // Returns (u64)-1 if some error happens (will also print to stderr)
  assert(stk);
  if(stk->count < paramn){
    fprintf(stderr, "Expected %zu arguments but found only %zu items on stack\n",
	    (size_t)paramn, stk->count);
    return (u64)-1;
  }
  // Pack all the data in a single u64
  // [--- 32 bits stk ---][--- 24 bits opcode num ---][--- 8 bits ret count ---]
  // This does pose a limit for opcode count and such, but we dont care
  // Currently not reasearching the bitpacked structs and such (idk)
  if(opn > (1ULL<<24ULL)){
    fprintf(stderr, "Too high opcode number referenced during block visitation\n");
    return (u64)-1;
  }
  u64 v = retn & ((1<<8)-1);
  v = v | (opn<<8);
  v = v | (((u64)(stk->count-paramn)) << (u64)32);
  return v;
}
size_t break_old_blk(u64_Darray* stk, u64 label){
  // Given the 'label', it decomposes that into the two values, stk pos and opcode number
  // For the stk pos, it then adjusts the stk value (if possible) and saves just the
  // return value amount
  // Returns the index of the opcode to jump to

  // On error (stack problem), it prints error and returns (size_t)-1
  assert(stk);

  const u8 blk_retn = label & ((1<<8)-1);
  label = (label ^ blk_retn)>>8;
  const u32 opn = label & ((((u32)1)<<((u32)24))-1);
  label = (label ^ opn)>>((u32)24);
  const u32 stkcnt = label;

  if(stk->count < (stkcnt+blk_retn)){
    fprintf(stderr, "Stack violation, expected at least %u items, found %zu on stack\n",
	    (stkcnt+blk_retn), stk->count);
    return (size_t)(-1);
  }
  // Now move and pop stack
  size_t to_remove = stk->count - stkcnt - blk_retn;
  if(to_remove > 0){
    if(!downsize_u64_darray(stk, stkcnt+blk_retn, to_remove)){
      fprintf(stderr, "Failed doing memory operation\n");
      return (size_t)(-1);
    }
  }
  
  return opn;
}

Opcode_Count_Darray opcode_cntr;

u64 run_wasm_opcodes(Alloc_Interface allocr, Exec_Context* cxt, const Opcode_Slice opcodes, Wasm_Data_Slice vars){
//PROFILABLE_FXN(u64, run_wasm_opcodes, (Alloc_Interface, allocr), (Exec_Context*, cxt),
//	       (const Str_Slice, opcodes), (Wasm_Data_Slice, vars)){
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
      fprintf(stderr, "Couldnt pop argument, stack underflow : %s:%d\n",__func__, __LINE__); \
      return 0;								\
    }									\
    (v) = slice_last(*stk);						\
    if(!pop_u64_darray(stk, 1)){					\
      fprintf(stderr, "Couldnt pop argument, memory error\n");		\
      return 0;								\
    }									\
  }while(0)

#define pushblk_stk(v)					\
  do{							\
    if(!push_u64_darray(blk_stk, (v))){			\
      fprintf(stderr, "Couldnt push to block stack\n");	\
      return 0;						\
    }							\
  }while(0)
#define popblk_stk(v)							\
  do{									\
    if(blk_stk->count == 0){							\
      fprintf(stderr, "Couldnt pop argument, block stack underflow\n");	\
      return 0;								\
    }									\
    (v) = slice_last(*blk_stk);						\
    if(!pop_u64_darray(blk_stk, 1)){					\
      fprintf(stderr, "Couldnt pop argument, memory error\n");		\
      return 0;								\
    }									\
  }while(0)
  
  u64 cycles = 0;

  bool trace = cxt->trace;
  bool trace_vars = cxt->trace_vars;
  const bool trace_mem = cxt->trace_mem;
  const u64 cycle_limit = cxt->cycle_limit;
  if(trace){
    printf_stk(*stk, "Running function, the current stack is : ");
    if(trace_mem){
      printf("\nThe initial memory is : \n    ");
      memory_rgn_dump(mem);
      printf("\n");
    }
  }

  // Cache for the labels
  Find_End_Block_Cache blk_end_cache[FIND_END_BLOCK_CACHE_SIZE] = {0};

  // TODO:: Care about the result ?
  // Go through the opcodes?
  for_slice(opcodes, i){

    //if(str_cmp("if", slice_inx(opcodes, i).name) == 0) cxt->step_into = true;
    //if(str_cmp("i32.add", slice_inx(opcodes, i).name) == 0) cxt->step_into = true;

    // Step into mode, used to examine the opcodes and stuff
    if(cxt->step_into){
#define NEXT_OPCODE_CHAR 's'
#define DISABLE_STEP_CHAR 'r'
#define GET_HELP_CHAR 'h'
#define DUMP_MEMORY_CHAR 'd'
#define DUMP_LOCALS_CHAR 'l'
#define DUMP_GLOBALS_CHAR 'g'
#define DUMP_OPCODES_CHAR 'p'

      Cstr help =
	"[Get Help: " STRINGIFY(GET_HELP_CHAR) " ]\n"
	"[Next Opcode: " STRINGIFY(NEXT_OPCODE_CHAR) " ]\n"
	"[Disable Stepping: " STRINGIFY(DISABLE_STEP_CHAR) " ]\n"
	"[Dump Memory: " STRINGIFY(DUMP_MEMORY_CHAR) " <begin_abs> <end_abs> ]\n"
	"[Dump Locals: " STRINGIFY(DUMP_LOCALS_CHAR) " <begin_abs> <end_abs> ]\n"
	"[Dump Globals: " STRINGIFY(DUMP_GLOBALS_CHAR) " <begin_abs> <end_abs> ]\n"
	"[Dump Opcodes: " STRINGIFY(DUMP_OPCODES_CHAR) " <begin_relative> <end_relative> ]\n"
	"<If begin_abs = -ve, go the to top> <If end_abs = -ve go to the bottom>"
	;
	
      // Prompt for info
      printf("\nOpcode %zu : `%.*s`\n%s\n",
	     i, str_print(slice_inx(opcodes, i).name), help);
      bool to_quit = false;
      while(!to_quit){
	char c;
	int v = scanf("%c", &c);
	(void)v;
	switch(c){
	case DISABLE_STEP_CHAR: { cxt->step_into = false;}
	case NEXT_OPCODE_CHAR: {to_quit = true; break;}

	case DUMP_MEMORY_CHAR:
	case DUMP_LOCALS_CHAR:
	case DUMP_GLOBALS_CHAR:
	case DUMP_OPCODES_CHAR:
	  {
	    long begin, end;
	    int v = scanf("%ld %ld", &begin, &end);
	    (void)v;
	    switch(c){
	    case DUMP_MEMORY_CHAR:{
	      printf("TODO:: Yet to implement a proper memory dump, for now, take it all\n");
	      memory_rgn_dump(&cxt->mem);
	      break;
	    }
	    case DUMP_LOCALS_CHAR:{
	      if(begin < 0) begin = 0;
	      if(end < 0 || end > vars.count) end = vars.count;
	      if(begin > end) break;
	      printf(" {");
	      for_range(size_t, i, begin, end) wasm_data_print(slice_inx(vars, i));
	      printf("} \n");
	      break;
	    }
	    case DUMP_GLOBALS_CHAR:{
	      if(begin < 0) begin = 0;
	      if(end < 0 || end > cxt->globals.count) end = cxt->globals.count;
	      if(begin > end) break;
	      printf(" {");
	      for_range(size_t, i, begin, end) wasm_data_print(slice_inx(cxt->globals, i));
	      printf("} \n");
	      break;
	    }
	    case DUMP_OPCODES_CHAR:{
	      if(begin > end) break;	      
	      begin = begin + i;
	      end = end + i;
	      if(begin < 0) begin = 0;
	      if(end > opcodes.count) end = opcodes.count;

	      printf(" <");
	      for_range(size_t, i, begin, end)
		printf("%.*s(%d)  ", str_print(slice_inx(opcodes, i).name),
		       (int)slice_inx(opcodes, i).id);
	      printf(">\n");
	      break;
	    }
	    }
	    char d;
	    v = scanf("%c", &d); // flushing newline
	    (void)v;
	    break;
	  }

	default: {printf("Unknown instruction `%c`\n", c);}
	case GET_HELP_CHAR: {
	  printf("\nOpcode %zu : `%.*s`\n%s\n", i, str_print(slice_inx(opcodes, i).name), help);
	  break;
	}
	}
      }
    }

    if(cycle_limit && cycles > cycle_limit){
      printf("reached > %zu cycles, quitting for now\n", (size_t)cycle_limit);
      return 0;
    }

    
    const Opcode op = slice_inx(opcodes, i);
    //add_opcode_count(&opcode_cntr, op);
    //if(str_cmp(op.name, "local.get") == 0){
    if (op.id == OPCODE_LOCAL_GET){
      // TODO:: Find if there is some better way of validating types
      i++; // maybe verify that its not ended yet ??
      u64 inx;
      if(!parse_as_u64(slice_inx(opcodes, i).name, &inx)){
	// TODO:: Also print location of source code
	fprintf(stderr, "Expected %zu-th opcode to be index, found `%.*s`\n",
		i, str_print(slice_inx(opcodes, i).name));
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
    } else if (op.id == OPCODE_LOCAL_TEE){
      //} else if (str_cmp(op.name, "local.tee") == 0){
      // TODO:: Find if theres some better way of validating types
      i++; // maybe verify that its not ended yet ??
      u64 inx;
      if(!parse_as_u64(slice_inx(opcodes, i).name, &inx)){
	// TODO:: Also print location of source code
	fprintf(stderr, "Expected %zu-th opcode to be index, found `%.*s`\n",
		i, str_print(slice_inx(opcodes, i).name));
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
    } else if (op.id == OPCODE_LOCAL_SET){
      //} else if (str_cmp(op.name, "local.set") == 0){
      // TODO:: Find if theres some better way of validating types
      i++; // maybe verify that its not ended yet ??
      u64 inx;
      if(!parse_as_u64(slice_inx(opcodes, i).name, &inx)){
	// TODO:: Also print location of source code
	fprintf(stderr, "Expected %zu-th opcode to be index, found `%.*s`\n",
		i, str_print(slice_inx(opcodes, i).name));
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
    } else if(op.id == OPCODE_GLOBAL_GET){
      //} else if(str_cmp(op.name, "global.get") == 0){
      // TODO:: Find if there is some better way of validating types
      i++; // maybe verify that its not ended yet ??
      u64 inx;
      if(!parse_as_u64(slice_inx(opcodes, i).name, &inx)){
	// TODO:: Also print location of source code
	fprintf(stderr, "Expected %zu-th opcode to be index, found `%.*s`\n",
		i, str_print(slice_inx(opcodes, i).name));
	return 0;
      }
      // See if the index exists
      if(inx >= cxt->globals.count){
	// TODO:: Also print location of source code
	fprintf(stderr, "Tried to access %zu-th global when only %zu globals are present\n",
		inx, cxt->globals.count);
	return 0;
	
      }
      // Arguments are validated already
      pushstk(slice_inx(cxt->globals, inx).du64);
    }else if(op.id == OPCODE_GLOBAL_SET){
      //} else if (str_cmp(op.name, "global.set") == 0){
      // TODO:: Find if theres some better way of validating types
      i++; // maybe verify that its not ended yet ??
      u64 inx;
      if(!parse_as_u64(slice_inx(opcodes, i).name, &inx)){
	// TODO:: Also print location of source code
	fprintf(stderr, "Expected %zu-th opcode to be index, found `%.*s`\n",
		i, str_print(slice_inx(opcodes, i).name));
	return 0;
      }
      // See if the index exists
      if(inx >= cxt->globals.count){
	// TODO:: Also print location of source code
	fprintf(stderr, "Tried to access %zu-th global when only %zu globals are present\n",
		inx, cxt->globals.count);
	return 0;
      }
      // Arguments are validated already
      // Get the value from stack and then pop it
      popstk(slice_inx(cxt->globals,inx).du64);
    //} else if (str_cmp(op.name, "i32.const") == 0){
    } else if (op.id == OPCODE_I32_CONST){
      i++; // maybe verify that its not ended yet ??
      s64 v;
      if(!parse_as_s64(slice_inx(opcodes, i).name, &v)){
	// TODO:: Also print location of source code
	fprintf(stderr, "Expected %zu-th opcode to be i32 literal, found `%.*s`\n",
		i, str_print(slice_inx(opcodes, i).name));
	return 0;
      }
      Wasm_Data p = {0};
      p.di32 = v;
      pushstk(p.du64);
    } else if(op.id == OPCODE_I64_CONST){
      //} else if (str_cmp(op.name, "i64.const") == 0){
      i++; // maybe verify that its not ended yet ??
      s64 v;
      if(!parse_as_s64(slice_inx(opcodes, i).name, &v)){
	// TODO:: Also print location of source code
	fprintf(stderr, "Expected %zu-th opcode to be i64 literal, found `%.*s`\n",
		i, str_print(slice_inx(opcodes, i).name));
	return 0;
      }
      Wasm_Data p = {0};
      p.di64 = v;
      pushstk(p.du64);
    } else if(op.id == OPCODE_F64_CONST){
      //} else if (str_cmp(op.name, "f64.const") == 0){
      i++; // maybe verify that its not ended yet ??
      f64 v;
      if(!parse_as_f64(slice_inx(opcodes, i).name, &v)){
	// TODO:: Also print location of source code
	fprintf(stderr, "Expected %zu-th opcode to be f64 literal, found `%.*s`\n",
		i, str_print(slice_inx(opcodes, i).name));
	return 0;
      }
      Wasm_Data p = {0};
      p.df64 = v;
      pushstk(p.du64);
    } else if(op.id == OPCODE_MEMORY_FILL){
      //} else if (str_cmp(op.name, "memory.fill") == 0){
      Wasm_Data ptr = {0}, ch = {0}, len = {0};
      popstk(len.du64);
      popstk(ch.du64);
      popstk(ptr.du64);

      //printf("$$$$$$$ Filling the memory at %x with %d for %d\n",
      //ptr.di32, ch.di32, len.di32);

      memory_rgn_memset(mem, ptr.di32, len.di32, ch.di32);
    } else if(op.id == OPCODE_MEMORY_COPY){
      //} else if (str_cmp(op.name, "memory.copy") == 0){
      // args: dst, src, size, no return values
      Wasm_Data len={0}, src={0}, dst={0};
      popstk(len.du64);
      popstk(src.du64);
      popstk(dst.du64);

      // Since the memory regions can overlap, doing staging buffer method
      u8_Slice tmp_buff = SLICE_ALLOC(allocr, u8, len.di32);
      if(!tmp_buff.data){
	fprintf(stderr, "memory allocation error");
	return 0;
      }

      if(!memory_rgn_read(&cxt->mem, src.di32, tmp_buff.count, tmp_buff.data)){
	// TODO:: Memory leak
	fprintf(stderr, "memory allocation error");
	return 0;
      }
      if(!memory_rgn_write(&cxt->mem, dst.di32, tmp_buff.count, tmp_buff.data)){
	// TODO:: Memory leak
	fprintf(stderr, "memory allocation error");
	return 0;
      }
      SLICE_FREE(allocr, tmp_buff);
    } else if(op.id == OPCODE_F64_CONVERT_I32_S){
      //} else if (str_cmp(op.name, "f64.convert_i32_s") == 0){
      Wasm_Data p0 = {0}, r = {0};
      popstk(p0.du64);
      r.df64 = p0.di32;
      pushstk(r.du64);
    } else if(op.id == OPCODE_I32_SUB){
      //} else if (str_cmp(op.name, "i32.sub") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = p1.di32 - p0.di32;
      pushstk(r.du64);
      //} else if (str_cmp(op.name, "i32.add") == 0){
    } else if (op.id == OPCODE_I32_ADD){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = p1.di32 + p0.di32;
      pushstk(r.du64);
    } else if(op.id == OPCODE_F64_ADD){
      //} else if (str_cmp(op.name, "f64.add") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.df64 = p1.df64 + p0.df64;
      pushstk(r.du64);
    } else if(op.id == OPCODE_F64_NEG){
      //} else if (str_cmp(op.name, "f64.neg") == 0){
      Wasm_Data p0 = {0}, r = {0};
      popstk(p0.du64);
      r.df64 = -p0.df64;
      pushstk(r.du64);
    } else if(op.id == OPCODE_I32_SHL){
      //} else if (str_cmp(op.name, "i32.shl") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = p1.di32 << p0.di32; // TODO:: Find out if the expected behavior matches
      pushstk(r.du64);
    } else if(op.id == OPCODE_I32_SHR_S){
      //} else if (str_cmp(op.name, "i32.shr_s") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = p1.di32 >> p0.di32; // TODO:: Find out if the expected behavior matches
      pushstk(r.du64);
    } else if(op.id == OPCODE_I32_MUL){
    //} else if (str_cmp(op.name, "i32.mul") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = p1.di32 * p0.di32;
      pushstk(r.du64);
    } else if(op.id == OPCODE_I32_AND){
      //} else if (str_cmp(op.name, "i32.and") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = p1.di32 & p0.di32;
      pushstk(r.du64);
    }  else if(op.id == OPCODE_F64_DIV){
      //} else if (str_cmp(op.name, "f64.div") == 0){
      Wasm_Data p_divr = {0}, p_divd = {0}, r = {0};
      popstk(p_divr.du64); popstk(p_divd.du64);
      // For floating points, div by 0 is not a problem
      r.df64 = p_divd.df64 / p_divr.df64;
      pushstk(r.du64);
    } else if(op.id == OPCODE_F64_LT){
      //} else if (str_cmp(op.name, "f64.lt") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = p1.df64 < p0.df64;
      pushstk(r.du64);
    } else if(op.id == OPCODE_I32_NE){
      //} else if (str_cmp(op.name, "i32.ne") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = p1.di32 != p0.di32;
      pushstk(r.du64);
    } else if(op.id == OPCODE_I32_EQ){
      //} else if (str_cmp(op.name, "i32.eq") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = p1.di32 == p0.di32;
      pushstk(r.du64);
    } else if(op.id == OPCODE_SELECT){
      //} else if (str_cmp(op.name, "select") == 0){
      Wasm_Data comp = {0}, val2 = {0};
      popstk(comp.du64); popstk(val2.du64);
      if(comp.di32==0) slice_last(*stk) = val2.du64;
    } else if(op.id == OPCODE_BLOCK){
      //} else if (str_cmp(op.name, "block") == 0){
      // TODO:: It seems that it also takes in 'blocktype' as another argument
      s64 end = find_end_block(opcodes, i, blk_end_cache);
      if(end < 0){
	fprintf(stderr, "Unmatched block found for opcode %zu\n", i);
	return 0;
      }
      u64 pcnt = 0;
      u64 retcnt = 0;
      for(int it = 1; it < 3; it++){
	if((i+it)>=opcodes.count) break;
	const Str v = slice_inx(opcodes, i+it).name;
	if(match_str_prefix(v, "param")){
	  pcnt = func_param_res_count(v);
	}else if(match_str_prefix(v, "result")){
	  retcnt = func_param_res_count(v);
	}else{
	  // TODO:: Support type indices also
	  break;
	}
      }
      if(pcnt) i++; if(retcnt) i++;
      pushblk_stk(visit_new_blk(stk, end, pcnt, retcnt));
    } else if(op.id == OPCODE_LOOP){
      //else if (str_cmp(op.name, "loop") == 0){
      // TODO:: Figure out how loops param work actually 
      u64 pcnt = 0;
      u64 retcnt = 0;
      for(int it = 1; it < 3; it++){
	if((i+it)>=opcodes.count) break;
	const Str v = slice_inx(opcodes, i+it).name;
	if(match_str_prefix(v, "param")){
	  pcnt = func_param_res_count(v);
	}else if(match_str_prefix(v, "result")){
	  retcnt = func_param_res_count(v);
	}else{
	  // TODO:: Support type indices also
	  break;
	}
      }
      if(pcnt) i++; if(retcnt) i++;
      pushblk_stk(visit_new_blk(stk, i, pcnt, retcnt));
    } else if(op.id == OPCODE_IF){
      //else if (str_cmp(op.name, "if") == 0){
      
      // TODO:: It seems that it also takes in 'blocktype' as another argument
      Wasm_Data comp;
      popstk(comp.du64);

      u64 pcnt = 0;
      u64 retcnt = 0;
      for(int it = 1; it < 3; it++){
	if((i+it)>=opcodes.count) break;
	const Str v = slice_inx(opcodes, i+it).name;
	if(match_str_prefix(v, "param")){
	  pcnt = func_param_res_count(v);
	}else if(match_str_prefix(v, "result")){
	  retcnt = func_param_res_count(v);
	}else{
	  // TODO:: Support type indices also
	  break;
	}
      }


      // Find the end of the block first
      const s64 end1 = find_end_block(opcodes, i, blk_end_cache);
      if(end1 < 0){
	fprintf(stderr, "Unmatched block found for opcode(1) %zu(%.*s)\n",
		i, str_print(slice_inx(opcodes, i).name));
	return 0;
      }

      // For != 0 condition, continue but skip to the 'end'
      // For == 0 condition, if there is 'else' block, continue to that
      //                     else, go to 'end' directly
      s64 end2 = end1;
      //if(str_cmp(slice_inx(opcodes, end1).name, "else") == 0){
      if(slice_inx(opcodes, end1).id == OPCODE_ELSE){
	end2 = find_end_block(opcodes, end1, blk_end_cache);
	if(end2 < 0){
	  fprintf(stderr, "Unmatched block found for opcode(2) %zu(%.*s)\n",
		  end1, str_print(slice_inx(opcodes, i).name));
	  return 0;
	}
      }

      if(pcnt) i++; if(retcnt) i++;
      if(comp.di32 != 0){
	// Execute if block, after jump to 'end2'
	pushblk_stk(visit_new_blk(stk, end2, pcnt, retcnt));
      } else if(end1 == end2) {
	// Directly jump past the 'end' 
	i = end1;
      } else {
	// Jump to the 'end1', set label to 'end2'
	pushblk_stk(visit_new_blk(stk, end2, pcnt, retcnt));
	i = end1;
      }
    } else if(op.id == OPCODE_ELSE){
      //} else if (str_cmp(op.name, "else") == 0){
      // TODO:: Always synchronize the behavior with the if block
      // else when encountered normally, should indicate the end of a 'if' part
      //  so, simply behave as a break opcode for the top block marker
      u64 v;
      popblk_stk(v);
      i = break_old_blk(stk, v);
    } else if (match_str_prefix(op.name, cstr_to_str("br"))) {
      // The first argument is the number of labels(blocks) to pop
      i++; // maybe verify that its not ended yet ??

      // Test against various br types
      bool to_jmp = 0;
      if (op.id == OPCODE_BR){
	//if (str_cmp(op.name, "br") == 0){
	to_jmp = true;
      } else if (op.id == OPCODE_BR_IF){
	//} else if(str_cmp(op.name, "br_if")==0){
	Wasm_Data comp;
	popstk(comp.du64);
	if(comp.di32!=0) to_jmp=true;
      }
      
      if(to_jmp){
	// Simply jump to the label, and pop upto before the label
	// If 'loop' type, also push the label again

	u64 v;
	if(!parse_as_u64(slice_inx(opcodes, i).name, &v)){
	  // TODO:: Also print location of source code
	  fprintf(stderr, "Expected %zu-th opcode to be index, found `%.*s`\n",
		  i, str_print(slice_inx(opcodes, i).name));
	  return 0;
	}

	if(v >= blk_stk->count){
	  fprintf(stderr, "Expected to break out of %zu-th block, when only %zu are present\n", v+1, blk_stk->count);
	  return 0;	  
	}

	u64 label = slice_inx(*blk_stk, blk_stk->count-v-1);
	i = break_old_blk(stk, label);
	// Now do the jump operation
	if(!pop_u64_darray(blk_stk, v+1)){
	  fprintf(stderr, "Couldnt pop from block stack\n");
	  return 0;
	}

	// TODO:: See if break operations can be done by other instruction types
	const Opcode label_op = slice_inx(opcodes, i);
	// For 'loop' type blocks, you need to re-push the loop index
	if (label_op.id == OPCODE_LOOP){
	  //if(str_cmp(label_op, "loop") == 0){
	  // TODO:: Figure out how the parameters work here with loops
	  u64 pcnt = 0;
	  u64 retcnt = 0;
	  for(int it = 0; it < 2; it++){
	    if((i+it)>=opcodes.count) break;
	    const Str v = slice_inx(opcodes, i+it).name;
	    if(match_str_prefix(v, "param")){
	      pcnt = func_param_res_count(v);
	    }else if(match_str_prefix(v, "result")){
	      retcnt = func_param_res_count(v);
	    }else{
	      // TODO:: Support type indices also
	      break;
	    }
	  }
	  if(pcnt) i++; if(retcnt) i++;
	  pushblk_stk(visit_new_blk(stk, i, pcnt, retcnt));
	} 
      }
    } else if(op.id == OPCODE_END){
      //} else if (str_cmp(op.name, "end") == 0){
      // Might need to pop from the stack
      u64 label;
      popblk_stk(label);
      (void)break_old_blk(stk, label);
    } else if(op.id == OPCODE_CALL){
      //} else if (str_cmp(op.name, "call") == 0){
      // The next argument is the function index
      i++; // maybe verify that its not ended yet ??
      u64 finx;
      if(!parse_as_u64(slice_inx(opcodes, i).name, &finx)){
	// TODO:: Also print location of source code
	fprintf(stderr, "Expected %zu-th opcode to be index, found `%.*s`\n",
		i, str_print(slice_inx(opcodes, i).name));
	return 0;
      }
      // Call the function
      const u64 calld_cyc = exec_wasm_fxn(allocr, cxt, finx);
      if(calld_cyc == 0){
	fprintf(stderr, "Failure in calling the function at %zu\n", finx);
	return 0;
      }
      cycles += calld_cyc;
      trace = cxt->trace; // Some fxns can change state of tracing
      trace_vars = cxt->trace_vars; // Some fxns can change state of tracing
    } else if (str_cmp(op.name, "i32.xor") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = p1.di32 ^ p0.di32; // TODO:: Find out if the expected behavior matches
      pushstk(r.du64);
    } else if (str_cmp(op.name, "i32.div_u") == 0){
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
    } else if (str_cmp(op.name, "i32.gt_s") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = (s32)(p1.di32 > p0.di32);
      pushstk(r.du64);
    } else if (str_cmp(op.name, "i32.gt_u") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      // TODO:: Need to ensure that this is alright
      r.di32 = (s32)((u32)p1.di32 > (u32)p0.di32);
      pushstk(r.du64);
    } else if (str_cmp(op.name, "i32.ge_s") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = (s32)(p1.di32 >= p0.di32);
      pushstk(r.du64);
    } else if (str_cmp(op.name, "i32.ge_u") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      // TODO:: Need to ensure that this is alright
      r.di32 = (s32)((u32)p1.di32 >= (u32)p0.di32);
      pushstk(r.du64);
    } else if (str_cmp(op.name, "i32.lt_s") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = (s32)(p1.di32 < p0.di32);
      pushstk(r.du64);
    } else if (str_cmp(op.name, "i32.lt_u") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      // TODO:: Need to ensure that this is alright
      r.di32 = (s32)((u32)p1.di32 < (u32)p0.di32);
      pushstk(r.du64);
    } else if (str_cmp(op.name, "i32.le_s") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = (s32)(p1.di32 <= p0.di32);
      pushstk(r.du64);
    } else if (str_cmp(op.name, "i32.le_u") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      // TODO:: Need to ensure that this is alright
      r.di32 = (s32)((u32)p1.di32 <= (u32)p0.di32);
      pushstk(r.du64);
    } else if (str_cmp(op.name, "i32.eqz") == 0){
      Wasm_Data p = {0},  r = {0};
      popstk(p.du64);
      r.di32 = (s32)(p.di32 == 0);
      pushstk(r.du64);
    } else if (match_str_suffix(op.name, cstr_to_str("load")) ||
	       match_str_suffix(op.name, cstr_to_str("store"))||
	       match_str_suffix(op.name, cstr_to_str("load8_u"))||
	       match_str_suffix(op.name, cstr_to_str("load8_s"))||
	       match_str_suffix(op.name, cstr_to_str("store8"))){
      Wasm_Data val; // For store, these are first popped from stack

      const bool to_load = match_str_suffix(op.name, cstr_to_str("load")) ||
	match_str_suffix(op.name, cstr_to_str("load8_u"))||
	match_str_suffix(op.name, cstr_to_str("load8_s"))||
	match_str_suffix(op.name, cstr_to_str("store8"));
      if(!to_load) popstk(val.du64);

      // Common part of store and load
      // In this case, lets pop stack value first before seeing arguments
      Wasm_Data base = {0};
      popstk(base.du64);
      s64 offset = base.di32; //The memory address to load/store to

      // Do twice, once expecting offset=, another expecting alignment=
      for_range(size_t, ii, 0, 2){
	if((i+1)<opcodes.count){
	  Str maybearg = slice_inx(opcodes, i+1).name;
	  if(match_str_prefix(maybearg, cstr_to_str("offset="))){
	    maybearg = str_slice(maybearg, strlen("offset="), maybearg.count);
	    u64 off;
	    if(!parse_as_u64(maybearg, &off)){
	      fprintf(stderr, "Expected `offset=<u32>`, found `offset=%.*s`\n",
		      str_print(maybearg));
	      return 0;
	    }
	    // Adding the constant offset 
	    offset += off;
	    i++;
	  }else if(match_str_prefix(maybearg, cstr_to_str("align="))){
	    // TODO:: Someday care about alignment
	    i++;
	  }else {break;}
	}
      }
      void* pdata=nullptr;
      size_t sz_dt=0;
#define fill_ptr_n_size(prefix, item)			\
      do{						\
	if(match_str_prefix(op.name, prefix)){		\
	  pdata = &val.item;				\
	  sz_dt = sizeof(val.item);			\
	}						\
      }while(0)
      fill_ptr_n_size("i32", di32);
      fill_ptr_n_size("u32", du32);
      fill_ptr_n_size("i64", di64);
      fill_ptr_n_size("u64", du64);
      fill_ptr_n_size("f32", df32);
      fill_ptr_n_size("f64", df64);
      // TODO:: Handle when none of the values match
      if(match_str_suffix(op.name, "8_u")||match_str_suffix(op.name, "8_s")||match_str_suffix(op.name, "8")){
	sz_dt = 1;
      }
#undef fill_ptr_n_size

      if(to_load) {
	(void)memory_rgn_read(mem, offset, sz_dt, pdata);
	if(match_str_suffix(op.name, "8_u")){
	  val.du64 = val.du64 & 0xff;
	}
	if(match_str_suffix(op.name, "8_s")){
	  val.du64 = val.du64 & 0xff;
	  // Need to sign extend
	  val.di64 = val.di8;
	}
	pushstk(val.du64);
      } else {
	if(match_str_suffix(op.name, "8_s")){
	  // Just consider bitpattern (wrap x) opcode
	  s8 v = val.di32;
	  val.du64 = 0;
	  val.di8 = v;
	}
	(void)memory_rgn_write(mem, offset, sz_dt, pdata);
      }
    } else if (str_cmp(op.name, "drop") == 0){
      Wasm_Data v = {0};
      popstk(v.du64);
    } else if (str_cmp(op.name, "return") == 0){
      break; // Maybe there is a better solution
    } else {
      fprintf(stderr, "TODO:: Implement opcode `%.*s`\n", str_print(op.name));
      return 0;
    }
    cycles++;

    if(trace){
      printf_stk(*stk, "After opcode %zu (%.*s) : ",
		 i, str_print(op.name));
      printf("\n");
      if(trace_vars){
	printf("  { ");
	for_slice(vars, i) wasm_data_print(slice_inx(vars, i));
	printf("}\n");
	if(match_str_prefix(op.name, cstr_to_str("global"))){
	  printf("    Globals: ");
	  for_slice(cxt->globals, i) wasm_data_print(slice_inx(cxt->globals, i));
	  printf("\n");
	}
      }
      if(trace_mem){ 
	if(match_str_suffix(op.name, cstr_to_str("load")) || 
	   match_str_suffix(op.name, cstr_to_str("store")) ||
	   match_str_suffix(op.name, cstr_to_str("load8_u"))||
	   match_str_suffix(op.name, cstr_to_str("load8_s")) ||
	   match_str_suffix(op.name, cstr_to_str("store8")) ||
	   match_str_prefix(op.name, cstr_to_str("memory"))){
	  printf("    ");
	  memory_rgn_dump(mem);
	  printf("\n");
	}
      }

    }
    
  }
  if(trace){
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
      case WASM_DATA_F64: {printf("%lf", (float)d.df64); break;}
      case WASM_DATA_U64: {printf("%llu", (long long unsigned)d.du64); break;}
      }
      printf(") ");
    }
    printf("}\n");
  }
#undef pushblk_stk
#undef popblk_stk
#undef pushstk
#undef popstk
  return cycles;
}
