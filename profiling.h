// This is intentionally not done, this file is supposed to be included only once
// knowingly as part of some 'unity build' (for now)
// The same reason (also lazy) is why no extra includes are done here
// #pragma once


#define COMMA() ,
#define EMPTY()

// A macro to generate the comma separated list of fxn prototype from a list like
//     (int, a), (float, b), (double, c) ...
//  => int a, float b, double c ...

#define MAKE_ONE_FXN_PROTOTYPE_(type, iden) type iden
#define MAKE_ONE_FXN_PROTOTYPE(N, arg) MAKE_ONE_FXN_PROTOTYPE_ arg  IF_ZERO(N, EMPTY, COMMA)()
#define MAKE_FXN_PROTOTYPE_LIST(...)		\
  FOR_EACH_VA2(MAKE_ONE_FXN_PROTOTYPE , __VA_ARGS__)

// A macro to generate the comma separated list of fxn arg identifiers from a list like
//     (int, a), (float, b), (double, c) ...
//  => a, b, c ...
#define MAKE_ONE_ARG_FWDR_(type, iden) iden
#define MAKE_ONE_ARG_FWDR(N, arg) MAKE_ONE_ARG_FWDR_ arg  IF_ZERO(N, EMPTY, COMMA)()
#define MAKE_FXN_ARG_FWDR(...)		\
  FOR_EACH_VA2(MAKE_ONE_ARG_FWDR , __VA_ARGS__)

// Function wrappers that works for A> non-void B> void return type fxns

#define PROFILABLE_FXN_CALL_NON_VOID(ret_type) ret_type res = 
#define PROFILABLE_FXN_CALL_VOID(ret_type) (void)

#define PROFILABLE_FXN_RET_NON_VOID() return res
#define PROFILABLE_FXN_RET_VOID() return 
  
// TODO:: Figure out how to add attributes
#define PROFILABLE_FXN_(fxn_ret_kind, ret_type, name, ... /*args*/)	\
  static double name##_fxn_profiled_sum = 0.0;					\
  static u64 name##_fxn_profiled_count = 0;					\
  static ret_type name##_inner(MAKE_FXN_PROTOTYPE_LIST(__VA_ARGS__));	\
  ret_type name(MAKE_FXN_PROTOTYPE_LIST(__VA_ARGS__)){			\
    timespec timer = start_process_timer();				\
    PROFILABLE_FXN_CALL_##fxn_ret_kind(ret_type)			\
      name##_inner(MAKE_FXN_ARG_FWDR(__VA_ARGS__));			\
    name##_fxn_profiled_sum += timer_sec(end_process_timer(&timer));	\
    name##_fxn_profiled_count++;					\
    PROFILABLE_FXN_RET_##fxn_ret_kind();				\
  }									\
  static ret_type name##_inner(MAKE_FXN_PROTOTYPE_LIST(__VA_ARGS__))	

// When we do need the return value
#define PROFILABLE_FXN(ret_type, name, ... /*args*/)			\
  PROFILABLE_FXN_(NON_VOID, ret_type, name, __VA_ARGS__)
// When we dont need the return value
#define PROFILABLE_PROC(ret_type, name, ... /*args*/)	\
  PROFILABLE_FXN_(VOID, ret_type, name, __VA_ARGS__)

#define GET_PROFILED_COUNT(fxn_name) fxn_name##_fxn_profiled_count
#define GET_PROFILED_SUM(fxn_name) fxn_name##_fxn_profiled_sum

// Usage: 
//PROFILABLE_FXN(int, get_sum, (int, a), (float, b)){
//  return a + (float)b;
//}
//PROFILABLE_PROC(void, print_sum, (int, a), (float, b)){
//  printf("%d\n", get_sum(a, b));
//}

//GET_PROFILED_COUNT(get_sum)
//GET_PROFILED_SUM(print_sum)
