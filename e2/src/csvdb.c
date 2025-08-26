#include "../include/csvdb.h"

static void db_reserve(db_t* db, size_t need){
  if(db->cap >= need) return;
  size_t nc = db->cap? db->cap*2:32; if(nc<need) nc=need;
  row_t* nv = realloc(db->v, nc*sizeof(row_t));
  if(!nv) die("OOM");
  db->v=nv; db->cap=nc;
}

int db_load(const char* path, db_t* db){
  memset(db,0,sizeof(*db));
  FILE* f = fopen(path,"r");
  if(!f) return -1;
  char *line=NULL; size_t sz=0;
  // header
  if(getline(&line,&sz,f)<0){ fclose(f); free(line); return -1; }
  // rows
  while(getline(&line,&sz,f)>0){
    row_t r={0};
    trim_nl(line);
    // id,generador,pid,Nombre
    char *id=strtok(line, ",");
    char *gen=strtok(NULL,",");
    char *pid=strtok(NULL,",");
    char *nom=strtok(NULL,",");
    if(!id||!gen||!pid||!nom) continue;
    r.id=atoi(id); r.generador=atoi(gen); r.pid=atoi(pid);
    snprintf(r.nombre, NAME_MAXLEN, "%s", nom); // seguro y con '\0'
    db_reserve(db, db->len+1);
    db->v[db->len++]=r;
  }
  free(line); fclose(f);
  return 0;
}

void db_free(db_t* db){
  free(db->v); memset(db,0,sizeof(*db));
}

int db_write_atomic(const char* path, const db_t* db){
  char tmp[512]; snprintf(tmp,sizeof(tmp),"%s.tmp",path);
  FILE* f = fopen(tmp,"w"); if(!f) return -1;
  fprintf(f,"id,generador,pid,Nombre\n");
  for(size_t i=0;i<db->len;i++){
    const row_t*r=&db->v[i];
    fprintf(f,"%d,%d,%d,%s\n", r->id, r->generador, r->pid, r->nombre);
  }
  if(fclose(f)!=0) return -1;
  if(rename(tmp,path)!=0) return -1;
  return 0;
}

int db_max_id(const db_t* db){
  int mx=0; for(size_t i=0;i<db->len;i++) if(db->v[i].id>mx) mx=db->v[i].id;
  return mx;
}

int db_find_by_id(const db_t* db, int id, row_t* out){
  for(size_t i=0;i<db->len;i++) if(db->v[i].id==id){ if(out)*out=db->v[i]; return 0; }
  return -1;
}

int db_find_by_nombre(const db_t* db, const char* nombre, row_t** out_list, size_t* out_n){
  size_t cap=0, len=0; row_t* buf=NULL;
  for(size_t i=0;i<db->len;i++){
    if(strncmp(db->v[i].nombre, nombre, NAME_MAXLEN)==0){
      if(len==cap){ cap=cap?cap*2:8; buf=realloc(buf, cap*sizeof(row_t)); if(!buf) die("OOM"); }
      buf[len++]=db->v[i];
    }
  }
  *out_list=buf; *out_n=len; return 0;
}

int db_add(db_t* db, const row_t* r){
  db_reserve(db, db->len+1);
  db->v[db->len++]=*r;
  return 0;
}

int db_update(db_t* db, int id, const row_t* patch){
  for(size_t i=0;i<db->len;i++){
    if(db->v[i].id==id){
      if(patch->generador>=0) db->v[i].generador=patch->generador;
      if(patch->pid>=0)       db->v[i].pid=patch->pid;
      if(patch->nombre[0])    snprintf(db->v[i].nombre, NAME_MAXLEN, "%s", patch->nombre);
      return 0;
    }
  }
  return -1;
}

int db_delete(db_t* db, int id){
  for(size_t i=0;i<db->len;i++){
    if(db->v[i].id==id){
      db->v[i]=db->v[db->len-1];
      db->len--; return 0;
    }
  }
  return -1;
}
