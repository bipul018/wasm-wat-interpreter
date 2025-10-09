(module
  (type (;0;) (func (param i32)))
  (type (;1;) (func (param i32 i32)))
  (type (;2;) (func (param i32 i32) (result i32)))
  (type (;3;) (func))
  (type (;4;) (func (param i32) (result i32)))
  (type (;5;) (func (result i32)))
  (import "env" "printint" (func (;0;) (type 0)))
  (import "env" "init_window" (func (;1;) (type 1)))
  (import "env" "printstr" (func (;2;) (type 1)))
  (import "env" "window_should_close" (func (;3;) (type 5)))
  (import "env" "begin_drawing" (func (;4;) (type 3)))
  (import "env" "printhex" (func (;5;) (type 0)))
  (import "env" "clear_background" (func (;6;) (type 0)))
  (import "env" "draw_fps" (func (;7;) (type 1)))
  (import "env" "end_drawing" (func (;8;) (type 3)))
  (import "env" "memory" (memory (;0;) 2))
  (func (;9;) (type 3))
  (func (;10;) (type 2) (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.add)
  (func (;11;) (type 2) (param i32 i32) (result i32)
    (local i32)
    local.get 0
    i32.const 2
    i32.shl
    i32.const 1120
    i32.add
    local.get 0
    local.get 1
    i32.sub
    local.tee 2
    i32.store
    i32.const 1120
    i32.load
    local.get 2
    i32.const 1
    i32.shl
    local.get 0
    local.get 1
    i32.mul
    i32.sub
    i32.add)
  (func (;12;) (type 2) (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.sub
    local.tee 0
    local.get 0
    i32.const 31
    i32.shr_s
    local.tee 0
    i32.xor
    local.get 0
    i32.sub)
  (func (;13;) (type 2) (param i32 i32) (result i32)
    local.get 0
    i32.const 0
    i32.gt_s
    if  ;; label = @1
      local.get 1
      i32.const 0
      i32.gt_s
      if  ;; label = @2
        local.get 0
        local.get 1
        i32.sub
        return
      end
      local.get 0
      local.get 1
      i32.mul
      return
    end
    local.get 0
    local.get 0
    i32.mul
    local.get 1
    i32.const 1
    i32.shl
    i32.add)
  (func (;14;) (type 4) (param i32) (result i32)
    (local i32 i32)
    local.get 0
    i32.const 0
    i32.le_s
    if  ;; label = @1
      i32.const 0
      return
    end
    local.get 0
    i32.const 1
    i32.add
    local.set 0
    i32.const 1
    local.set 2
    loop  ;; label = @1
      local.get 1
      local.get 2
      local.tee 1
      i32.add
      local.set 2
      local.get 0
      i32.const 1
      i32.sub
      local.tee 0
      i32.const 1
      i32.gt_u
      br_if 0 (;@1;)
    end
    local.get 1)
  (func (;15;) (type 4) (param i32) (result i32)
    (local i32 i32)
    local.get 0
    i32.const 2
    i32.ge_s
    if  ;; label = @1
      loop  ;; label = @2
        local.get 0
        i32.const 1
        i32.sub
        call 15
        local.get 1
        i32.add
        local.set 1
        local.get 0
        i32.const 3
        i32.gt_u
        local.get 0
        i32.const 2
        i32.sub
        local.set 0
        br_if 0 (;@2;)
      end
    end
    local.get 0
    local.get 1
    i32.add)
  (func (;16;) (type 1) (param i32 i32)
    local.get 0
    local.get 1
    i32.add
    call 0)
  (func (;17;) (type 0) (param i32)
    (local i32 i32)
    global.get 0
    i32.const 16
    i32.sub
    local.tee 0
    global.set 0
    i32.const 800
    i32.const 800
    call 1
    local.get 0
    i64.const 317827580147
    i64.store offset=4 align=4
    local.get 0
    i32.const 1056
    i32.store
    i32.const 1067
    local.get 0
    call 2
    call 3
    i32.eqz
    if  ;; label = @1
      loop  ;; label = @2
        call 4
        local.get 1
        i32.const 100
        i32.div_u
        i32.const 7
        i32.and
        local.tee 2
        call 0
        local.get 2
        i32.const 2
        i32.shl
        i32.load offset=1024
        local.tee 2
        call 5
        local.get 2
        call 6
        i32.const 10
        i32.const 10
        call 7
        local.get 1
        i32.const 1
        i32.add
        local.set 1
        call 8
        call 3
        i32.eqz
        br_if 0 (;@2;)
      end
    end
    i32.const -16711936
    call 5
    local.get 0
    i32.const 16
    i32.add
    global.set 0)
  (global (;0;) (mut i32) (i32.const 66688))
  (global (;1;) i32 (i32.const 1120))
  (global (;2;) i32 (i32.const 1024))
  (global (;3;) i32 (i32.const 1140))
  (global (;4;) i32 (i32.const 1152))
  (global (;5;) i32 (i32.const 66688))
  (global (;6;) i32 (i32.const 1024))
  (global (;7;) i32 (i32.const 66688))
  (global (;8;) i32 (i32.const 131072))
  (global (;9;) i32 (i32.const 0))
  (global (;10;) i32 (i32.const 1))
  (global (;11;) i32 (i32.const 65536))
  (export "__wasm_call_ctors" (func 9))
  (export "__stack_pointer" (global 0))
  (export "add" (func 10))
  (export "sub" (func 11))
  (export "v" (global 1))
  (export "abs_diff" (func 12))
  (export "pick_branch" (func 13))
  (export "fibo" (func 14))
  (export "fibo_rec" (func 15))
  (export "print_sum" (func 16))
  (export "run_raylib" (func 17))
  (export "__dso_handle" (global 2))
  (export "__data_end" (global 3))
  (export "__stack_low" (global 4))
  (export "__stack_high" (global 5))
  (export "__global_base" (global 6))
  (export "__heap_base" (global 7))
  (export "__heap_end" (global 8))
  (export "__memory_base" (global 9))
  (export "__table_base" (global 10))
  (export "__wasm_first_page_end" (global 11))
  (data (;0;) (i32.const 1024) "\ff\ff\ff\ff\00\ff\ff\ff\ff\00\ff\ff\ff\ff\00\ff\00\00\ff\ff\00\ff\00\ff\ff\00\00\ff\00\00\00\ff<printing>\00Hello, beginning the %s operation now %d %d\0a\00")
  (data (;1;) (i32.const 1120) "'\22\00\002\00\00\003\00\00\004\00\00\005\00\00\00"))
