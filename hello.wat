(module
  (type (;0;) (func (param i32 i32) (result i32)))
  (func (;0;) (type 0) (param i32 i32) (result i32)
    local.get 1
    local.get 0
    i32.add)
  (func (;1;) (type 0) (param i32 i32) (result i32)
    (local i32)
    local.get 0
    i32.const 2
    i32.shl
    i32.const 1048576
    i32.add
    local.get 0
    local.get 1
    i32.sub
    local.tee 2
    i32.store
    local.get 2
    i32.const 1
    i32.shl
    local.get 1
    local.get 0
    i32.mul
    i32.sub
    i32.const 0
    i32.load offset=1048576
    i32.add)
  (memory (;0;) 17)
  (global (;0;) (mut i32) (i32.const 1048576))
  (export "memory" (memory 0))
  (export "add" (func 0))
  (export "sub" (func 1))
  (data (;0;) (i32.const 1048576) "'\22\00\002\00\00\003\00\00\004\00\00\005\00\00\00"))
