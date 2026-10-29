import re,math
def load_siec(path, incdir):
    lines=open(path,encoding='latin-1').read().replace('\r','').split('\n')
    heads={}
    def head(name):
        if name in heads: return heads[name]
        f=incdir+'/'+name.split('/',1)[1] if '/' in name else None
        h=None
        try:
            t=open(f,encoding='latin-1').read()
            m=re.search(r'//rainsted,([^\n]*)',t)
            if m:
                h={k:float(v) for k,v in (kv.split('=') for kv in m.group(1).strip().split(','))}
            h=(h,t)
        except Exception: h=(None,None)
        heads[name]=h; return h
    incs=[];spans=[]
    i=0
    while i<len(lines):
        l=lines[i].split()
        if l and l[0]=='include':
            incs.append({'line':i,'file':l[1],'params':l[2:-1] if l[-1]=='end' else l[2:]})
        elif len(l)>4 and l[0]=='node' and l[4]=='traction':
            body=[]
            j=i
            while not lines[j].strip().endswith('endtraction'): j+=1
            blk=' '.join(lines[i:j+1]).split()
            # node rmax rmin name traction power V I R mat thick dmg p1x p1y p1z p2.. p3.. p4.. minh seg wires off vis ...
            nums=blk[5:]
            spans.append({'line':i,'end':j,'name':blk[3],'tok':blk})
            i=j
        i+=1
    return lines,incs,spans,head
