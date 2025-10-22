#include <stddef.h>

#ifdef FOR_WASM
int strcmp(const char*, const char*);
int sprintf(char*, const char*, ...);
int puts(const char*);
typedef struct FILE FILE;
FILE* fopen(const char* fname, const char* mode);
void perror(const char*);
#define EXIT_FAILURE -1
void exit(int exit_code);
#define _align_up(size, alignment)                                   \
    ((((unsigned long long)size) + ((unsigned long long)alignment) - \
      1ULL) &                                                        \
     ~(((unsigned long long)alignment) - 1ULL))

// Declare a custom logging function with printf-style format checking
void printstr(const char *format, ...) __attribute__((format(printf, 1, 2)));

void printstr(const char* fmt, ...);
// https://surma.dev/things/c-to-webassembly/
// Externally exported things?
extern unsigned char __heap_base; // We care about the ptr of this variable
extern void* __heap_end;
static void* heap_ptr = &__heap_base;
static void* malloc(size_t sz){
  //printstr("Before malloc: (%lx) %lx\n", (unsigned long)sz, (unsigned long)heap_ptr);
  void* next_ptr = heap_ptr;
  if(((char*)next_ptr+sz) >= (char*)__heap_end){
    return (void*)0;
  }
  heap_ptr = (void*)_align_up((size_t)heap_ptr+sz, sizeof(void*));
  //printstr("After malloc: %lx\n", (unsigned long)heap_ptr);
  return next_ptr;
}
#define MAP_FAILED ((void*)(-1))
// This doesnt have to be implemented, just declared and is replaced with
//      memory.fill instruction (wtf clang?)
void* memset(void *s, int c, size_t n);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
int fseek(FILE *stream, long offset, int whence);
enum {
  SEEK_SET=1,
  SEEK_CUR=2,
  SEEK_END=3,
};
int memcmp(const void *s1, const void *s2, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void qsort(void *base, size_t nmemb, size_t size,
	   int (*compar)(const void *, const void *));
int fclose(FILE *stream);

void __Begin_Tracing(void);
void __End_Tracing(void);
void __Begin_Stepping(void);

static size_t strlen(const char* str){
  size_t l = 0;
  while(*str) {
    str++;
    l++;
  }
  return l;
}
#define printf(...) printstr(__VA_ARGS__)

#else
#include <linux/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#define main_(...) main(__VA_ARGS__)
#define __Begin_Tracing(...)
#define __End_Tracing(...)
#define __Begin_Stepping(...)

#endif
