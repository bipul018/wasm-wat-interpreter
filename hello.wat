(module
  (type (;0;) (func (param i32 i32) (result i32)))
  (type (;1;) (func (param i32)))
  (type (;2;) (func (param i32 i32)))
  (type (;3;) (func))
  (type (;4;) (func (param i32) (result i32)))
  (type (;5;) (func (param i32 i32 i32 i32)))
  (type (;6;) (func (result i32)))
  (import "env" "printint" (func (;0;) (type 1)))
  (import "env" "init_window" (func (;1;) (type 2)))
  (import "env" "printstr" (func (;2;) (type 2)))
  (import "env" "qsort" (func (;3;) (type 5)))
  (import "env" "window_should_close" (func (;4;) (type 6)))
  (import "env" "begin_drawing" (func (;5;) (type 3)))
  (import "env" "clear_background" (func (;6;) (type 1)))
  (import "env" "draw_fps" (func (;7;) (type 2)))
  (import "env" "end_drawing" (func (;8;) (type 3)))
  (import "env" "printhex" (func (;9;) (type 1)))
  (import "env" "__stack_pointer" (global (;0;) (mut i32)))
  (import "env" "__memory_base" (global (;1;) i32))
  (import "env" "__table_base" (global (;2;) i32))
  (import "env" "memory" (memory (;0;) 1))
  (import "env" "__indirect_function_table" (table (;0;) 1 funcref))
  (func (;10;) (type 0) (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.add)
  (func (;11;) (type 0) (param i32 i32) (result i32)
    (local i32 i32)
    global.get 1
    i32.const 128
    i32.add
    local.tee 2
    local.get 0
    i32.const 2
    i32.shl
    i32.add
    local.get 0
    local.get 1
    i32.sub
    local.tee 3
    i32.store
    local.get 2
    i32.load
    local.get 3
    i32.const 1
    i32.shl
    local.get 0
    local.get 1
    i32.mul
    i32.sub
    i32.add)
  (func (;12;) (type 0) (param i32 i32) (result i32)
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
  (func (;13;) (type 0) (param i32 i32) (result i32)
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
  (func (;16;) (type 2) (param i32 i32)
    local.get 0
    local.get 1
    i32.add
    call 0)
  (func (;17;) (type 0) (param i32 i32) (result i32)
    local.get 0
    i32.load
    local.get 1
    i32.load
    i32.sub)
  (func (;18;) (type 1) (param i32)
    (local i32 i32)
    global.get 0
    i32.const 80
    i32.sub
    local.tee 0
    global.set 0
    i32.const 800
    i32.const 800
    call 1
    local.get 0
    i64.const 317827580147
    i64.store offset=36 align=4
    local.get 0
    global.get 1
    local.tee 1
    i32.const 32
    i32.add
    i32.store offset=32
    local.get 1
    i32.const 47
    i32.add
    local.get 0
    i32.const 32
    i32.add
    call 2
    local.get 0
    i32.const 72
    i32.add
    local.get 1
    i32.const 96
    i32.add
    local.tee 1
    i32.const 24
    i32.add
    i32.load
    i32.store
    local.get 0
    i32.const -64
    i32.sub
    local.get 1
    i32.const 16
    i32.add
    i64.load
    i64.store
    local.get 0
    local.get 1
    i32.const 8
    i32.add
    i64.load
    i64.store offset=56
    local.get 0
    local.get 1
    i64.load
    i64.store offset=48
    i32.const 0
    local.set 1
    loop  ;; label = @1
      local.get 0
      local.get 0
      i32.const 48
      i32.add
      local.tee 2
      local.get 1
      i32.add
      i32.load
      i32.store offset=16
      global.get 1
      i32.const 43
      i32.add
      local.get 0
      i32.const 16
      i32.add
      call 2
      local.get 1
      i32.const 4
      i32.add
      local.tee 1
      i32.const 28
      i32.ne
      br_if 0 (;@1;)
    end
    i32.const 0
    local.set 1
    global.get 1
    i32.const 90
    i32.add
    i32.const 0
    call 2
    local.get 2
    i32.const 7
    i32.const 4
    global.get 2
    call 3
    loop  ;; label = @1
      local.get 0
      local.get 0
      i32.const 48
      i32.add
      local.get 1
      i32.add
      i32.load
      i32.store
      global.get 1
      i32.const 43
      i32.add
      local.get 0
      call 2
      local.get 1
      i32.const 4
      i32.add
      local.tee 1
      i32.const 28
      i32.ne
      br_if 0 (;@1;)
    end
    i32.const 0
    local.set 1
    global.get 1
    i32.const 90
    i32.add
    i32.const 0
    call 2
    call 4
    i32.eqz
    if  ;; label = @1
      loop  ;; label = @2
        global.get 1
        local.get 1
        i32.const 100
        i32.div_u
        i32.const 7
        i32.and
        i32.const 2
        i32.shl
        i32.add
        i32.load
        call 5
        call 6
        i32.const 10
        i32.const 10
        call 7
        local.get 1
        i32.const 1
        i32.add
        local.set 1
        call 8
        call 4
        i32.eqz
        br_if 0 (;@2;)
      end
    end
    i32.const -16711936
    call 9
    local.get 0
    i32.const 80
    i32.add
    global.set 0)
  (export "add" (func 10))
  (export "sub" (func 11))
  (export "abs_diff" (func 12))
  (export "pick_branch" (func 13))
  (export "fibo" (func 14))
  (export "fibo_rec" (func 15))
  (export "print_sum" (func 16))
  (export "run_raylib" (func 18))
  (elem (;0;) (global.get 2) func 17)
  (data (;0;) (global.get 1) "\ff\ff\ff\ff\00\ff\ff\ff\ff\00\ff\ff\ff\ff\00\ff\00\00\ff\ff\00\ff\00\ff\ff\00\00\ff\00\00\00\ff<printing>\00%d \00Hello, beginning the %s operation now %d %d\0a\00\00\00\00\00(\00\00\00\1e\00\00\00\14\00\00\00\0a\00\00\00\0f\00\00\00\0c\00\00\00\10\00\00\00\00\00\00\00'\22\00\002\00\00\003\00\00\004\00\00\005\00\00\00"))
