// This is intentionally not done, this file is supposed to be included only once
// knowingly as part of some 'unity build' (for now)
// The same reason (also lazy) is why no extra includes are done here
// #pragma once

typedef struct Wasm_Data Wasm_Data;
struct Wasm_Data {
  enum {
    WASM_DATA_VOID=0, // Cannot actually use this to store items
    WASM_DATA_I8,
    WASM_DATA_I32,
    WASM_DATA_U32,
    WASM_DATA_F32,
    WASM_DATA_F64,
    WASM_DATA_U64,
    WASM_DATA_I64,
  } tag;
  union{
    s8 di8;
    s32 di32;
    u32 du32;
    f32 df32;
    f64 df64;
    u64 du64;
    u64 di64;
  };
};
// Just in case, to prevent any undefined behavior
static_assert(sizeof(u64) == sizeof(Wasm_Data)-offsetof(Wasm_Data, di8));
static_assert(sizeof(u64) == sizeof(Wasm_Data)-offsetof(Wasm_Data, di32));
static_assert(sizeof(u64) == sizeof(Wasm_Data)-offsetof(Wasm_Data, du32));
static_assert(sizeof(u64) == sizeof(Wasm_Data)-offsetof(Wasm_Data, df32));
static_assert(sizeof(u64) == sizeof(Wasm_Data)-offsetof(Wasm_Data, df64));
static_assert(sizeof(u64) == sizeof(Wasm_Data)-offsetof(Wasm_Data, du64));
static_assert(sizeof(u64) == sizeof(Wasm_Data)-offsetof(Wasm_Data, di64));
const Cstr wasm_names[] = {
  [WASM_DATA_I8] = "i8",
  [WASM_DATA_I32] = "i32",
  [WASM_DATA_U32] = "u32",
  [WASM_DATA_F32] = "f32",
  [WASM_DATA_F64] = "f64",
  [WASM_DATA_U64] = "u64",
  [WASM_DATA_I64] = "i64",
  [WASM_DATA_VOID] = "void",
};
DEF_SLICE(Wasm_Data);

bool wasm_data_is_signed(Wasm_Data d){
  return d.tag == WASM_DATA_I8 || d.tag == WASM_DATA_I32 || d.tag == WASM_DATA_I64;
}
bool wasm_data_is_unsigned(Wasm_Data d){
  return d.tag == WASM_DATA_U32 || d.tag == WASM_DATA_U64;
}
bool wasm_data_is_floating(Wasm_Data d){
  return d.tag == WASM_DATA_F32 || d.tag == WASM_DATA_F64;
}

Wasm_Data wasm_data_by_type(const Str type_name){
  Wasm_Data d = {0};
  for_range(size_t, i, 0, _countof(wasm_names)){
    if(str_cstr_cmp(type_name, wasm_names[i]) == 0){
      d.tag = i;
    }
  }
  return d;
}
void wasm_data_print(Wasm_Data d){
  printf("%s(", wasm_names[d.tag]);
  switch(d.tag){
  case WASM_DATA_I8: {printf("%d", (int)d.di8); break;}
  case WASM_DATA_I32: {printf("%ld", (long)d.di32); break;}
  case WASM_DATA_U32: {printf("%lu", (unsigned long)d.du32); break;}
  case WASM_DATA_F32: {printf("%f", (float)d.df32); break;}
  case WASM_DATA_F64: {printf("%lf", (float)d.df64); break;}
  case WASM_DATA_U64: {printf("%llu", (long long unsigned)d.du64); break;}
  case WASM_DATA_I64: {printf("%lld", (long long)d.di64); break;}
  }
  printf(") ");
}



