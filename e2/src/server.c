#include "common.h"
#include "csvdb.h"
#include "proto.h"

typedef struct {
  const char* csv_path;
  int tx_active;             // 0/1
  pthread_t tx_owner;        // thread owner
  int tx_fd;                 // fd con WRLCK durante la tx
  db_t tx_db_snapshot;       // snapshot en memoria mientras dura la tx
  pthread_mutex_t m;
} tx_state_t;

typedef struct {
  int listen_fd;
  int max_active;            // N
  int backlog;               // M
  const char* bind_ip;
  uint16_t port;
  const char* csv_path;
} server_cfg_t;

static tx_state_t G={0};
static sem_t SEM_SLOTS;      // limita N conexiones activas
static pthread_mutex_t G_CONN_M = PTHREAD_MUTEX_INITIALIZER;
static int G_CONN_COUNT = 0;

static void usage(const char* p){
  fprintf(stderr,
    "Uso: %s -p <puerto> -f <csv> [-n <activos>] [-m <backlog>] [-H <bind_ip>]\n"
    "Ej:  %s -p 5000 -f ../e1_mod/db.csv -n 4 -m 16\n", p,p);
}
static int lock_exclusive_nb(int fd){
  struct flock fl={.l_type=F_WRLCK,.l_whence=SEEK_SET,.l_start=0,.l_len=0};
  if(fcntl(fd, F_SETLK, &fl)==-1) return -1;
  return 0;
}
static int unlock_exclusive(int fd){
  struct flock fl={.l_type=F_UNLCK,.l_whence=SEEK_SET,.l_start=0,.l_len=0};
  return fcntl(fd, F_SETLK, &fl);
}

