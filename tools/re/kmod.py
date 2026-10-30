"""Minimal Kenshi .mod reader/writer (FCS format 16/17), lossless round-trip."""
import struct, io

class R:
    def __init__(s, b): s.b = b; s.p = 0
    def i(s): v = struct.unpack_from('<i', s.b, s.p)[0]; s.p += 4; return v
    def f(s): v = struct.unpack_from('<f', s.b, s.p)[0]; s.p += 4; return v
    def raw(s, n): v = s.b[s.p:s.p + n]; s.p += n; return v
    def s(s):
        n = s.i(); v = s.b[s.p:s.p + n]; s.p += n; return v  # keep bytes

class W:
    def __init__(s): s.o = io.BytesIO()
    def i(s, v): s.o.write(struct.pack('<i', v))
    def raw(s, b): s.o.write(b)
    def s(s, b): s.i(len(b)); s.o.write(b)

def read(path):
    r = R(open(path, 'rb').read())
    m = {}
    m['type'] = r.i()
    if m['type'] == 17:
        hsize = r.i()
        start = r.p
        h = {}
        h['version'] = r.i(); h['author'] = r.s(); h['description'] = r.s()
        h['dependencies'] = r.s(); h['references'] = r.s()
        h['rest'] = r.raw(hsize - (r.p - start))
        m['header'] = h
    elif m['type'] != 16:
        raise ValueError('unknown type %d' % m['type'])
    m['lastid'] = r.i()
    n = r.i()
    recs = []
    for _ in range(n):
        rec = {}
        rec['inst'] = r.i(); rec['rtype'] = r.i(); rec['id'] = r.i()
        rec['name'] = r.s(); rec['sid'] = r.s(); rec['flags'] = r.i()
        rec['bools'] = [(r.s(), r.raw(1)) for _ in range(r.i())]
        rec['floats'] = [(r.s(), r.raw(4)) for _ in range(r.i())]
        rec['ints'] = [(r.s(), r.raw(4)) for _ in range(r.i())]
        rec['vec3'] = [(r.s(), r.raw(12)) for _ in range(r.i())]
        rec['vec4'] = [(r.s(), r.raw(16)) for _ in range(r.i())]
        rec['strings'] = [[r.s(), r.s()] for _ in range(r.i())]
        rec['files'] = [[r.s(), r.s()] for _ in range(r.i())]
        cats = []
        for _ in range(r.i()):
            cname = r.s(); items = []
            for _ in range(r.i()):
                items.append((r.s(), r.raw(12)))
            cats.append((cname, items))
        rec['extra'] = cats
        insts = []
        for _ in range(r.i()):
            iid = r.s(); tgt = r.s(); geo = r.raw(28)
            states = [r.s() for _ in range(r.i())]
            insts.append((iid, tgt, geo, states))
        rec['instances'] = insts
        recs.append(rec)
    m['records'] = recs
    m['tail'] = r.b[r.p:]
    return m

def write(m, path=None):
    w = W()
    w.i(m['type'])
    if m['type'] == 17:
        h = m['header']; hw = W()
        hw.i(h['version']); hw.s(h['author']); hw.s(h['description'])
        hw.s(h['dependencies']); hw.s(h['references']); hw.raw(h['rest'])
        hb = hw.o.getvalue(); w.i(len(hb)); w.raw(hb)
    w.i(m['lastid']); w.i(len(m['records']))
    for rec in m['records']:
        w.i(rec['inst']); w.i(rec['rtype']); w.i(rec['id'])
        w.s(rec['name']); w.s(rec['sid']); w.i(rec['flags'])
        for key in ('bools', 'floats', 'ints', 'vec3', 'vec4'):
            w.i(len(rec[key]))
            for k, v in rec[key]: w.s(k); w.raw(v)
        for key in ('strings', 'files'):
            w.i(len(rec[key]))
            for k, v in rec[key]: w.s(k); w.s(v)
        w.i(len(rec['extra']))
        for cname, items in rec['extra']:
            w.s(cname); w.i(len(items))
            for t, v in items: w.s(t); w.raw(v)
        w.i(len(rec['instances']))
        for iid, tgt, geo, states in rec['instances']:
            w.s(iid); w.s(tgt); w.raw(geo); w.i(len(states))
            for st in states: w.s(st)
    w.raw(m['tail'])
    b = w.o.getvalue()
    if path: open(path, 'wb').write(b)
    return b
