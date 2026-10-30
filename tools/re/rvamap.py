import sys, struct
sys.path.insert(0, sys.argv[1])
import pefile, capstone
R = r'C:\Program Files (x86)\Steam\steamapps\common\Kenshi\RE_Kenshi\RVAs'
gog = struct.unpack('<%dI' % (30520 // 4), open(R + r'\GOG_1.0.65.br', 'rb').read())
steam = struct.unpack('<%dI' % (30520 // 4), open(R + r'\Steam_1.0.65.br', 'rb').read())
idx = {v: i for i, v in enumerate(gog)}
def to_steam(r):
    return steam[idx[r]] if r in idx else None
pe = pefile.PE(r'C:\Program Files (x86)\Steam\steamapps\common\Kenshi\RE_Kenshi\kenshi_x64.exe', fast_load=True)
data = pe.get_memory_mapped_image(); base = pe.OPTIONAL_HEADER.ImageBase
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
inv = {}
for i, v in enumerate(steam): inv.setdefault(v, i)
for spec in sys.argv[2:]:
    name, rva, n = spec.split(':'); rva = int(rva, 16); n = int(n)
    s = to_steam(rva)
    print('=====', name, 'gog', hex(rva), '-> steam', hex(s) if s else None)
    if not s: continue
    k = 0
    for ins in md.disasm(data[s:s + 600], base + s):
        extra = ''
        if ins.mnemonic in ('call', 'jmp') and ins.op_str.startswith('0x'):
            t = int(ins.op_str, 16) - base
            if t in inv: extra = '   ; gog=' + hex(gog[inv[t]])
        print('  %x  %-7s %s%s' % (ins.address - base, ins.mnemonic, ins.op_str, extra))
        k += 1
        if k >= n or ins.mnemonic == 'ret': break
