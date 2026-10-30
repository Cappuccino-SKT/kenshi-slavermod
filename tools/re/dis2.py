# dis2.py <pylib> name:gogRVA:maxInstr ...
# Disassembles Steam kenshi_x64.exe functions given GOG RVAs; annotates calls with
# KenshiLib header names and rip-relative references with strings.
import sys, struct, os, re
sys.path.insert(0, sys.argv[1])
import pefile, capstone
K = r'C:\Program Files (x86)\Steam\steamapps\common\Kenshi'
R = os.path.join(K, 'RE_Kenshi', 'RVAs')
gog = struct.unpack('<%dI' % (30520 // 4), open(os.path.join(R, 'GOG_1.0.65.br'), 'rb').read())
steam = struct.unpack('<%dI' % (30520 // 4), open(os.path.join(R, 'Steam_1.0.65.br'), 'rb').read())
idx = {v: i for i, v in enumerate(gog)}
inv = {}
for i, v in enumerate(steam):
    inv.setdefault(v, i)

names = {}
inc = r'C:\KenshiDev\deps\KenshiLib_Examples_deps\KenshiLib\Include\kenshi'
pat = re.compile(r'^\s*(?:virtual\s+|static\s+)?(.*?)\s*\(.*RVA = (0x[0-9A-F]+)')
for root, _, files in os.walk(inc):
    for f in files:
        if not f.endswith('.h'):
            continue
        cls = None
        for line in open(os.path.join(root, f), encoding='utf-8', errors='replace'):
            m = re.match(r'^(?:class|struct)\s+(\w+)', line)
            if m:
                cls = m.group(1)
            m = pat.match(line)
            if m:
                r = int(m.group(2), 16)
                fn = m.group(1).split()[-1] if m.group(1) else '?'
                names.setdefault(r, '%s::%s' % (cls, fn))

def steam_of(r):
    return steam[idx[r]] if r in idx else None

pe = pefile.PE(os.path.join(K, 'RE_Kenshi', 'kenshi_x64.exe'), fast_load=True)
data = pe.get_memory_mapped_image()
base = pe.OPTIONAL_HEADER.ImageBase
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
md.detail = True

def string_at(rva):
    if rva < 0 or rva >= len(data):
        return None
    s = data[rva:rva + 80]
    end = s.find(b'\0')
    if end < 3:
        return None
    t = s[:end]
    if all(32 <= c < 127 for c in t):
        return t.decode()
    return None

for spec in sys.argv[2:]:
    name, rva, n = spec.split(':')
    rva = int(rva, 16); n = int(n)
    s = steam_of(rva)
    print('=====', name, 'gog', hex(rva), '-> steam', hex(s) if s else None)
    if not s:
        continue
    k = 0
    for ins in md.disasm(data[s:s + n * 10], base + s):
        extra = ''
        if ins.mnemonic in ('call', 'jmp') and ins.op_str.startswith('0x'):
            t = int(ins.op_str, 16) - base
            if data[t] == 0xE9: t = t + 5 + struct.unpack_from("<i", data, t + 1)[0]
            if t in inv:
                g = gog[inv[t]]
                extra = '   ; ' + names.get(g, 'gog=' + hex(g))
        if 'rip' in ins.op_str:
            for op in ins.operands:
                if op.type == capstone.x86.X86_OP_MEM and op.mem.base == capstone.x86.X86_REG_RIP:
                    tgt = ins.address + ins.size + op.mem.disp - base
                    st = string_at(tgt)
                    if st:
                        extra += '   ; "%s"' % st
                    elif tgt in inv:
                        g = gog[inv[tgt]]
                        extra += '   ; ' + names.get(g, 'gog=' + hex(g))
        print('  %x  %-7s %s%s' % (ins.address - base, ins.mnemonic, ins.op_str, extra))
        k += 1
        if k >= n:
            break
