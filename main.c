// ./build.sh && cc main.c -lraylib -lm -lGL -lpthread -ldl -lrt -lX11  -o main.out && ./main.out
#include <stddef.h>
#include <stdio.h>
#include <ctype.h>

#define UTIL_INCLUDE_ALL
#define UTILAPI static inline
#include "../c-utils/util_headers.h"
#define slice_last(slice) slice_inx((slice), ((slice).count-1))
#define slice_first(slice) slice_inx((slice), 0)



// A flag to indicate if no extra stdout is to be printed
bool to_print = false;

// Helper macro
#define printfopt(...)				\
  do{						\
    if(to_print){				\
      printf(__VA_ARGS__);			\
    }						\
  }while(0)


DEF_SLICE(u8);
DEF_DARRAY(u8, 8);
typedef u8_Slice Str;
typedef const char* Cstr;
typedef u8_Darray Str_Builder;
DEF_SLICE(Str);
DEF_DARRAY(Str, 8);
static inline Str str_slice(Str str, size_t begin, size_t end){
//PROFILABLE_FXN(Str, str_slice, (Str, str), (size_t, begin), (size_t, end)){
  if(nullptr == str.data || begin >= end || end > str.count) return (Str){ 0 };
  return (Str){ .data = str.data + begin, .count = end - begin };
}
static inline Str cstr_to_str(const Cstr str){
  return (Str){.data = (void*)str, .count = strlen(str)};
}
static inline int str_str_cmp(Str str1, Str str2){
  const size_t l = _min(str1.count, str2.count);
  int v = strncmp(str1.data, str2.data, l);
  if(v != 0) return v;
  return (int)(str1.count - l) - (int)(str2.count - l);
}
#define DEF_STR_CSTR_FXN(base_fxn, ret_type)		\
  static inline ret_type base_fxn##_c_c(Cstr a, Cstr b){return base_fxn(cstr_to_str(a), cstr_to_str(b));} \
  static inline ret_type base_fxn##_s_c(Str a, Cstr b){return base_fxn(a, cstr_to_str(b));} \
  static inline ret_type base_fxn##_c_s(Cstr a, Str b){return base_fxn(cstr_to_str(a), b);} 

