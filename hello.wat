(module
	(; Hello a block comment ;)
	(; Now img oing to do a nested block comment
	   (;  Wassap ;)
	;)
	;; Now an inline comment
	;; Another inline comment
	

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
  (func (;2;) (type 0) (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.sub
    local.tee 1
    local.get 1
    i32.const 31
    i32.shr_s
    local.tee 1
    i32.xor
    local.get 1
    i32.sub)
  (func (;3;) (type 0) (param i32 i32) (result i32)
    block  ;; label = @1
      local.get 0
      i32.const 1
      i32.lt_s
      br_if 0 (;@1;)
      block  ;; label = @2
        local.get 1
        i32.const 1
        i32.lt_s
        br_if 0 (;@2;)
        local.get 0
        local.get 1
        i32.sub
        return
      end
      local.get 1
      local.get 0
      i32.mul
      return
    end
    local.get 1
    i32.const 1
    i32.shl
    local.get 0
    local.get 0
    i32.mul
    i32.add)
  (memory (;0;) 17)
  (global (;0;) (mut i32) (i32.const 1048576))
  (export "memory" (memory 0))
  (export "add" (func 0))
  (export "sub" (func 1))
  (export "abs_diff" (func 2))
  (export "pick_branch" (func 3))
  (data (;0;) (i32.const 1048576) "'\22\00\002\00\00\003\00\00\004\00\00\005\00\00\00"))
