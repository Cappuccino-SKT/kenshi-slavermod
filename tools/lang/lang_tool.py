"""SlaverMod translation files (mod/SlaverMod/lang/*.txt).

    python tools/lang/lang_tool.py update     rewrite _template.txt and refresh every language file
    python tools/lang/lang_tool.py check      only report how much of each language is translated
    python tools/lang/lang_tool.py merge <file> [code]
                                              take a translator's file: keep its translations, put it
                                              in the current layout as lang/<code>.txt, report gaps

Each language file is UTF-8 text:

    [info]
    name = "Español"        shown in the MCM language list
    squad = "Esclavos"      default slave squad name
    author = "..."          credit
    [text]
    "English text" = "translation"
    [lines]
    EnslaveMaster = "line|line|line"
    [mod]
    "1-SlaverMod.mod|description" = "translation"     (texts of SlaverMod.mod, swapped in game)

The English texts are every T("English", "Português") / L(...) literal in SlaverMod.cpp, the
speech lines of LoadLines() and the visible strings of SlaverMod.mod. "update" keeps every
translation that still has an English text, adds the new ones with an empty translation (empty =
stays in English) and moves translations whose English text is gone to [obsolete].
"""
import os, re, shutil, sys
sys.stdout.reconfigure(encoding="utf-8")

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
CPP = os.path.join(REPO, 'SlaverMod.cpp')
MOD = os.path.join(REPO, 'mod', 'SlaverMod', 'SlaverMod.mod')
LANG = os.path.join(REPO, 'mod', 'SlaverMod', 'lang')
INFO_KEYS = ['name', 'squad', 'author']
# record types whose texts players see: 1 CHARACTER (names), 64 NEW_GAME_STARTOFF (game start)
MOD_TYPES = {1: ['name'], 64: ['name', 'description', 'difficulty', 'style']}

LIT = r'"((?:[^"\\]|\\.)*)"'


def c_unescape(raw):
    """C string literal body -> text (UTF-8 bytes in \\x escapes become characters)."""
    out = bytearray()
    i = 0
    b = raw.encode('utf-8')
    while i < len(b):
        c = b[i]
        if c == 0x5C and i + 1 < len(b):
            n = chr(b[i + 1])
            if n == 'x':
                m = re.match(rb'[0-9a-fA-F]{1,2}', b[i + 2:i + 4])
                out.append(int(m.group(0), 16))
                i += 2 + len(m.group(0))
                continue
            out += {'n': b'\n', 't': b'\t', '"': b'"', '\\': b'\\', "'": b"'"}.get(n, n.encode())
            i += 2
            continue
        out.append(c)
        i += 1
    return out.decode('utf-8')


def source_texts():
    s = open(CPP, encoding='utf-8').read()
    # adjacent literals ("a" "b") are one literal for the compiler
    pair = re.compile(r'\b[TL]\(\s*((?:' + LIT + r'\s*)+),\s*((?:' + LIT + r'\s*)+)\)', re.S)
    texts, seen = [], set()
    for m in pair.finditer(s):
        en = ''.join(c_unescape(x) for x in re.findall(LIT, m.group(1)))
        if en not in seen:
            seen.add(en)
            texts.append(en)
    lines = []
    body = s[s.index('static void LoadLines()'):]
    body = body[:body.index('};')]
    for m in re.finditer(r'\{\s*' + LIT + r',\s*' + LIT + r',\s*' + LIT + r'\s*\}', body):
        lines.append((c_unescape(m.group(1)), c_unescape(m.group(2))))
    return texts, lines


def mod_texts():
    """[(key 'sid|field', English)] of SlaverMod.mod."""
    sys.path.insert(0, os.path.join(REPO, 'tools', 're'))
    import kmod
    out = []
    for r in kmod.read(MOD)['records']:
        fields = MOD_TYPES.get(r['rtype'])
        if not fields:
            continue
        sid = r['sid'].decode('utf-8')
        strings = {k.decode('utf-8'): v.decode('utf-8') for k, v in r['strings']}
        for f in fields:
            en = r['name'].decode('utf-8') if f == 'name' else strings.get(f, '')
            if en:
                out.append((sid + '|' + f, en))
    return out


def q(text):
    return '"' + text.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n') + '"'


def parse_quoted(line, p):
    while p < len(line) and line[p] in ' \t':
        p += 1
    if p >= len(line) or line[p] != '"':
        return None, p
    out = []
    p += 1
    while p < len(line):
        c = line[p]
        if c == '"':
            return ''.join(out), p + 1
        if c == '\\' and p + 1 < len(line):
            p += 1
            out.append('\n' if line[p] == 'n' else line[p])
        else:
            out.append(c)
        p += 1
    return None, p


def read_lang(path):
    """-> dict of sections: info, text, lines, mod, obsolete (each key -> translation)."""
    d = {k: {} for k in ('info', 'text', 'lines', 'mod', 'obsolete')}
    section = ''
    for raw in open(path, encoding='utf-8-sig').read().splitlines():
        line = raw.strip()
        if not line or line[0] in ';#':
            continue
        if line.startswith('['):
            section = line[1:line.index(']')]
            continue
        if section not in d:
            continue
        if section in ('text', 'mod', 'obsolete'):
            key, p = parse_quoted(line, 0)
            if key is None:
                print('  cannot read:', raw)
                continue
            p = line.find('=', p)
            val, _ = parse_quoted(line, p + 1)
        else:
            if '=' not in line:
                continue
            key, rest = line.split('=', 1)
            key = key.strip()
            val, _ = parse_quoted(rest, 0)
        d[section][key] = val or ''
    return d