// Load a partiucular data section
bool load_data_to_memory(Memory_Region* mem, Wasm_Data_Slice globals, const Data* data_sec){
  assert(mem && data_sec && data_sec->offset_expr /* Only active data sections */);

  // Parse the offset expression
  const Parse_Node* off_expr = data_sec->offset_expr;
  bool is_global_inx = false;
  if(str_cstr_cmp(off_expr->data, "i32.const") == 0){}
  else if(str_cstr_cmp(off_expr->data, "global.get") == 0){
    is_global_inx = true;
  } else {
    fprintf(stderr, "Expected data section offset expression to be of `i32.const` or `global.get`"
	    ", found `%.*s` instead\n", str_print(off_expr->data));
    return false;
  }
  if(off_expr->children.count != 1 ||
     slice_first(off_expr->children)->children.count != 0){
    fprintf(stderr, "Expected a data section offset expression only consisting a"
	    "single i32.const/index value, found `%.*s` instead",
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
  if(is_global_inx){
    if(slice_inx(globals, off).tag != WASM_DATA_I32){
      fprintf(stderr, "Expected i32 type global variable for initializing the data segment, found a global of type %s\n", wasm_names[slice_inx(globals, off).tag]);
      return false;
    }
    // TODO:: check for negative values
    off = slice_inx(globals, off).di32;
  }

  
  if(!memory_rgn_write(mem, off, data_sec->raw_bytes.count, data_sec->raw_bytes.data)){
    return false;
  }
  printf("... Initialized data section at offset %u with length %u\n",
	 (unsigned)off, (unsigned)data_sec->raw_bytes.count);
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

  // TODO:: When doing multithreading, linear memory might need to be made a pointer
 // TODO:: Somehow detect stack overflow or change the position of stack
  Memory_Region mem;
  Wasm_Fxn_Slice fxns;
  u64_Darray fxn_table; // Maybe u32 is good enough ?
  // TODO:: Implement immutability
  Wasm_Data_Slice globals;

  // Runtime metadata
  bool trace;
  bool trace_vars;
  bool trace_mem;
  u64 cycle_limit;
  bool step_into; // Need to be triggered by something in the code
};

Wasm_Fxn_Ptr wasm_fxn_executor; // Used to make calls to a wasm fxn, expects param in stk
Wasm_Fxn_Ptr dummy_import_fxn; // To be used everywhere if import not fo

// Only for initial testing
Wasm_Fxn_Ptr wasm_extern_printint;
Wasm_Fxn_Ptr wasm_extern_printhex;
Wasm_Fxn_Ptr wasm_begin_tracing;
Wasm_Fxn_Ptr wasm_end_tracing;
Wasm_Fxn_Ptr wasm_begin_stepping;

u64 exec_wasm_fxn(Alloc_Interface allocr, Exec_Context* cxt, size_t finx);
// TODO:: figure out some other way of resolving the imports maybe
Exec_Context init_exec_context(Alloc_Interface allocr, const Module* mod){
  // Allocate the memory region, stk, blk_stk
  Exec_Context cxt = {
    .mod=mod,
    .stk={.allocr=allocr},
    .blk_stk={.allocr=allocr},
    .fxn_table={.allocr=allocr},
    .trace = false,
    .trace_vars = false,
    .trace_mem = false,
    .cycle_limit = 0,
    .step_into = false,
  };
  cxt.mem = memory_rgn_init(allocr);

  // Analyze the imports and fill up fxn slice
  size_t fxn_cnt = mod->funcs.count;
  size_t glbl_cnt = mod->globals.count;
  for_slice(mod->imports, i){
    if(mod->imports.data[i].import_type == FUNCTION_IMPORT_TYPE) fxn_cnt++;
    if(mod->imports.data[i].import_type == GLOBAL_IMPORT_TYPE) glbl_cnt++;
  }
  cxt.fxns = SLICE_ALLOC(allocr, Wasm_Fxn, fxn_cnt);
  assert(cxt.fxns.data);
  fxn_cnt = 0;
  cxt.globals = SLICE_ALLOC(allocr, Wasm_Data, glbl_cnt);
  assert(cxt.globals.data);
  glbl_cnt = 0;

  // First of all, find the imported memory and deduce the data segment max size
  // Then formulate the memory model
  u64 pages_needed = 0;
  for_slice(mod->imports, i){
    if(mod->imports.data[i].import_type == MEMORY_IMPORT_TYPE){
      // Expect the data to be only of a single unit of no of pages
      // TODO:: Validate this
      (void)parse_as_u64(mod->imports.data[i].import_details->data, &pages_needed);
      break;
    }
  }
  if(pages_needed == 0){
    fprintf(stderr, "Pages needed = 0, or the memory is not imported\n");
    // TODO:: Someday return a proper error here
    return cxt;
  }
  // My interpreter memory model:
  // <-- stack size --><-- pages_needed -->                   v--(2 ^ 31 -ve i32 range)
  // [---- Stack -----][------ Data ------][--- Heap ---].....[---- Forbidden zone ----]
  // If stack overflow occurs, the 'i32' index will go into forbidden zone first
  // We can either 'allow' it or trap it
  // In the future though, when multithreading will be allowed, some
  //    better solution might be needed (hmm, multiple memories in this case?)
  //    (or since -ve pages are forbidden, we can 'remap' the memory indices)

  const u32 stack_pages = 2;
  const u64 PAGE_SIZE = ((u64)1 << (u64)16);
  struct {
    Cstr name;
    u64 value; // The value if not compatible is to be made so first
    bool imported; // If already true, then its 'optional'
  } imports[] = {
    {"__stack_pointer", (u64)2*PAGE_SIZE},
    {"__memory_base", (u64)2*PAGE_SIZE},
    {"__table_base", 0, true}, // TODO:: Understand tables
    {"__heap_base", (u64)(2+pages_needed)*PAGE_SIZE, true},
    {"__heap_end",  (u64)(pages_needed+50) * PAGE_SIZE, true},
  };
  const size_t glob_imprt_cnt = _countof(imports);

  for_slice(mod->imports, i){
    if(mod->imports.data[i].import_type == GLOBAL_IMPORT_TYPE){
      // All global imports must be known
      const Import gi = mod->imports.data[i];
      // What is the type?   (import "env" "__stack_pointer" (global (;0;) (mut i32)))
      Str typename = gi.import_details->data;
      if(str_cmp(gi.import_details->data, "mut") == 0 &&
	 gi.import_details->children.count == 1){
	typename = slice_first(gi.import_details->children)->data;
      }

      // Ignore the 'module name' for the import for now
      for_range(size_t, j, 0, glob_imprt_cnt){
	if(str_cmp(gi.self_name, imports[j].name) == 0){
	  slice_inx(cxt.globals, glbl_cnt) = wasm_data_by_type(typename);
	  slice_inx(cxt.globals, glbl_cnt).du64 = imports[j].value;
	  glbl_cnt++;
	  imports[j].imported = true;
	  goto found_global_import;
	}
      }
      fprintf(stderr, "Importing the global named `%.*s`.`%.*s` is not supported\n",
	      str_print(gi.mod_name), str_print(gi.self_name));
      return cxt;
    found_global_import:
    }
  }
  // Now check if all required imports were imported
  {
    bool all_imported = true;
    for_range(size_t, j, 0, glob_imprt_cnt){
      if(!imports[j].imported){
	fprintf(stderr, "The required symbol %s was not imported\n", imports[j].name);
	all_imported = false;
      }
    }
    if(!all_imported) return cxt;
  }
  // TODO:: Import the table also what is that and implement it
  
  // Initialize declared globals
  if(mod->globals.count > 0){
    for_slice(mod->globals, i){
      const Global* glbl = mod->globals.data + i;
      // TODO:: Implement immutability someday somehow
      slice_inx(cxt.globals, glbl_cnt) = wasm_data_by_type(glbl->valtype->data);
      if(!match_str_suffix(glbl->expr->data, cstr_to_str(".const"))){
	fprintf(stderr, "Only const type global expressions supported, found `%.*s`\n",
		str_print(glbl->expr->data));
	// TODO:: Maybe stop working, for now its initialized to 0
      } else {
	Wasm_Data* d = cxt.globals.data + i + glbl_cnt;
	glbl_cnt++;
	// TODO:: Separate this part
	if(wasm_data_is_signed(*d)){
	  if(!parse_as_s64(slice_first(glbl->expr->children)->data, &d->di64)){
	    fprintf(stderr, "Couldnt parse `%.*s` as signed integer\n",
		    str_print(slice_first(glbl->expr->children)->data));
	  } else {
	    if(d->tag == WASM_DATA_I32){
	      d->di32 = d->di64;
	    }
	  }
	}else if(wasm_data_is_unsigned(*d)){
	  if(!parse_as_s64(slice_first(glbl->expr->children)->data, &d->du64)){
	    fprintf(stderr, "Couldnt parse `%.*s` as unsigned integer\n",
		    str_print(slice_first(glbl->expr->children)->data));
	  } else {
	    if(d->tag == WASM_DATA_U32){
	      d->du32 = d->du64;
	    }
	  }
	}else{
	  fprintf(stderr, "We dont support wasm data global for %s\n",
		  wasm_names[d->tag]);
	  return cxt;
	}
      }
    }
  }


  // Initialize the data segments
  for_slice(mod->data_sections, i){
    printf("Initializing the memory with data section %zu...\n", i);
    if(!load_data_to_memory(&cxt.mem, cxt.globals, mod->data_sections.data+i)){
      printf("   ... Loading data section at %zu failed!!!\n", i);
    }
  }

  // Initialize imported functions
  for_slice(mod->imports, i){
    if(mod->imports.data[i].import_type == FUNCTION_IMPORT_TYPE){
      // Hardcode the 'printint' fxn for now
      if(str_cstr_cmp(mod->imports.data[i].self_name, "printint") == 0){
	slice_inx(cxt.fxns, fxn_cnt).fptr = wasm_extern_printint;
      }
      // Hardcode the 'printhex' fxn for now
      else if(str_cstr_cmp(mod->imports.data[i].self_name, "printhex") == 0){
	slice_inx(cxt.fxns, fxn_cnt).fptr = wasm_extern_printhex;
      }
      // Hardcode the '__Begin_Tracing' fxn for now
      else if(str_cstr_cmp(mod->imports.data[i].self_name, "__Begin_Tracing") == 0){
	slice_inx(cxt.fxns, fxn_cnt).fptr = wasm_begin_tracing;
      }
      // Hardcode the '__End_Tracing' fxn for now
      else if(str_cstr_cmp(mod->imports.data[i].self_name, "__End_Tracing") == 0){
	slice_inx(cxt.fxns, fxn_cnt).fptr = wasm_end_tracing;
      }
      // Hardcode the '__Begin_Stepping' fxn for now
      else if(str_cstr_cmp(mod->imports.data[i].self_name, "__Begin_Stepping") == 0){
	slice_inx(cxt.fxns, fxn_cnt).fptr = wasm_begin_stepping;
      }
      else {
	// Manually fill them for now
	slice_inx(cxt.fxns, fxn_cnt).fptr = dummy_import_fxn;
	slice_inx(cxt.fxns, fxn_cnt).data = mod->imports.data+i;
      }
      fxn_cnt++;
    }
  }
  // Initialize declared functions
  for_slice(mod->funcs, i){
    slice_inx(cxt.fxns, fxn_cnt).fptr = wasm_fxn_executor;
    slice_inx(cxt.fxns, fxn_cnt).data = mod->funcs.data + i;
    fxn_cnt++;    
  }

  // Initialize the function table
  for_slice(mod->unknowns, i){
    // TODO:: Remove this parsing
    Parse_Node* nod = slice_inx(mod->unknowns, i);
    if(str_cmp(nod->data, "elem") == 0){
      // Expect the first entry to be global.get (expect -shared)
      if(nod->children.count != 3){
	fprintf(stderr, "Expected exactly 3 children for `elem`, found %zu\n",
		nod->children.count);
	// TODO:: Signal error here somehow
	assert(false);
      }

      Parse_Node *a = slice_inx(nod->children, 0),
	*b = slice_inx(nod->children, 1),
	*c = slice_inx(nod->children, 2);
      // Expect the first to be global.get,
      // TODO:: Care about the table index also
      if(str_cmp(a->data, "global.get") != 0){
	fprintf(stderr, "Currently only global.get type of exports are "
		"expected, found `%.*s`\n", str_print(a->data));
	// TODO:: Signal error here somehow
	assert(false);
      }

      // Expect the second one to be 'func' type
      if(str_cmp(b->data, "func") != 0){
	fprintf(stderr, "Expected only a 'func' type table referenced, found "
		"`%.*s`\n", str_print(b->data));
	// TODO:: Signal error here somehow
	assert(false);	
      }

      if(c->children.count != 0){
	fprintf(stderr, "Expected no children for function index, found %zu\n",
		c->children.count);
	// TODO:: Signal error here somehow
	assert(false);
      }

      u64 finx;
      if(!parse_as_u64(c->data, &finx)){
	fprintf(stderr, "Expected unsigned integer for function index, found `%.*s`\n",
		str_print(c->data));
	// TODO:: Signal error here somehow
	assert(false);
      }

      if(!push_u64_darray(&cxt.fxn_table, finx)){
	fprintf(stderr, "Could not push to function table\n");
	// TODO:: Signal error here somehow
	assert(false);
      }
      printf(".... Filled function %lu at function table index %lu\n",
	     (unsigned long)finx, (unsigned long)(cxt.fxn_table.count -1));
      
    }
  }

  // TODO:: Run the start function here
  //        wasm std says that no extern things are accessible in the start fxn
  //        but for now the print fxns are made accessible since hardcoded
  for_slice(mod->unknowns, i){
    Parse_Node* nod = slice_inx(mod->unknowns, i);
    if(str_cmp(nod->data, "start") == 0){
      // TODO:: Assert the type to have no args and no ret vals maybe
      if(nod->children.count != 1){
	fprintf(stderr, "Invalid start declaration\n");
      } else {
	u64 finx = 0;
	if(!parse_as_u64(slice_first(nod->children)->data, &finx)){
	  fprintf(stderr, "Failed to get fxn %zu for `start`\n", (size_t)finx);
	  continue;
	}
	printf("Running start function at %zu....\n", finx);
	//const bool prev_trace = cxt.trace;
	//const bool prev_trace_vars = cxt.trace_vars;
	//const bool prev_trace_mem = cxt.trace_mem;
	//cxt.trace = cxt.trace_vars = cxt.trace_mem = true;
	u64 cycles = exec_wasm_fxn(allocr, &cxt, finx);
	//cxt.trace = prev_trace;
	//cxt.trace_vars = prev_trace_vars;
	//cxt.trace_mem = prev_trace_mem;
	if(cycles == 0){
	  printf("Sus..., cycles = 0 found when running start function\n");
	}
	printf(".... Finished function at %zu\n", finx);
      }
    }
  }

  // If available, also run the exports for ctors, reloc, initialize
  s32 reloc_fn = mod_find_export(mod, "__wasm_apply_data_relocs", FUNCTION_EXPORT_TYPE);
  if(reloc_fn >= 0){
    printf("Running __wasm_apply_data_relocs function at %d....\n", reloc_fn);
    //const bool prev_trace = cxt.trace;
    //const bool prev_trace_vars = cxt.trace_vars;
    //const bool prev_trace_mem = cxt.trace_mem;
    //cxt.trace = cxt.trace_vars = cxt.trace_mem = true;
    u64 cycles = exec_wasm_fxn(allocr, &cxt, reloc_fn);
    //cxt.trace = prev_trace;
    //cxt.trace_vars = prev_trace_vars;
    //cxt.trace_mem = prev_trace_mem;
    printf(".... Finished function at %d with %zu cycles\n", reloc_fn, (size_t)cycles);
  }

  return cxt;
}

void deinit_exec_context(Alloc_Interface allocr, Exec_Context* cxt){
  if(!cxt) return;
  SLICE_FREE(allocr, cxt->fxns);
  (void)resize_u64_darray(&cxt->fxn_table, 0);
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
u64 wasm_begin_tracing(Alloc_Interface , Exec_Context* cxt, void* ){
  cxt->trace = true;
  cxt->trace_vars = true;
  printf("    Globals: ");
  for_slice(cxt->globals, i) wasm_data_print(slice_inx(cxt->globals, i));
  printf("\n");
  return 1;
}
u64 wasm_end_tracing(Alloc_Interface , Exec_Context* cxt, void* ){
  cxt->trace = false;
  cxt->trace_vars = false;
  return 1;
}
u64 wasm_begin_stepping(Alloc_Interface , Exec_Context* cxt, void* ){
  cxt->step_into = true;
  return 1;
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
  if(cnt != 0) {
    return -1;
  }
  return inx; // Returns the index of the end block, need to do +1 for continuation
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

  // TODO:: Care about the result ?
  // Go through the opcodes?
  for_slice(opcodes, i){

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
	     i, str_print(slice_inx(opcodes, i)), help);
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
		printf("%.*s  ", str_print(slice_inx(opcodes, i)));
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
	  printf("\nOpcode %zu : `%.*s`\n%s\n", i, str_print(slice_inx(opcodes, i)), help);
	  break;
	}
	}
      }
    }

    if(cycle_limit && cycles > cycle_limit){
      printf("reached > %zu cycles, quitting for now\n", (size_t)cycle_limit);
      return 0;
    }

    
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
    } else if(str_cstr_cmp(op, "global.get") == 0){
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
      if(inx >= cxt->globals.count){
	// TODO:: Also print location of source code
	fprintf(stderr, "Tried to access %zu-th global when only %zu globals are present\n",
		inx, cxt->globals.count);
	return 0;
	
      }
      // Arguments are validated already
      pushstk(slice_inx(cxt->globals, inx).du64);
    } else if (str_cstr_cmp(op, "global.set") == 0){
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
      if(inx >= cxt->globals.count){
	// TODO:: Also print location of source code
	fprintf(stderr, "Tried to access %zu-th global when only %zu globals are present\n",
		inx, cxt->globals.count);
	return 0;
      }
      // Arguments are validated already
      // Get the value from stack and then pop it
      popstk(slice_inx(cxt->globals,inx).du64);
    } else if (str_cstr_cmp(op, "i32.const") == 0){
      i++; // maybe verify that its not ended yet ??
      s64 v;
      if(!parse_as_s64(slice_inx(opcodes, i), &v)){
	// TODO:: Also print location of source code
	fprintf(stderr, "Expected %zu-th opcode to be i32 literal, found `%.*s`\n",
		i, str_print(slice_inx(opcodes, i)));
	return 0;
      }
      Wasm_Data p = {0};
      p.di32 = v;
      pushstk(p.du64);
    } else if (str_cstr_cmp(op, "i64.const") == 0){
      i++; // maybe verify that its not ended yet ??
      s64 v;
      if(!parse_as_s64(slice_inx(opcodes, i), &v)){
	// TODO:: Also print location of source code
	fprintf(stderr, "Expected %zu-th opcode to be i64 literal, found `%.*s`\n",
		i, str_print(slice_inx(opcodes, i)));
	return 0;
      }
      Wasm_Data p = {0};
      p.di64 = v;
      pushstk(p.du64);
    } else if (str_cstr_cmp(op, "f64.const") == 0){
      i++; // maybe verify that its not ended yet ??
      f64 v;
      if(!parse_as_f64(slice_inx(opcodes, i), &v)){
	// TODO:: Also print location of source code
	fprintf(stderr, "Expected %zu-th opcode to be f64 literal, found `%.*s`\n",
		i, str_print(slice_inx(opcodes, i)));
	return 0;
      }
      Wasm_Data p = {0};
      p.df64 = v;
      pushstk(p.du64);
    } else if (match_str_suffix(op, cstr_to_str("load")) ||
	       match_str_suffix(op, cstr_to_str("store"))||
	       match_str_suffix(op, cstr_to_str("load8_u"))||
	       match_str_suffix(op, cstr_to_str("load8_s"))||
	       match_str_suffix(op, cstr_to_str("store8"))){
      Wasm_Data val; // For store, these are first popped from stack

      const bool to_load = match_str_suffix(op, cstr_to_str("load")) ||
	match_str_suffix(op, cstr_to_str("load8_u"))||
	match_str_suffix(op, cstr_to_str("load8_s"))||
	match_str_suffix(op, cstr_to_str("store8"));
      if(!to_load) popstk(val.du64);

      // Common part of store and load
      // In this case, lets pop stack value first before seeing arguments
      Wasm_Data base = {0};
      popstk(base.du64);
      s64 offset = base.di32; //The memory address to load/store to

      // Do twice, once expecting offset=, another expecting alignment=
      for_range(size_t, ii, 0, 2){
	if((i+1)<opcodes.count){
	  Str maybearg = slice_inx(opcodes, i+1);
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
	if(match_str_prefix(op, prefix)){		\
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
      if(match_str_suffix(op, "8_u")||match_str_suffix(op, "8_s")||match_str_suffix(op, "8")){
	sz_dt = 1;
      }
#undef fill_ptr_n_size

      if(to_load) {
	(void)memory_rgn_read(mem, offset, sz_dt, pdata);
	if(match_str_suffix(op, "8_u")){
	  val.du64 = val.du64 & 0xff;
	}
	if(match_str_suffix(op, "8_s")){
	  val.du64 = val.du64 & 0xff;
	  // Need to sign extend
	  val.di64 = val.di8;
	}
	pushstk(val.du64);
      } else {
	if(match_str_suffix(op, "8_s")){
	  // Just consider bitpattern (wrap x) opcode
	  s8 v = val.di32;
	  val.du64 = 0;
	  val.di8 = v;
	}
	(void)memory_rgn_write(mem, offset, sz_dt, pdata);
      }
    } else if (str_cstr_cmp(op, "memory.fill") == 0){
      Wasm_Data ptr = {0}, ch = {0}, len = {0};
      popstk(len.du64);
      popstk(ch.du64);
      popstk(ptr.du64);

      //printf("$$$$$$$ Filling the memory at %x with %d for %d\n",
      //ptr.di32, ch.di32, len.di32);

      memory_rgn_memset(mem, ptr.di32, len.di32, ch.di32);
    } else if (str_cstr_cmp(op, "memory.copy") == 0){
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
    } else if (str_cstr_cmp(op, "f64.convert_i32_s") == 0){
      Wasm_Data p0 = {0}, r = {0};
      popstk(p0.du64);
      r.df64 = p0.di32;
      pushstk(r.du64);
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
    } else if (str_cstr_cmp(op, "f64.add") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.df64 = p1.df64 + p0.df64;
      pushstk(r.du64);
    } else if (str_cstr_cmp(op, "f64.neg") == 0){
      Wasm_Data p0 = {0}, r = {0};
      popstk(p0.du64);
      r.df64 = -p0.df64;
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
    } else if (str_cstr_cmp(op, "f64.div") == 0){
      Wasm_Data p_divr = {0}, p_divd = {0}, r = {0};
      popstk(p_divr.du64); popstk(p_divd.du64);
      // For floating points, div by 0 is not a problem
      r.df64 = p_divd.df64 / p_divr.df64;
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
    } else if (str_cstr_cmp(op, "i32.ge_u") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      // TODO:: Need to ensure that this is alright
      r.di32 = (s32)((u32)p1.di32 >= (u32)p0.di32);
      pushstk(r.du64);
    } else if (str_cstr_cmp(op, "i32.lt_s") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = (s32)(p1.di32 < p0.di32);
      pushstk(r.du64);
    } else if (str_cstr_cmp(op, "i32.lt_u") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      // TODO:: Need to ensure that this is alright
      r.di32 = (s32)((u32)p1.di32 < (u32)p0.di32);
      pushstk(r.du64);
    } else if (str_cstr_cmp(op, "f64.lt") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = p1.df64 < p0.df64;
      pushstk(r.du64);
    } else if (str_cstr_cmp(op, "i32.le_s") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = (s32)(p1.di32 <= p0.di32);
      pushstk(r.du64);
    } else if (str_cstr_cmp(op, "i32.le_u") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      // TODO:: Need to ensure that this is alright
      r.di32 = (s32)((u32)p1.di32 <= (u32)p0.di32);
      pushstk(r.du64);
    } else if (str_cstr_cmp(op, "i32.ne") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = p1.di32 != p0.di32;
      pushstk(r.du64);
    } else if (str_cstr_cmp(op, "i32.eq") == 0){
      Wasm_Data p0 = {0}, p1 = {0}, r = {0};
      popstk(p0.du64); popstk(p1.du64);
      r.di32 = p1.di32 == p0.di32;
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
    } else if (str_cstr_cmp(op, "drop") == 0){
      Wasm_Data v = {0};
      popstk(v.du64);
    } else if (str_cstr_cmp(op, "block") == 0){
      // TODO:: It seems that it also takes in 'blocktype' as another argument
      s64 end = find_end_block(opcodes, i);
      if(end < 0){
	fprintf(stderr, "Unmatched block found for opcode %zu\n", i);
	return 0;
      }
      u64 pcnt = 0;
      u64 retcnt = 0;
      for(int it = 1; it < 3; it++){
	if((i+it)>=opcodes.count) break;
	const Str v = slice_inx(opcodes, i+it);
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
    } else if (str_cstr_cmp(op, "loop") == 0){
      // TODO:: Figure out how loops param work actually 
      u64 pcnt = 0;
      u64 retcnt = 0;
      for(int it = 1; it < 3; it++){
	if((i+it)>=opcodes.count) break;
	const Str v = slice_inx(opcodes, i+it);
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
    } else if (str_cstr_cmp(op, "if") == 0){
      // TODO:: It seems that it also takes in 'blocktype' as another argument
      Wasm_Data comp;
      popstk(comp.du64);

      u64 pcnt = 0;
      u64 retcnt = 0;
      for(int it = 1; it < 3; it++){
	if((i+it)>=opcodes.count) break;
	const Str v = slice_inx(opcodes, i+it);
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
      const s64 end1 = find_end_block(opcodes, i);
      if(end1 < 0){
	fprintf(stderr, "Unmatched block found for opcode(1) %zu(%.*s)\n",
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
	  fprintf(stderr, "Unmatched block found for opcode(2) %zu(%.*s)\n",
		  end1, str_print(slice_inx(opcodes, i)));
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
    } else if (str_cstr_cmp(op, "else") == 0){
      // TODO:: Always synchronize the behavior with the if block
      // else when encountered normally, should indicate the end of a 'if' part
      //  so, simply behave as a break opcode for the top block marker
      u64 v;
      popblk_stk(v);
      i = break_old_blk(stk, v);
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

	u64 label = slice_inx(*blk_stk, blk_stk->count-v-1);
	i = break_old_blk(stk, label);
	// Now do the jump operation
	if(!pop_u64_darray(blk_stk, v+1)){
	  fprintf(stderr, "Couldnt pop from block stack\n");
	  return 0;
	}

	// TODO:: See if break operations can be done by other instruction types
	const Str label_op = slice_inx(opcodes, i);
	// For 'loop' type blocks, you need to re-push the loop index
	if(str_cstr_cmp(label_op, "loop") == 0){
	  // TODO:: Figure out how the parameters work here with loops
	  u64 pcnt = 0;
	  u64 retcnt = 0;
	  for(int it = 0; it < 2; it++){
	    if((i+it)>=opcodes.count) break;
	    const Str v = slice_inx(opcodes, i+it);
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
    } else if (str_cstr_cmp(op, "end") == 0){
      // Might need to pop from the stack
      u64 label;
      popblk_stk(label);
      (void)break_old_blk(stk, label);
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
      trace = cxt->trace; // Some fxns can change state of tracing
      trace_vars = cxt->trace_vars; // Some fxns can change state of tracing
    } else if (str_cstr_cmp(op, "return") == 0){
      break; // Maybe there is a better solution
    } else {
      fprintf(stderr, "TODO:: Implement opcode `%.*s`\n", str_print(op));
      return 0;
    }
    cycles++;

    if(trace){
      printf_stk(*stk, "After opcode %zu (%.*s) : ",
		 i, str_print(op));
      printf("\n");
      if(trace_vars){
	printf("  { ");
	for_slice(vars, i) wasm_data_print(slice_inx(vars, i));
	printf("}\n");
	if(match_str_prefix(op, cstr_to_str("global"))){
	  printf("    Globals: ");
	  for_slice(cxt->globals, i) wasm_data_print(slice_inx(cxt->globals, i));
	  printf("\n");
	}
      }
      if(trace_mem){ 
	if(match_str_suffix(op, cstr_to_str("load")) || 
	   match_str_suffix(op, cstr_to_str("store")) ||
	   match_str_suffix(op, cstr_to_str("load8_u"))||
	   match_str_suffix(op, cstr_to_str("load8_s")) ||
	   match_str_suffix(op, cstr_to_str("store8")) ||
	   match_str_prefix(op, cstr_to_str("memory"))){
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
  SetTargetFPS(240);
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
  // THIS ORDER IS PROB WRONG
  Wasm_Data px={0},py={0},fsz={0};
  py.du64 = slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");
  px.du64 = slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");

  DrawFPS(px.di32, py.di32);
  return 1;
}

#include <stdarg.h>
bool str_builder_append(Str_Builder* out, Cstr fmt, ...){
  va_list args = {0};
  va_start(args, fmt);
  const size_t sz2 = vsnprintf(nullptr, 0, fmt, args);
  va_end(args);

  // Ensure last element was 0
  size_t cnt = out->count;
  if(cnt && slice_last(*out) == 0){
    cnt--;
  }

  if(!resize_u8_darray(out, cnt + sz2 + 1)) return false;
  
  va_start(args, fmt);
  (void)vsnprintf(&slice_inx(*out, cnt), 1+sz2, fmt, args);
  va_end(args);

  //printf("-- Appended to str builder (%zu): `", sz2);
  //va_start(args, fmt);
  //vprintf(fmt, args);
  //va_end(args);
  //printf("` (%zu)------\n", out->count);
  
  return true;
}

Str print_to_str(Alloc_Interface allocr, Exec_Context* cxt,
		      s32 fmtstr_off, s32 argoff){
  //  Copy the string first
  Str fmtstr = memory_rgn_cstrdup(allocr, &cxt->mem, fmtstr_off);
  if(!fmtstr.data){
    fprintf(stderr, "allocation/memory region read failure\n");
    goto return_from_fxn;
  }
  Str_Builder out = {.allocr=allocr}; // Will be 'disowned'

  for_slice(fmtstr, fmtinx){
    char ch = slice_inx(fmtstr, fmtinx);
    if(ch == 0) break;
    if(ch == '%'){
      fmtinx++; ch = slice_inx(fmtstr, fmtinx);
      if(ch == 'd'){
	s32 v;
	if(!memory_rgn_read(&cxt->mem, argoff, sizeof(s32), &v)){
	  fprintf(stderr, "Couldnt read at %zu\n", (size_t)argoff);
	  goto return_from_fxn;
	}
	argoff+=sizeof(s32);
	if(!str_builder_append(&out, "%ld", (long)v)){
	  fprintf(stderr, "Allocation error\n");
	  goto return_from_fxn;
	}
      } else if(ch == 'l' && ((fmtinx+1)<fmtstr.count) &&
		slice_inx(fmtstr, fmtinx+1) == 'x'){
	fmtinx++;
	s32 v;
	if(!memory_rgn_read(&cxt->mem, argoff, sizeof(s32), &v)){
	  fprintf(stderr, "Couldnt read at %zu\n", (size_t)argoff);
	  goto return_from_fxn;
	}
	argoff+=sizeof(s32);
	if(!str_builder_append(&out, "%lx", (long)v)){
	  fprintf(stderr, "Allocation error\n");
	  goto return_from_fxn;
	}
      } else if(ch == 's') {
	s32 v;
	if(!memory_rgn_read(&cxt->mem, argoff, sizeof(s32), &v)){
	  fprintf(stderr, "Couldnt read at %zu\n", (size_t)argoff);
	  goto return_from_fxn;
	}
	argoff+=sizeof(s32);

	// Read another string
	Str argstr = memory_rgn_cstrdup(allocr, &cxt->mem, v);
	if(!argstr.data){
	  fprintf(stderr, "allocation or memory read failure\n");
	  goto return_from_fxn;
	}
	if(!str_builder_append(&out, "%.*s", str_print(argstr))){
	  fprintf(stderr, "Allocation error\n");
	  goto return_from_fxn;
	}
	SLICE_FREE(allocr, argstr);
      } else if(match_str_prefix(str_slice(fmtstr, fmtinx, fmtstr.count-1), ".*s")) {
	fmtinx+=2;
	s32 len;
	if(!memory_rgn_read(&cxt->mem, argoff, sizeof(s32), &len)){
	  fprintf(stderr, "Couldnt read at %zu\n", (size_t)argoff);
	  goto return_from_fxn;
	}
	argoff+=sizeof(s32);

	s32 v;
	if(!memory_rgn_read(&cxt->mem, argoff, sizeof(s32), &v)){
	  fprintf(stderr, "Couldnt read at %zu\n", (size_t)argoff);
	  goto return_from_fxn;
	}
	argoff+=sizeof(s32);

	// Read another string
	Str argstr = memory_rgn_copy_from(allocr, &cxt->mem, v, len);
	if(!argstr.data){
	  fprintf(stderr, "allocation or memory read failure\n");
	  goto return_from_fxn;
	}
	if(!str_builder_append(&out, "%.*s", str_print(argstr))){
	  fprintf(stderr, "Allocation error\n");
	  goto return_from_fxn;
	}
	SLICE_FREE(allocr, argstr);
      } else {
	if(!str_builder_append(&out, "%%%c", ch)){
	  fprintf(stderr, "Allocation error\n");
	  goto return_from_fxn;
	}
      }
    } else {
      if(!str_builder_append(&out, "%c", ch)){
	fprintf(stderr, "Allocation error\n");
	goto return_from_fxn;
      }
    }
  }

 return_from_fxn:
  SLICE_FREE(allocr, fmtstr);
  return (Str){.data = out.data, .count = out.count};
}
u64 printstr(Alloc_Interface allocr, Exec_Context* cxt, void* data){
  // Two int32 args: first pointer to cstr, second pointer to varargs
  if(cxt->stk.count < 2) errnret(0, "printstr needs 2 args, (cstr,varargs)\n");
  Wasm_Data cstr={0},varargs={0};
  varargs.du64 = slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");
  cstr.du64 = slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");

  Str printed_str = print_to_str(allocr, cxt, cstr.di32, varargs.di32);
  printf("%.*s", str_print(printed_str));
  SLICE_FREE(allocr, printed_str);
  return 1;
}
u64 wasm_sprintf(Alloc_Interface allocr, Exec_Context* cxt, void* data){
  // 3 Args: <buffer offset> <format str offset> <...varargs offset>
  // 1 Return value: Length of written characters except for the null byte
  if(cxt->stk.count < 3) errnret(0, "printstr needs 3 args, (dst,cstr,varargs)\n");
  Wasm_Data dst_off={0},cstr={0},varargs={0};
  varargs.du64 = slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");
  cstr.du64 = slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");
  dst_off.du64 = slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");

  Str printed_str = print_to_str(allocr, cxt, cstr.di32, varargs.di32);

  if(!memory_rgn_write(&cxt->mem, dst_off.di32, printed_str.count, printed_str.data)){
    SLICE_FREE(allocr, printed_str);
    errnret(0, "memory region write failure\n");
  }

  const Wasm_Data res = {.di32 = printed_str.count};
  SLICE_FREE(allocr, printed_str);
  if(!push_u64_darray(&cxt->stk, res.du64)) errnret(0, "allocation failure\n");
  return 1;
}
typedef FILE* File_Ptr;
DEF_DARRAY(File_Ptr, 1);

u64 wasm_fopen(Alloc_Interface allocr, Exec_Context* cxt, void* data){
  // Two int32 args: first pointer to filename, second to mode
  if(cxt->stk.count < 2) errnret(0, "fopen needs 2 args, (name,mode)\n");
  Wasm_Data fname_off={0},fmode_off={0};
  fmode_off.du64 = slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");
  fname_off.du64 = slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");
  Str fname = memory_rgn_cstrdup(allocr, &cxt->mem, fname_off.di32);
  if(!fname.data){
    fprintf(stderr, "allocation/memory region read failure\n");
    return 0;
  }
  Str fmode = memory_rgn_cstrdup(allocr, &cxt->mem, fmode_off.di32);
  if(!fmode.data){
    SLICE_FREE(allocr, fname);
    fprintf(stderr, "allocation/memory region read failure\n");
    return 0;
  }

  File_Ptr_Darray* fptrs = data;
  FILE* fptr = fopen(fname.data, fmode.data);
  if(fptr == nullptr){
    const Wasm_Data res = {.di32 = 0};
    if(!push_u64_darray(&cxt->stk, res.du64)){
      SLICE_FREE(allocr, fmode);
      SLICE_FREE(allocr, fname);
      fprintf(stderr, "allocation failure\n");
      return 0;
    }
  } else {
    if(!push_File_Ptr_darray(fptrs, fptr)){
      fclose(fptr);
      SLICE_FREE(allocr, fmode);
      SLICE_FREE(allocr, fname);
      fprintf(stderr, "allocation failure\n");
      return 0;
    }
    const Wasm_Data res = {.di32 = fptrs->count};
    if(!push_u64_darray(&cxt->stk, res.du64)){
      fclose(fptr);
      SLICE_FREE(allocr, fmode);
      SLICE_FREE(allocr, fname);
      fprintf(stderr, "allocation failure\n");
      return 0;
    }
  }

  // count is returned, so the supplied index is actual index + 1
  return 1;
}
u64 wasm_fread(Alloc_Interface allocr, Exec_Context* cxt, void* data){
  // Four int32 args: <buffer ptr> <member size> <num mem> <file ptr>
  if(cxt->stk.count < 2) errnret(0, "fread needs 4 args, (ptr,size,nmemb,stream)\n");
  Wasm_Data ptr_off={0}, item_sz={0}, item_cnt={0}, fptr_inx={0};

  fptr_inx.du64 = slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");
  item_cnt.du64 = slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");
  item_sz.du64 = slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");
  ptr_off.du64 = slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");
  
  const File_Ptr_Darray* fptrs = data;
  if(fptr_inx.di32 > fptrs->count){
    fprintf(stderr, "fread trying to read file not yet opened\n");
    return 0;
  }
  FILE* fstrm = slice_inx(*fptrs, fptr_inx.di32-1);
  // Allocate a temporary buffer, read into it and write it to the
  // actual buffer
  const size_t buflen = (size_t)item_sz.di32 * item_cnt.di32;
  u8_Slice buff = SLICE_ALLOC(allocr, u8, buflen);
  if(!buff.data){
    fprintf(stderr, "allocation failure\n");
    return 0;
  }

  size_t res = fread(buff.data, item_sz.di32, item_cnt.di32, fstrm);
  if(!memory_rgn_write(&cxt->mem, ptr_off.di32, buff.count, buff.data)){
    SLICE_FREE(allocr, buff);
    fprintf(stderr, "memory region allocation failure\n");
    return 0;
  }
  SLICE_FREE(allocr, buff);

  Wasm_Data retv = {0};
  retv.di32 = res;
  if(!push_u64_darray(&cxt->stk, retv.du64)){
    fprintf(stderr, "allocation failure\n");
    return 0;
  }
  return 1;
}

u64 wasm_fseek(Alloc_Interface, Exec_Context* cxt, void* data){
  // Three int32 args, 1 int32 return value
  if(cxt->stk.count < 3) errnret(0, "fseek needs 3 args, (stream, offset, whence)\n");
  Wasm_Data whence_={0}, offset={0}, stream_inx={0};
  whence_.du64=slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");
  offset.du64=slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");
  stream_inx.du64=slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");

  File_Ptr_Darray* fptrs = data;
  if(stream_inx.di32 > fptrs->count){
    fprintf(stderr, "fseek operation on a file not yet opened\n");
    return 0;
  }
  FILE* stream = slice_inx(*fptrs, stream_inx.di32-1);

  int whence = -1;
  switch(whence_.di32){
  case 1: {whence=SEEK_SET; break;}
  case 2: {whence=SEEK_CUR; break;}
  case 3: {whence=SEEK_END; break;}
  }

  Wasm_Data res={0};
  res.di32 = fseek(stream, offset.di32, whence);

  if(!push_u64_darray(&cxt->stk, res.du64)){
    fprintf(stderr, "allocation failure\n");
    return 0;
  }
  return 1;
}

u64 wasm_perror(Alloc_Interface allocr, Exec_Context* cxt, void* data){
  // One int32 args: first pointer to filename, second to mode
  if(cxt->stk.count < 1) errnret(0, "popen needs 1 arg, (errstring)\n");
  Wasm_Data errstr_inx={0};
  errstr_inx.du64=slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");

  Str errstr = memory_rgn_cstrdup(allocr, &cxt->mem, errstr_inx.di32);
  if(!errstr.data){
    fprintf(stderr, "allocation/memory region read failure\n");
    return 0;
  }
  perror(errstr.data);
  SLICE_FREE(allocr, errstr);
  return 1;
}

// Maybe this has to be promoted to some fxn in memory pages file
u64 wasm_memcmp(Alloc_Interface allocr, Exec_Context* cxt, void* data){
  // Three args, <s1 inx, s2 inx, len> one return value <int>
  Wasm_Data s1_inx={0}, s2_inx={0}, len={0};
  len.du64=slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");
  s2_inx.du64=slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");
  s1_inx.du64=slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");

  u8_Slice s1 = memory_rgn_copy_from(allocr, &cxt->mem, s1_inx.di32, len.di32);
  if(!s1.data){
    fprintf(stderr, "allocation/memory region read failure\n");
    return 0;
  }
  u8_Slice s2 = memory_rgn_copy_from(allocr, &cxt->mem, s2_inx.di32, len.di32);  
  if(!s2.data){
    SLICE_FREE(allocr, s1);
    fprintf(stderr, "allocation/memory region read failure\n");
    return 0;
  }

  Wasm_Data res = {0};
  res.di32 = memcmp(s1.data, s2.data, s1.count);
  
  SLICE_FREE(allocr, s1);
  SLICE_FREE(allocr, s2);

  if(!push_u64_darray(&cxt->stk, res.du64)){
    fprintf(stderr, "allocation failure\n");
    return 0;
  }
  return 1;
}

// TODO:: Make this thread local thing later
typedef struct Data_For_Qsort Data_For_Qsort;
struct Data_For_Qsort {
  Exec_Context* cxt;
  Alloc_Interface allocr;
  // Reserved places in the memory heap
  u32 com_finx;
} wasm_qsort_glob_data;
int wasm_qsort_comparator(const void* a, const void* b){
  // The pointers are values to indices on the heap for the array

  const s32* inx1 = a;
  const s32* inx2 = b;
  // TODO:: Handle if error
  Wasm_Data d = {0};
  d.di32 = *inx1;
  (void)push_u64_darray(&wasm_qsort_glob_data.cxt->stk,
			d.du64);
  d.di32 = *inx2;
  (void)push_u64_darray(&wasm_qsort_glob_data.cxt->stk,
			d.du64);

  (void)exec_wasm_fxn(wasm_qsort_glob_data.allocr,
		      wasm_qsort_glob_data.cxt,
		      wasm_qsort_glob_data.com_finx);
  // Assume that there is value on stack
  d.du64 = slice_last(wasm_qsort_glob_data.cxt->stk);
  (void)pop_u64_darray(&wasm_qsort_glob_data.cxt->stk, 1);

  return d.di32;
}

// Maybe this has to be promoted to some fxn in memory pages file
u64 wasm_qsort(Alloc_Interface allocr, Exec_Context* cxt, void* data){
  // Four args, <base inx, count(nmemb), elemsize(size), comparator>
  // Comparator function type:  Two args <elem inx1> <elem inx2> one return : <int>
  Wasm_Data base_inx={0}, count={0}, elemsize={0}, comparator={0};
  comparator.du64=slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");
  elemsize.du64=slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");
  count.du64=slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");
  base_inx.du64=slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");

  // Array to later rearrange data back
  u8_Slice base = SLICE_ALLOC(allocr, u8, count.di32*elemsize.di32);
  if(!base.data){
    fprintf(stderr, "allocation/memory region read failure\n");
    return 0;
  }

  // Create an array of s32
  s32_Slice inxes = SLICE_ALLOC(allocr, s32, count.di32);
  if(!inxes.data){
    fprintf(stderr, "allocation/memory region read failure\n");
    return 0;
  }
  for_slice(inxes, i){
    inxes.data[i] = base_inx.di32 + i * elemsize.di32;
  }

  //fprintf(stderr, "TODO:: yet to implement qsort, args are: \n"
  //	  "base @ %d, count = %d, elemsize = %d (total size = %zu)\n"
  //	  "and the comparator is at %d\n", base_inx.di32, count.di32,
  //	  elemsize.di32, base.count, comparator.di32);
  //return 0;

  wasm_qsort_glob_data.cxt = cxt;
  wasm_qsort_glob_data.allocr = allocr;

  // Find the comparator finx from the table array
  wasm_qsort_glob_data.com_finx = slice_inx(cxt->fxn_table, comparator.di32);

  printf("\n**** Before running qsort ****\n");
  for_slice(inxes, i){
    printf("%d ", slice_inx(inxes, i));
  }
  printf("\n******************************\n");

  qsort(inxes.data, inxes.count, sizeof(inxes.data[0]), wasm_qsort_comparator);
  
  printf("**** After running qsort ****\n");
  for_slice(inxes, i){
    printf("%d ", slice_inx(inxes, i));
  }
  printf("\n******************************\n");


  // Read the data in the sorted order
  for_slice(inxes, i){
    s32 wasm_off = inxes.data[i];
    u32 base_off = i * elemsize.di32;

    (void)slice_inx(base, base_off + elemsize.di32-1); // triggering the assert

    printf("%d from %d -> %p\n", wasm_off, elemsize.di32,
	   &slice_inx(base, base_off));

    if(!memory_rgn_read(&cxt->mem, wasm_off, elemsize.di32,
			&slice_inx(base, base_off))){
      fprintf(stderr, "memory region read failure\n");
      return 0;
    }
  }
  // Write back the data
  if(!memory_rgn_write(&cxt->mem, base_inx.di32, base.count,
		      base.data)){
    fprintf(stderr, "memory region write failure\n");
    return 0;
  }

  SLICE_FREE(allocr, inxes);
  SLICE_FREE(allocr, base);

  return 1;
}

u64 wasm_strcmp(Alloc_Interface allocr, Exec_Context* cxt, void* data){
  // Three args, <s1 inx, s2 inx> one return value <int>
  Wasm_Data s1_inx={0}, s2_inx={0};
  s2_inx.du64=slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");
  s1_inx.du64=slice_last(cxt->stk);
  if(!pop_u64_darray(&cxt->stk, 1)) errnret(0, "allocation failure\n");

  Str s1 = memory_rgn_cstrdup(allocr, &cxt->mem, s1_inx.di32);
  if(!s1.data){
    fprintf(stderr, "allocation/memory region read failure\n");
    return 0;
  }
  Str s2 = memory_rgn_cstrdup(allocr, &cxt->mem, s2_inx.di32);
  if(!s2.data){
    SLICE_FREE(allocr, s1);
    fprintf(stderr, "allocation/memory region read failure\n");
    return 0;
  }

  Wasm_Data res = {0};
  res.di32 = str_cmp(s1, s2); // Using my wrapper cuz whynot
  
  SLICE_FREE(allocr, s1);
  SLICE_FREE(allocr, s2);

  if(!push_u64_darray(&cxt->stk, res.du64)){
    fprintf(stderr, "allocation failure\n");
    return 0;
  }
  return 1;
}

// TODO:: Store this somewhere else
File_Ptr_Darray fptrs;
void patch_imports(Alloc_Interface allocr, Exec_Context* cxt){
  fptrs = init_File_Ptr_darray(allocr);
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
      else if(str_cstr_cmp(((Import*)slice_inx(cxt->fxns, i).data)->self_name, "printstr") == 0){
	slice_inx(cxt->fxns, i).fptr = printstr;
      }
      else if(str_cstr_cmp(((Import*)slice_inx(cxt->fxns, i).data)->self_name, "sprintf") == 0){
	slice_inx(cxt->fxns, i).fptr = wasm_sprintf;
      }
      else if(str_cstr_cmp(((Import*)slice_inx(cxt->fxns, i).data)->self_name, "fopen") == 0){
	slice_inx(cxt->fxns, i).fptr = wasm_fopen;
	slice_inx(cxt->fxns, i).data = &fptrs;
      }
      else if(str_cstr_cmp(((Import*)slice_inx(cxt->fxns, i).data)->self_name, "fread") == 0){
	slice_inx(cxt->fxns, i).fptr = wasm_fread;
	slice_inx(cxt->fxns, i).data = &fptrs;
      }
      else if(str_cstr_cmp(((Import*)slice_inx(cxt->fxns, i).data)->self_name, "fseek") == 0){
	slice_inx(cxt->fxns, i).fptr = wasm_fseek;
	slice_inx(cxt->fxns, i).data = &fptrs;
      }
      else if(str_cstr_cmp(((Import*)slice_inx(cxt->fxns, i).data)->self_name, "perror") == 0){
	slice_inx(cxt->fxns, i).fptr = wasm_perror;
      }
      else if(str_cstr_cmp(((Import*)slice_inx(cxt->fxns, i).data)->self_name, "memcmp") == 0){
	slice_inx(cxt->fxns, i).fptr = wasm_memcmp;
      }
      else if(str_cstr_cmp(((Import*)slice_inx(cxt->fxns, i).data)->self_name, "qsort") == 0){
	slice_inx(cxt->fxns, i).fptr = wasm_qsort;
      }
      else if(str_cstr_cmp(((Import*)slice_inx(cxt->fxns, i).data)->self_name, "strcmp") == 0){
	slice_inx(cxt->fxns, i).fptr = wasm_strcmp;
      }

    }
  }
}


// TODO:: Fix the __stack_pointer also if it exists (or force it to exist)
void run_sample(Alloc_Interface allocr, Module* mod, Str entrypath){
  Exec_Context exec_cxt = init_exec_context(allocr, mod);
  patch_imports(allocr, &exec_cxt);
  // Find the desired exported function
  //exec_cxt.trace = true;
  //exec_cxt.cycle_limit = 1000;
  //exec_cxt.trace_vars = true;
  // Dangerous to enable when large storage is used
  //exec_cxt.trace_mem = true;
  
  //Cstr fxn = "abs_diff";
  //Cstr fxn = "pick_branch";
  //Cstr fxn = "fibo";
  //Cstr fxn = "fibo_rec";
  //Cstr fxn = "sub";
  //Cstr fxn = "print_sum";
  Cstr fxn = "run_raylib";
  //Cstr fxn = "main_";

  // Find the entry of the fxn : first find index, then use fxn
  s32 finx = mod_find_export(mod, fxn, FUNCTION_EXPORT_TYPE);
  if(finx < 0){
    fprintf(stderr, "Couldnt find any function export named %s\n", fxn);
    return;
  }
  Wasm_Data args[] = {
    //{.tag = WASM_DATA_I32, .di32 = 420},
    //{.tag = WASM_DATA_I32, .di32 = 351},
    //{.tag = WASM_DATA_I32, .di32 = 12},
    {.tag = WASM_DATA_I32, .di32 = 0},
    {.tag = WASM_DATA_I32, .di32 = 0},
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
