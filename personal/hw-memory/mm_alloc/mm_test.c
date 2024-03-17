#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

/* Function pointers to hw3 functions */
void* (*mm_malloc)(size_t);
void* (*mm_realloc)(void*, size_t);
void (*mm_free)(void*);

static void* try_dlsym(void* handle, const char* symbol) {
  char* error;
  void* function = dlsym(handle, symbol);
  if ((error = dlerror())) {
    fprintf(stderr, "%s\n", error);
    exit(EXIT_FAILURE);
  }
  return function;
}

static void load_alloc_functions() {
  void* handle = dlopen("hw3lib.so", RTLD_NOW);
  if (!handle) {
    fprintf(stderr, "%s\n", dlerror());
    exit(EXIT_FAILURE);
  }

  mm_malloc = try_dlsym(handle, "mm_malloc");
  mm_realloc = try_dlsym(handle, "mm_realloc");
  mm_free = try_dlsym(handle, "mm_free");
}

int main() {
  load_alloc_functions();

  long long* p1 = mm_malloc(sizeof(long long));
  int* p2 = mm_malloc(sizeof(int));

  char* str = mm_malloc(sizeof(char)*12);
  str[0] = 'a',str[1] = 'b',str[2] = 'c';

  int* arr = mm_malloc(sizeof(int)*12);
  arr[0] = 1,arr[1] = 2,arr[2] = 3;

  printf("%p:p1\n",p1);
  printf("%p:p2\n",p2);
  printf("%p:",str);
  printf("%s\n",str);
  printf("%p:",arr);
  for(int i=0;i<12;i++)
  {
	  printf("%d ",arr[i]);
  }
  printf("\n");

  mm_free(p2);
  mm_free(str);
  int* barr = mm_malloc(sizeof(int)*4);
  barr[0] = 6,barr[1] = 7,barr[2] = 8;

  arr = mm_realloc(arr,sizeof(int)*16);

  printf("%p:",barr);
  for(int i=0;i<4;i++)
  {
	  printf("%d ",barr[i]);
  }
  printf("\n");
  printf("%p:",arr);
  for(int i=0;i<16;i++)
  {
	  printf("%d ",arr[i]);
  }
  printf("\n"); 

  long long* p3 = mm_malloc(sizeof(long long));
  printf("%p:p3\n",p3);
  int* p4 = mm_malloc(sizeof(int));
  printf("%p:p4\n",p4);
  barr = mm_realloc(arr,0);
  mm_free(p1);
  mm_free(barr);
  mm_free(p3);
  mm_free(p4);

//  int* data = mm_malloc(sizeof(int));
//  assert(data != NULL);
//  data[0] = 0x162;
//  mm_free(data);
//  puts("malloc test successful!");
}
