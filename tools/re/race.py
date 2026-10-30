import sys,os,struct; sys.path.insert(0,sys.argv[1]); sys.stdout.reconfigure(encoding='utf-8')
import kmod
W=r'C:\Program Files (x86)\Steam\steamapps\workshop\content\233860'
D=r'C:\Program Files (x86)\Steam\steamapps\common\Kenshi\data'
files=[os.path.join(D,f) for f in ('gamedata.base','Newwworld.mod','Dialogue.mod','rebirth.mod')]+[os.path.join(W,'2850143028','Second Life of Kenshi.mod'),os.path.join(W,'3826814355','Second Life of Kenshi PT-BR.mod'),os.path.join(W,'3777203526','QueenBirth.mod')]
sid=sys.argv[2].encode()
for f in files:
    m=kmod.read(f)
    for r in m['records']:
        if r['sid']==sid:
            print('=====',os.path.basename(f),'name=',r['name'].decode('utf-8','replace'),'flags',hex(r['flags']&0xffffffff))
            for k,v in r['floats']:
                if any(w in k.decode().lower() for w in ('hunger','food','eat','starv','metab')): print('  F',k.decode(),struct.unpack('<f',v)[0])
            for k,v in r['bools']:
                if any(w in k.decode().lower() for w in ('hunger','food','eat','starv','metab','robot','undead','can ')): print('  B',k.decode(),v[0])
            for k,v in r['ints']:
                if any(w in k.decode().lower() for w in ('hunger','food','eat','starv','metab')): print('  I',k.decode(),struct.unpack('<i',v)[0])
            for c,items in r['extra']:
                if any(w in c.decode().lower() for w in ('food','eat')): print('  X',c.decode(),[t.decode() for t,_ in items][:10])
