
set -xe
clang \
--target=wasm32 \
-nostdlib \
-Os \
wasm_1brc.c -DFOR_WASM \
-o 1brc.wasm \
-Wl,--no-entry \
-fuse-ld=lld \
-Wl,--export=main_ \
-Wl,--allow-undefined \
 -fident \
 --shared -fPIC \

# -Wl,--export=shared_data \

#  -Wl,--export=__wasm_call_ctors \
#  -Wl,--export=__dso_handle \
#  -Wl,--export=__data_end \
#  -Wl,--export=__stack_low \
#  -Wl,--export=__stack_high \
#  -Wl,--export=__global_base \
#  -Wl,--export=__wasm_first_page_end \
#  -Wl,--export=__wasm_init_memory \

# -Wl,--export-all \
# -Wl,--import-memory \
# -Wl,--export=__stack_pointer \
# -Wl,--export=__heap_base \
# -Wl,--export=__heap_end \
# -Wl,--export=__stack_low \
# -Wl,--export=__stack_high \

wasm2wat 1brc.wasm -o 1brc.wat

clang wasm_1brc.c -o 1brc_native.out
