# strrefs.py <pylib> <string> [ctx]: finds rip-relative lea references to an exact C string and shows nearby calls
import sys, os, struct, re
sys.path.insert(0, sys.argv[1])
import pefile, capstone
K = r'C:\Program Files (x86)\Steam\steamapps\common\Kenshi'
R = os.path.join(K, 'RE_Kenshi', 'RVAs')
gog = struct.unpack('<%dI' % (30520 // 4), open(os.path.join(R, 'GOG_1.0.65.br'), 'rb').read())
steam = struct.unpack('<%dI' % (30520 // 4), open(os.path.join(R, 'Steam_1.0.65.br'), 'rb').read())
inv = {}
for i, v in enumerate(steam): inv.setdefault(v, i)
names = {}
inc = r'C:\KenshiDev\deps\KenshiLib_Examples_deps\KenshiLib\Include\kenshi'
pat = re.compile(r'^\s*(?:virtual\s+|static\s+)?(.*?)\s*\(.*RVA = (0x[0-9A-F]+)')
for root, _, files in os.walk(inc):
    for f in files:
        if f.endswith('.h'):
            cls = None
            for line in open(os.path.join(root, f), encoding='utf-8', errors='replace'):
                m = re.match(r'^(?:class|struct)\s+(\w+)', line)
                if m: cls = m.group(1)
                m = pat.match(line)
                if m: names.setdefault(int(m.group(2), 16), '%s::%s' % (cls, m.group(1).split()[-1] if m.group(1) else '?'))
pe = pefile.PE(os.path.join(K, 'RE_Kenshi', 'kenshi_x64.exe'), fast_load=True)
data = pe.get_memory_mapped_image(); base = pe.OPTIONAL_HEADER.ImageBase
s = sys.argv[2].encode() + b'\0'
locs = [m.start() for m in re.finditer(re.escape(s), data) if data[m.start()-1] == 0]
text = [x for x in pe.sections if x.Name.startswith(b'.text')][0]
lo, hi = text.VirtualAddress, text.VirtualAddress + text.Misc_VirtualSize
# thunk map: jmp rel32 -> function
def resolve(t):
    if data[t] == 0xE9:
        t2 = t + 5 + struct.unpack_from('<i', data, t + 1)[0]
        return t2
    return t
refs = []
for loc in locs:
    for m in re.finditer(rb'\x48\x8d[\x05\x0d\x15\x1d\x25\x2d\x35\x3d]|\x4c\x8d[\x05\x0d\x15\x1d\x25\x2d\x35\x3d]', data[lo:hi]):
        a = lo + m.start()
        if a + 7 + struct.unpack_from('<i', data, a + 3)[0] == loc:
            refs.append(a)
print('string at', [hex(l) for l in locs], 'refs', len(refs))
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
ctx = int(sys.argv[3]) if len(sys.argv) > 3 else 12
for a in refs[:10]:
    print('---- ref at', hex(a))
    k = 0
    for ins in md.disasm(data[a:a + 200], base + a):
        extra = ''
        if ins.mnemonic == 'call' and ins.op_str.startswith('0x'):
            t = resolve(int(ins.op_str, 16) - base)
            if t in inv:
                g = gog[inv[t]]; extra = '  ; ' + names.get(g, 'gog=' + hex(g))
        print('  %x  %-7s %s%s' % (ins.address - base, ins.mnemonic, ins.op_str, extra))
        k += 1
        if k >= ctx: break
