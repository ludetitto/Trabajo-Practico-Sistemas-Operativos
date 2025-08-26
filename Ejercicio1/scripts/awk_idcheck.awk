BEGIN { FS=","; min=0; max=0; dup=0 }
NR==1 { next }
{ id=$1+0; if(id in seen) dup=1; seen[id]=1; if(min==0||id<min)min=id; if(id>max)max=id }
END {
  gap=0; for(i=min;i<=max;i++) if(!(i in seen)) gap=1
  if(!dup && !gap){ print "OK: IDs correlativos y sin duplicados"; exit 0 }
  if(dup) print "ERROR: IDs duplicados"; if(gap) print "ERROR: Faltan IDs en el rango"; exit 1
}