static void* client_thread(void* arg){
  int cfd = (int)(intptr_t)arg;
  FILE* io = fdopen(cfd,"r+");
  if(!io){ close(cfd); sem_post(&SEM_SLOTS); return NULL; }
  setvbuf(io,NULL,_IOLBF,0); // line buffered

  pthread_mutex_lock(&G_CONN_M); G_CONN_COUNT++; pthread_mutex_unlock(&G_CONN_M);

  char line[LINE_MAX];
  int my_is_owner=0;
  for(;;){
    if(!fgets(line,sizeof(line), io)) break; // desconexión
    trim_nl(line);
    if(line[0]==0) { fprintf(io,"ERR EMPTY\n"); continue; }

    // split en tokens por espacio
    char *cmd=strtok(line," ");
    if(!cmd){ fprintf(io,"ERR PARSE\n"); continue; }

    // Si hay tx activa de otro, bloquear todas las operaciones
    pthread_mutex_lock(&G.m);
    int busy = (G.tx_active && (!my_is_owner));
    pthread_mutex_unlock(&G.m);
    if(busy){
      fprintf(io,"ERR TX_ACTIVE\n");
      continue;
    }

    // --------- comandos sin estado ---------
    if(!strcasecmp(cmd,"PING")){
      fprintf(io,"OK\n"); continue;
    }
    if(!strcasecmp(cmd,"QUIT")){
      fprintf(io,"BYE\n"); break;
    }

    // --------- BEGIN / COMMIT / ROLLBACK ---------
    if(!strcasecmp(cmd,"BEGIN")){
      pthread_mutex_lock(&G.m);
      if(G.tx_active){
        pthread_mutex_unlock(&G.m);
        fprintf(io,"ERR TX_ACTIVE\n");
        continue;
      }
      int fd = open(G.csv_path, O_RDWR);
      if(fd<0){ pthread_mutex_unlock(&G.m); fprintf(io,"ERR IO\n"); continue; }
      if(lock_exclusive_nb(fd)<0){
        close(fd); pthread_mutex_unlock(&G.m); fprintf(io,"ERR TX_ACTIVE\n"); continue;
      }
      // cargar snapshot
      db_free(&G.tx_db_snapshot);
      if(db_load(G.csv_path, &G.tx_db_snapshot)<0){
        unlock_exclusive(fd); close(fd); pthread_mutex_unlock(&G.m);
        fprintf(io,"ERR LOAD\n"); continue;
      }
      G.tx_active=1; G.tx_owner=pthread_self(); G.tx_fd=fd;
      my_is_owner=1;
      pthread_mutex_unlock(&G.m);
      fprintf(io,"OK\n");
      continue;
    }

    if(!strcasecmp(cmd,"COMMIT")){
      pthread_mutex_lock(&G.m);
      if(!G.tx_active || !pthread_equal(G.tx_owner,pthread_self())){
        pthread_mutex_unlock(&G.m); fprintf(io,"ERR NO_TX\n"); continue;
      }
      if(db_write_atomic(G.csv_path, &G.tx_db_snapshot)<0){
        // no liberamos lock si falló volcado; el cliente podría intentar de nuevo o ROLLBACK
        pthread_mutex_unlock(&G.m); fprintf(io,"ERR COMMIT_IO\n"); continue;
      }
      db_free(&G.tx_db_snapshot);
      unlock_exclusive(G.tx_fd); close(G.tx_fd);
      G.tx_active=0; my_is_owner=0;
      pthread_mutex_unlock(&G.m);
      fprintf(io,"OK\n");
      continue;
    }

    if(!strcasecmp(cmd,"ROLLBACK")){
      pthread_mutex_lock(&G.m);
      if(!G.tx_active || !pthread_equal(G.tx_owner,pthread_self())){
        pthread_mutex_unlock(&G.m); fprintf(io,"ERR NO_TX\n"); continue;
      }
      db_free(&G.tx_db_snapshot);
      unlock_exclusive(G.tx_fd); close(G.tx_fd);
      G.tx_active=0; my_is_owner=0;
      pthread_mutex_unlock(&G.m);
      fprintf(io,"OK\n");
      continue;
    }

    // --------- GET / FIND / ADD / UPDATE / DELETE ---------
    if(!strcasecmp(cmd,"GET")){
      char* sid=strtok(NULL," ");
      if(!sid){ fprintf(io,"ERR ARGS\n"); continue; }
      int id=atoi(sid);
      // si soy el owner y hay tx, leer del snapshot; si no, del archivo
      row_t r; int rc=0;
      pthread_mutex_lock(&G.m);
      if(G.tx_active && pthread_equal(G.tx_owner,pthread_self())){
        rc = db_find_by_id(&G.tx_db_snapshot, id, &r);
        pthread_mutex_unlock(&G.m);
      } else {
        pthread_mutex_unlock(&G.m);
        db_t tmp={0}; if(db_load(G.csv_path,&tmp)<0){ fprintf(io,"ERR LOAD\n"); continue; }
        rc = db_find_by_id(&tmp, id, &r); db_free(&tmp);
      }
      if(rc==0){ fprintf(io,"RESULT 1\n%d,%d,%d,%s\nEND\n", r.id,r.generador,r.pid,r.nombre); }
      else { fprintf(io,"ERR NOT_FOUND\n"); }
      continue;
    }

    if(!strcasecmp(cmd,"FIND")){
      // formato: FIND nombre=<txt>
      char *tok=strtok(NULL," ");
      if(!tok){ fprintf(io,"ERR ARGS\n"); continue; }
      char name[NAME_MAXLEN]={0};
      if(!parse_kv_str(tok,"nombre",name,sizeof(name))){ fprintf(io,"ERR ARGS\n"); continue; }
      row_t* arr=NULL; size_t n=0; int rc=0;
      pthread_mutex_lock(&G.m);
      if(G.tx_active && pthread_equal(G.tx_owner,pthread_self())){
        rc = db_find_by_nombre(&G.tx_db_snapshot, name, &arr, &n);
        pthread_mutex_unlock(&G.m);
      } else {
        pthread_mutex_unlock(&G.m);
        db_t tmp={0}; if(db_load(G.csv_path,&tmp)<0){ fprintf(io,"ERR LOAD\n"); continue; }
        rc = db_find_by_nombre(&tmp, name, &arr, &n); db_free(&tmp);
      }
      if(rc==0){
        fprintf(io,"RESULT %zu\n", n);
        for(size_t i=0;i<n;i++) fprintf(io,"%d,%d,%d,%s\n", arr[i].id,arr[i].generador,arr[i].pid,arr[i].nombre);
        fprintf(io,"END\n"); free(arr);
      } else { fprintf(io,"ERR\n"); }
      continue;
    }

    if(!strcasecmp(cmd,"ADD")){
      // requiere BEGIN previo (ser owner)
      pthread_mutex_lock(&G.m);
      if(!(G.tx_active && pthread_equal(G.tx_owner,pthread_self()))){
        pthread_mutex_unlock(&G.m); fprintf(io,"ERR NO_TX\n"); continue;
      }
      pthread_mutex_unlock(&G.m);
      // parse: ADD nombre=X generador=Y pid=Z
      char *t=NULL; row_t r={.id=-1,.generador=-1,.pid=-1,.nombre=""};
      while((t=strtok(NULL," "))){
        parse_kv_str(t,"nombre",r.nombre,sizeof(r.nombre));
        parse_kv_int(t,"generador",&r.generador);
        parse_kv_int(t,"pid",&r.pid);
      }
      if(!r.nombre[0] || r.generador<0 || r.pid<0){ fprintf(io,"ERR ARGS\n"); continue; }
      pthread_mutex_lock(&G.m);
      r.id = db_max_id(&G.tx_db_snapshot)+1;
      db_add(&G.tx_db_snapshot, &r);
      pthread_mutex_unlock(&G.m);
      fprintf(io,"OK\n");
      continue;
    }

    if(!strcasecmp(cmd,"UPDATE")){
      // UPDATE id=K [nombre=..] [generador=..] [pid=..]
      pthread_mutex_lock(&G.m);
      if(!(G.tx_active && pthread_equal(G.tx_owner,pthread_self()))){
        pthread_mutex_unlock(&G.m); fprintf(io,"ERR NO_TX\n"); continue;
      }
      pthread_mutex_unlock(&G.m);
      char *t=NULL; int id=-1; row_t patch={.id=-1,.generador=-1,.pid=-1,.nombre=""};
      while((t=strtok(NULL," "))){
        parse_kv_int(t,"id",&id);
        parse_kv_str(t,"nombre",patch.nombre,sizeof(patch.nombre));
        parse_kv_int(t,"generador",&patch.generador);
        parse_kv_int(t,"pid",&patch.pid);
      }
      if(id<0){ fprintf(io,"ERR ARGS\n"); continue; }
      pthread_mutex_lock(&G.m);
      int rc = db_update(&G.tx_db_snapshot, id, &patch);
      pthread_mutex_unlock(&G.m);
      if(rc==0) fprintf(io,"OK\n"); else fprintf(io,"ERR NOT_FOUND\n");
      continue;
    }

    if(!strcasecmp(cmd,"DELETE")){
      // DELETE id=K
      pthread_mutex_lock(&G.m);
      if(!(G.tx_active && pthread_equal(G.tx_owner,pthread_self()))){
        pthread_mutex_unlock(&G.m); fprintf(io,"ERR NO_TX\n"); continue;
      }
      pthread_mutex_unlock(&G.m);
      char *t=strtok(NULL," "); int id=-1;
      if(!t || !parse_kv_int(t,"id",&id)){ fprintf(io,"ERR ARGS\n"); continue; }
      pthread_mutex_lock(&G.m);
      int rc=db_delete(&G.tx_db_snapshot, id);
      pthread_mutex_unlock(&G.m);
      if(rc==0) fprintf(io,"OK\n"); else fprintf(io,"ERR NOT_FOUND\n");
      continue;
    }

    // comando desconocido
    fprintf(io,"ERR UNKNOWN\n");
  }

  // cleanup por desconexión: si era owner de tx, rollback implícito
  pthread_mutex_lock(&G.m);
  if(G.tx_active && pthread_equal(G.tx_owner,pthread_self())){
    db_free(&G.tx_db_snapshot);
    unlock_exclusive(G.tx_fd); close(G.tx_fd);
    G.tx_active=0;
  }
  pthread_mutex_unlock(&G.m);

  fclose(io); // cierra cfd
  sem_post(&SEM_SLOTS);
  pthread_mutex_lock(&G_CONN_M); G_CONN_COUNT--; pthread_mutex_unlock(&G_CONN_M);
  return NULL;
}

