# clang --target=wasm32 -O0 -nostdlib -Wl,--no-entry -Wl,--export=add -o hello.wasm hello.c
set -xe
# zig cc -target wasm32-freestanding -Os \
#     -nostdlib -Wl,--no-entry -Wl,--export=add -Wl,--export=sub,--export=abs_diff,--export=pick_branch,--export=fibo,--export=fibo_rec \
#   hello.c -o hello.wasm

# nix shell nixpkgs#llvmPackages_21.clang-unwrapped nixpkgs#llvmPackages_21.lld -c \

clang \
--target=wasm32 \
-nostdlib \
-Os \
hello.c \
-o hello.wasm \
-Wl,--no-entry \
-Wl,--export=add \
-Wl,--export=sub \
-Wl,--export=abs_diff \
-Wl,--export=pick_branch \
-Wl,--export=fibo \
-Wl,--export=fibo_rec \
-Wl,--export=print_sum \
-Wl,--export=run_raylib \
-Wl,--allow-undefined \
-fuse-ld=lld


wasm2wat hello.wasm -o hello.wat
