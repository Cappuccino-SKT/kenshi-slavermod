import sys, os, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.stdout.reconfigure(encoding='utf-8')
import kmod
W = r'C:\Program Files (x86)\Steam\steamapps\workshop\content\233860'
K = r'C:\Program Files (x86)\Steam\steamapps\common\Kenshi\data'
d = lambda b: b.decode('utf-8', 'replace')
names = {}
files = [os.path.join(K, 'rebirth.mod'), os.path.join(K, 'Newwworld.mod'), os.path.join(W, '2850143028', 'Second Life of Kenshi.mod'), os.path.join(W, '3826814355', 'Second Life of Kenshi PT-BR.mod'), os.path.join(W, '3777203526', 'QueenBirth.mod')]
mods = {f: kmod.read(f) for f in files}
for f, m in mods.items():
    for rec in m['records']:
        names[d(rec['sid'])] = d(rec['name'])
for f, m in mods.items():
    for rec in m['records']:
        if d(rec['sid']) in ('317-Second Life of Kenshi.mod', '498-Second Life of Kenshi.mod', '57335-Newwworld.mod'):
            print('==', os.path.basename(f), 'rtype', rec['rtype'], repr(d(rec['name'])), d(rec['sid']), 'flags', rec['flags'])
            print('   ints', [(d(a), struct.unpack('<i', b)[0]) for a, b in rec['ints']])
            print('   bools', [(d(a), b[0]) for a, b in rec['bools']])
            print('   strings', [(d(a), d(b)[:60]) for a, b in rec['strings']][:6])
            for c, items in rec['extra']:
                print('   list', d(c), [(d(x[0]), names.get(d(x[0]), '?'), struct.unpack('<3i', x[1])) for x in items])
