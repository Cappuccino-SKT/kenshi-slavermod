import sys, os, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.stdout.reconfigure(encoding='utf-8')
import kmod
W = r'C:\Program Files (x86)\Steam\steamapps\workshop\content\233860'
K = r'C:\Program Files (x86)\Steam\steamapps\common\Kenshi\data'
d = lambda b: b.decode('utf-8', 'replace')
for f in [os.path.join(K, 'rebirth.mod'), os.path.join(W, '2850143028', 'Second Life of Kenshi.mod'), os.path.join(W, '3826814355', 'Second Life of Kenshi PT-BR.mod'), os.path.join(W, '3777203526', 'QueenBirth.mod')]:
    for rec in kmod.read(f)['records']:
        if rec['rtype'] != 64:
            continue
        n = d(rec['name'])
        if 'col' in n.lower() or 'hive' in n.lower() or 'colm' in n.lower():
            print('==', os.path.basename(f), repr(n), d(rec['sid']), 'flags', rec['flags'])
            for c, items in rec['extra']:
                print('   ', d(c), [(d(x[0]), struct.unpack('<3i', x[1])) for x in items][:12])
