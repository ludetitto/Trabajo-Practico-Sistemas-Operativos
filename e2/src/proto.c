#include "../include/proto.h"

int parse_kv_int(const char* token, const char* key, int* out){
  size_t klen=strlen(key);
  if(strncmp(token, key, klen)==0 && token[klen]=='='){
    *out = atoi(token + klen + 1);
    return 1;
  }
  return 0;
}

int parse_kv_str(const char* token, const char* key, char* out, size_t max){
  size_t klen=strlen(key);
  if(strncmp(token, key, klen)==0 && token[klen]=='='){
    const char* val = token + klen + 1;
    if (max == 0) return 0;
    // copia segura con terminación garantizada
    snprintf(out, max, "%s", val);
    return 1;
  }
  return 0;
}
