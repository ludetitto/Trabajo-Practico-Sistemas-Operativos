#include "common.h"

static void usage(const char* p){
  fprintf(stderr,"Uso: %s -h <host> -p <puerto>\n", p);
}

int main(int argc, char** argv){
  const char* host="127.0.0.1"; int port=0;
  for(int i=1;i<argc;i++){
    if(!strcmp(argv[i],"-h") && i+1<argc) host=argv[++i];
    else if(!strcmp(argv[i],"-p") && i+1<argc) port=atoi(argv[++i]);
    else if(!strcmp(argv[i],"--help")){ usage(argv[0]); return 0; }
  }
  if(port<=0){ usage(argv[0]); return 1; }

  int s = socket(AF_INET, SOCK_STREAM, 0);
  if(s<0) die("socket");
  struct sockaddr_in a={0}; a.sin_family=AF_INET; a.sin_port=htons(port);
  if(inet_pton(AF_INET, host, &a.sin_addr)<=0) die("host");
  if(connect(s,(struct sockaddr*)&a,sizeof(a))<0) die("connect");
  FILE* io = fdopen(s,"r+"); if(!io) die("fdopen");
  setvbuf(io,NULL,_IOLBF,0);

  printf("Conectado a %s:%d. Comandos: PING | GET <id> | FIND nombre=VAL | BEGIN | ADD nombre=.. generador=.. pid=.. | UPDATE id=.. [..] | DELETE id=.. | COMMIT | ROLLBACK | QUIT\n", host, port);

  char in[LINE_MAX], out[LINE_MAX];
  while(1){
    printf("> "); fflush(stdout);
    if(!fgets(in,sizeof(in), stdin)) break;
    trim_nl(in);
    if(!*in) continue;
    fprintf(io,"%s\n", in); fflush(io);
    // leer respuesta (puede ser multi-línea)
    while(fgets(out,sizeof(out), io)){
      fputs(out, stdout);
      if(!strncmp(out,"OK",2) || !strncmp(out,"ERR",3) || !strncmp(out,"BYE",3)) break;
      if(!strncmp(out,"END",3)) break;
      if(!strncmp(out,"RESULT",6)) continue; // seguir leyendo
    }
    if(!strcasecmp(in,"QUIT")) break;
  }

  fclose(io);
  return 0;
}
