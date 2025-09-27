(module
  (type (;0;) (func (param i32 i32) (result i32)))
  (func (;0;) (type 0) (param i32 i32) (result i32)
    local.get 1
    local.get 0
    i32.add)
  (func (;1;) (type 0) (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.sub
    i32.const 0
    i32.load offset=1
    i32.add)
  (func (;2;) (type 0) (param i32 i32) (result i32)
    i32.const 0
    local.get 1
    local.get 0
    i32.mul
    local.tee 1
    i32.store offset=1048576
    local.get 1
    local.get 0
    i32.add)
  (memory (;0;) 17)
  (global (;0;) (mut i32) (i32.const 1048576))
  (export "memory" (memory 0))
  (export "add" (func 0))
  (export "sub" (func 1))
  (export "sub2" (func 2))
  (data (;0;) (i32.const 1048576) "3\00\00\00"))
