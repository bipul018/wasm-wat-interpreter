(module
  (type (;0;) (func (param i32 i32) (result i32)))
  (type (;1;) (func (param i32) (result i32)))
  (type (;2;) (func (param i32)))
  (type (;3;) (func (param i32 i32)))
  (import "env" "printint" (func (;0;) (type 2)))
  (func (;1;) (type 0) (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.add)
  (func (;2;) (type 0) (param i32 i32) (result i32)
    (local i32)
    local.get 0
    i32.const 2
    i32.shl
    i32.const 1024
    i32.add
    local.get 0
    local.get 1
    i32.sub
    local.tee 2
    i32.store
    i32.const 1024
    i32.load
    local.get 2
    i32.const 1
    i32.shl
    local.get 0
    local.get 1
    i32.mul
    i32.sub
    i32.add)
  (func (;3;) (type 0) (param i32 i32) (result i32)
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
  (func (;4;) (type 0) (param i32 i32) (result i32)
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
  (func (;5;) (type 1) (param i32) (result i32)
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
  (func (;6;) (type 1) (param i32) (result i32)
    (local i32 i32)
    local.get 0
    i32.const 2
    i32.ge_s
    if  ;; label = @1
      loop  ;; label = @2
        local.get 0
        i32.const 1
        i32.sub
        call 6
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
  (func (;7;) (type 3) (param i32 i32)
    local.get 0
    local.get 1
    i32.add
    call 0)
  (memory (;0;) 2)
  (export "memory" (memory 0))
  (export "add" (func 1))
  (export "sub" (func 2))
  (export "abs_diff" (func 3))
  (export "pick_branch" (func 4))
  (export "fibo" (func 5))
  (export "fibo_rec" (func 6))
  (export "print_sum" (func 7))
  (data (;0;) (i32.const 1024) "'\22\00\002\00\00\003\00\00\004\00\00\005"))
