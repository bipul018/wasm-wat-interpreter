# clang --target=wasm32 -O0 -nostdlib -Wl,--no-entry -Wl,--export=add -o hello.wasm hello.c
zig cc -target wasm32-freestanding -Os \
  -nostdlib -Wl,--no-entry -Wl,--export=add -Wl,--export=sub,--export=abs_diff \
  hello.c -o hello.wasm

wasm2wat hello.wasm -o hello.wat