int main(int argc, char** argv){
  server_cfg_t cfg = {.max_active=4,.backlog=16,.bind_ip="0.0.0.0",.port=0,.csv_path=NULL};
  for(int i=1;i<argc;i++){
    if(!strcmp(argv[i],"-p") && i+1<argc) cfg.port=(uint16_t)atoi(argv[++i]);
    else if(!strcmp(argv[i],"-f") && i+1<argc) cfg.csv_path=argv[++i];
    else if(!strcmp(argv[i],"-n") && i+1<argc) cfg.max_active=atoi(argv[++i]);
    else if(!strcmp(argv[i],"-m") && i+1<argc) cfg.backlog=atoi(argv[++i]);
    else if(!strcmp(argv[i],"-H") && i+1<argc) cfg.bind_ip=argv[++i];
    else if(!strcmp(argv[i],"--help")){ usage(argv[0]); return 0; }
  }
  if(cfg.port==0 || !cfg.csv_path){ usage(argv[0]); return 1; }

  // init globals
  pthread_mutex_init(&G.m,NULL);
  sem_init(&SEM_SLOTS, 0, cfg.max_active);
  G.csv_path = cfg.csv_path;

  // socket
  int s = socket(AF_INET, SOCK_STREAM, 0);
  if(s<0) die("socket");
  int one=1; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one));
  struct sockaddr_in a={0};
  a.sin_family=AF_INET; a.sin_port=htons(cfg.port);
  if(inet_pton(AF_INET,cfg.bind_ip,&a.sin_addr)<=0) die("bind ip");
  if(bind(s,(struct sockaddr*)&a,sizeof(a))<0) die("bind");
  if(listen(s, cfg.backlog)<0) die("listen");

  fprintf(stderr,"Servidor escuchando en %s:%d (N=%d, backlog=%d) CSV=%s\n",
    cfg.bind_ip, cfg.port, cfg.max_active, cfg.backlog, cfg.csv_path);

  for(;;){
    struct sockaddr_in ca; socklen_t calen=sizeof(ca);
    int cfd = accept(s,(struct sockaddr*)&ca,&calen);
    if(cfd<0){ if(errno==EINTR) continue; perror("accept"); continue; }

    // limitar N: si no hay slot, devolver error y cerrar
    if(sem_trywait(&SEM_SLOTS)!=0){
      const char* msg="ERR SERVER_BUSY\n";
      send(cfd,msg,strlen(msg),0);
      close(cfd);
      continue;
    }

    pthread_t th;
    if(pthread_create(&th,NULL,client_thread,(void*)(intptr_t)cfd)!=0){
      const char* msg="ERR SERVER\n"; send(cfd,msg,strlen(msg),0);
      close(cfd); sem_post(&SEM_SLOTS); continue;
    }
    pthread_detach(th);
  }
}