HEADER = '''; SlaverMod translation{title}
; UTF-8 text. Each [text] line is   "English text" = "translation"
; - Only change the right side. Keep the spaces at the start or end of a text, and keep \\" and \\n.
; - An empty translation ("") stays in English, so you can translate a little at a time.
; - [lines] are speech bubbles: several lines separated by |, one is picked at random.
; - [mod] are the texts of the "Slave Hunters" game start (name, description...) and its characters;
;   the English text is in the comment above each one.
; - Save as <language code>.txt in this folder (es.txt, zh.txt, de.txt...). With Language=auto the
;   mod picks the file of the game's language (zh_CN -> zh_cn.txt or zh.txt); or set Language=<code>.
; - Guide and how to send your translation: github.com/Cappuccino-SKT/kenshi-slavermod/blob/main/docs/TRANSLATING.md
'''


def write_lang(path, title, d, texts, speech, modt, only_mod=False):
    out = [HEADER.format(title=title)]
    out.append('[info]')
    for k in INFO_KEYS:
        out.append('%s = %s' % (k, q(d['info'].get(k, ''))))
    out.append('')
    if not only_mod:
        out.append('[text]')
        for en in texts:
            out.append('%s = %s' % (q(en), q(d['text'].get(en, ''))))
        out.append('')
        out.append('[lines]')
        for key, en in speech:
            out.append('; ' + en)
            out.append('%s = %s' % (key, q(d['lines'].get(key, ''))))
        out.append('')
    out.append('[mod]')
    for key, en in modt:
        out.append('; ' + en.replace('\n', ' '))
        out.append('%s = %s' % (q(key), q(d['mod'].get(key, ''))))
    gone = dict(d['obsolete'])
    if not only_mod:
        gone.update({k: v for k, v in d['text'].items() if k not in set(texts) and v})
    if gone:
        out.append('')
        out.append('[obsolete]')
        out.append('; translations whose English text is no longer in the mod (not used)')
        for k, v in gone.items():
            out.append('%s = %s' % (q(k), q(v)))
    open(path, 'w', encoding='utf-8', newline='\r\n').write('\n'.join(out) + '\n')


def report(name, d, texts, speech, modt):
    done = sum(1 for t in texts if d['text'].get(t))
    sdone = sum(1 for k, _ in speech if d['lines'].get(k))
    mdone = sum(1 for k, _ in modt if d['mod'].get(k))
    print('  %-12s %3d/%d texts, %2d/%d speech keys, %d/%d game start texts  (%s)' %
          (name, done, len(texts), sdone, len(speech), mdone, len(modt), d['info'].get('name', '')))


def check_pieces(d, texts):
    """Translations that lost the leading/trailing space of the English piece."""
    for en in texts:
        tr = d['text'].get(en)
        if tr and ((en[:1] == ' ') != (tr[:1] == ' ') or (en[-1:] == ' ') != (tr[-1:] == ' ')):
            print('    space differs: %s -> %s' % (q(en), q(tr)))


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else 'check'
    texts, speech = source_texts()
    modt = mod_texts()
    print('%d texts, %d speech keys in SlaverMod.cpp, %d texts in SlaverMod.mod' % (len(texts), len(speech), len(modt)))
    os.makedirs(LANG, exist_ok=True)
    if cmd == 'merge':
        src = sys.argv[2]
        d = read_lang(src)
        code = sys.argv[3] if len(sys.argv) > 3 else os.path.splitext(os.path.basename(src))[0].lower()
        dest = os.path.join(LANG, code + '.txt')
        if os.path.exists(dest):
            shutil.copy(dest, dest + '.bak')
            print('  old file kept as', dest + '.bak')
        unknown = [k for k in d['text'] if k not in set(texts)]
        if unknown:
            print('  %d text(s) not in the mod (moved to [obsolete]):' % len(unknown))
            for k in unknown[:20]:
                print('    ' + q(k))
        write_lang(dest, ': ' + d['info'].get('name', code), d, texts, speech, modt)
        report(code + '.txt', d, texts, speech, modt)
        check_pieces(d, texts)
        return
    if cmd == 'update':
        empty = {k: {} for k in ('info', 'text', 'lines', 'mod', 'obsolete')}
        write_lang(os.path.join(LANG, '_template.txt'), ' template: copy it to <code>.txt and fill in the right side',
                   empty, texts, speech, modt)
    for f in sorted(os.listdir(LANG)):
        if not f.endswith('.txt') or f.startswith('_'):
            continue
        path = os.path.join(LANG, f)
        d = read_lang(path)
        only_mod = f == 'pt.txt'   # Portuguese is built in: only the game start texts
        report(f, d, texts, speech, modt)
        if cmd == 'update':
            write_lang(path, ': ' + d['info'].get('name', f[:-4]), d, texts, speech, modt, only_mod)


if __name__ == '__main__':
    main()
