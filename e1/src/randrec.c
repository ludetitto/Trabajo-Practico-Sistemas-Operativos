#include "common.h"
#include <stdlib.h>
#include <string.h>
static void fillstr(char* dst, size_t max){
    static const char* pool[]={"alpha","beta","gamma","delta","omega","kappa","lambda"};
    const char* s = pool[rand() % (sizeof(pool)/sizeof(pool[0]))];
    strncpy(dst, s, max-1); dst[max-1]='\0';
}
void make_random_record(record_t* r, uint64_t id){
    r->id = id;
    fillstr(r->f1, MAX_STR);
    fillstr(r->f2, MAX_STR);
    r->f3 = (double)(rand()%1000)/10.0;
}
