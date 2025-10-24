
set -xe
cc -pg -O3 main.c -lraylib -lm -lGL -lpthread -ldl -lrt -lX11  -o main.out 
./main.out
gprof main.out gmon.out \
    | nix run nixpkgs#gprof2dot \
    | nix shell nixpkgs#graphviz -c dot -Tpng -o perf.png