#define DEF_STR_CSTR_GEN(base_fxn, arg1, arg2)	\
  (_Generic((arg1),							\
	    Str: _Generic((arg2), Str: base_fxn, default:base_fxn##_s_c),	\
	    default: _Generic((arg2), Str: base_fxn##_c_s, default: base_fxn##_c_c))	\
   ((arg1),(arg2)))

DEF_STR_CSTR_FXN(str_str_cmp, int);
#define str_cmp(a, b) DEF_STR_CSTR_GEN(str_str_cmp, a, b)


bool match_str_prefix_(Str haystack, Str needle){
  return str_str_cmp(needle, str_slice(haystack, 0, needle.count)) == 0;
}
DEF_STR_CSTR_FXN(match_str_prefix_, bool);
#define match_str_prefix(haystack, needle)		\
  DEF_STR_CSTR_GEN(match_str_prefix_, haystack, needle)


bool match_str_suffix_(Str haystack, Str needle){
  if(haystack.count<needle.count) return false;
  return str_str_cmp(needle, str_slice(haystack, haystack.count-needle.count, haystack.count)) == 0;
}
DEF_STR_CSTR_FXN(match_str_suffix_, bool);
#define match_str_suffix(haystack, needle)		\
  DEF_STR_CSTR_GEN(match_str_suffix_, haystack, needle)


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

#include "profiling.h"

DEF_SLICE(s32);
#include "lines_and_files.h"
#include "parse_node.h"
#include "parse_into_trees.h"
#include "parsed_utils.h"

#include "parsed_type.h"
#include "parsed_func.h"
#include "parsed_exports.h" 
#include "parsed_imports.h"
#include "parsed_data.h"
#include "parsed_global.h"
#include "parsed_module.h"

// Try to run a given function
// Assume all the functions are ordered correctly ?
#include "stk_n_mems.h"

#include "runner.h"

int main(void){
  timespec time_whole = start_process_timer();

  Alloc_Interface allocr = gen_std_allocator();
  //parse_node_iter_run_demo(allocr);

  //Cstr watfname = "hello.wat";
  Cstr watfname = "1brc/1brc.wat";
  Parse_Info parser = init_parse_info(allocr, watfname);

  // printf("Printing the file by lines: \n");
  // for_slice(parser.line_nums, ln){
  //   Str line = get_line(parser.file, parser.line_nums, ln);
  //   printf("%4d: %.*s\n", (int)ln+1, str_print(line));
  // }

  u32_Slice poses = SLICE_FROM_ARRAY(u32, ((u32[]){0, 4, 6, 7, 8, 18, 499, 819, 731, 732, 733}));

  // for_slice(poses, i){
  //   Cursor pos = get_line_pos(parser.line_nums, poses.data[i]);
  //   printf("Char at %u is at line %u, offset %u\n",poses.data[i],pos.line+1,pos.offset);
  // }

  if(parse_brackets(&parser)){
    printfopt("Yes, we could parse %s into a parse tree!!!\n", parser.fname);
  } else {
    fprintf(stderr, "Oh no! we could not parse %s\n", parser.fname);
    return 1;
  }

  //for_each_parse_node_df(node, parser.root){
  //printfopt("Node %p = %.*s, children = %zu\n", node, str_print(node->data), node->children.count);
  //}

  Module main_module = {0}; // TODO:: itsy bitsy memory leak
  if(!parse_module(allocr, parser.root, &main_module)){
    fprintf(stderr, "Couldnt parse the main module!!!\n");
    return 1;
  }
  //try_printing_module(&main_module);

  //run_memory_page_sample(allocr);
  opcode_cntr = init_opcode_counter(allocr);
  // Measure the time taken
  timespec time_run = start_process_timer();
  const int runs = 4;
  for_range(int, i, 0, runs){
    run_sample(allocr, &main_module, cstr_to_str(watfname));
  }
  printf("Interpretation of program with %d runs took %lf s to run \n", runs, timer_sec(end_process_timer(&time_run)));
  free_parse_info(&parser);

  double prog_time = timer_milli(end_process_timer(&time_whole));
  printf("The whole program took %lf s to run \n", prog_time/1000.0);

  //dump_opcode_counts(opcode_cntr);

#define print_profiling_results(item)					\
  do{									\
    printf("Profiling results for '"#item"': \n");			\
    printf("    Total time spent: %lf ms\n", GET_PROFILED_SUM(item));	\
    printf("    Total number of invocations: %zu\n", GET_PROFILED_COUNT(item));	\
    printf("    Absolute average : %lf\n",				\
	   GET_PROFILED_SUM(item)/GET_PROFILED_COUNT(item));		\
    printf("    Percentage involvement: %.2lf %%\n",			\
	   100.0 * (GET_PROFILED_SUM(item) / prog_time));		\
      }while(0);

  //print_profiling_results(parse_brackets_);
  //print_profiling_results(print_to_str);
  //print_profiling_results(memory_rgn_read);
  //print_profiling_results(memory_rgn_write);
  //print_profiling_results(memory_rgn_ensure);
  //print_profiling_results(run_wasm_opcodes);
  //print_profiling_results(exec_wasm_fxn);

  //printf("Profiling results for 'str_slice': \n");
  //printf("    Total time spent: %lf\n", GET_PROFILED_SUM(str_slice));
  //printf("    Total number of invocations: %zu\n", GET_PROFILED_COUNT(str_slice));
  //printf("    Absolute average : %lf\n", 
  //	 GET_PROFILED_SUM(str_slice)/GET_PROFILED_COUNT(str_slice));;
  

  return 0;
}


