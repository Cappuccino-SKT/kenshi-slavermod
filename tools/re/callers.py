import sys, os, struct, re
sys.path.insert(0, sys.argv[1])
import pefile, capstone
K = r'C:\Program Files (x86)\Steam\steamapps\common\Kenshi'
pe = pefile.PE(os.path.join(K, 'RE_Kenshi', 'kenshi_x64.exe'), fast_load=True)
data = pe.get_memory_mapped_image(); base = pe.OPTIONAL_HEADER.ImageBase
target = int(sys.argv[2], 16)
text = [s for s in pe.sections if s.Name.startswith(b'.text')][0]
lo, hi = text.VirtualAddress, text.VirtualAddress + text.Misc_VirtualSize
targets = {target}
# thunks: E9 rel32 -> target
for m in re.finditer(b'\xE9', data[lo:hi]):
    a = lo + m.start()
    rel = struct.unpack_from('<i', data, a + 1)[0]
    if a + 5 + rel == target:
        targets.add(a)
calls = []
for m in re.finditer(b'\xE8', data[lo:hi]):
    a = lo + m.start()
    rel = struct.unpack_from('<i', data, a + 1)[0]
    if a + 5 + rel in targets:
        calls.append(a)
print('thunks', [hex(t) for t in targets], 'calls', len(calls))
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
for c in calls[:int(sys.argv[3])]:
    print('---- call at', hex(c))
    ins = list(md.disasm(data[c - 90:c + 5], base + c - 90))
    for i in ins[-14:]:
        print('  %x  %-7s %s' % (i.address - base, i.mnemonic, i.op_str))
