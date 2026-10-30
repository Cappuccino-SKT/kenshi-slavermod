import sys,os; sys.path.insert(0,sys.argv[1]); sys.stdout.reconfigure(encoding='utf-8')
import kmod
D=r'C:\Program Files (x86)\Steam\steamapps\common\Kenshi\data'
for f in ('gamedata.base','Newwworld.mod','rebirth.mod'):
    m=kmod.read(os.path.join(D,f))
    for r in m['records']:
        if r['rtype']==10:
            n=r['name'].decode('utf-8','replace')
            if 'slave' in n.lower() or 'escap' in n.lower(): print(f, r['sid'].decode(), '|', n)
