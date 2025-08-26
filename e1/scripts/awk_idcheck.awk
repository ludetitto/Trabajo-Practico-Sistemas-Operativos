BEGIN{FS=","}
NR==1{next}
{
  id=$1+0
  if (seen[id]++) dup=1
  ids[++n]=id
}
END{
  asort(ids)
  for(i=2;i<=n;i++) if (ids[i] != ids[i-1]+1){ print "ERROR: salto en ID entre", ids[i-1], "y", ids[i]; exit 1 }
  if (dup){ print "ERROR: IDs duplicados"; exit 1 }
  print "OK: IDs correlativos y sin duplicados"
}
