import re, json, sys
s = open(r'C:\Users\Felipe\Desktop\SlaverMod\SlaverMod.cpp', encoding='utf-8').read()
lit = r'"((?:[^"\\]|\\.)*)"'
pat = re.compile(r'\b[TL]\(\s*' + lit + r'\s*,\s*' + lit + r'\s*\)', re.S)
seen = {}
for m in pat.finditer(s):
    if m.group(1) not in seen:
        seen[m.group(1)] = m.group(2)
json.dump(list(seen.items()), open(sys.argv[1], 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
print(len(seen), 'strings')
for m in re.finditer(r'g_pt \?[^;]*', s):
    print('PT-TERNARY:', m.group(0)[:160])
