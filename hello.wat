(module
  (type (;0;) (func (param i32 i32) (result i32)))
  (type (;1;) (func (param i32 f32)))
  (type (;2;) (func (result i32)))
  (type (;3;) (func))
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
  (func (;3;) (type 1) (param i32 f32)
    local.get 0
    local.get 1
    i32.trunc_sat_f32_s
    i32.store)
  (func (;4;) (type 2) (result i32)
    i32.const 48)
  (func (;5;) (type 3))
  (memory (;0;) 17)
  (global (;0;) (mut i32) (i32.const 1048576))
  (export "memory" (memory 0))
  (export "add" (func 0))
  (export "sub" (func 1))
  (export "sub2" (func 2))
  (export "hiya" (func 3))
  (export "getit" (func 4))
  (export "donot" (func 5))
  (data (;0;) (i32.const 1048576) "3\00\00\00"))
