"""Adds the "Slave Hunters" game start to mod/SlaverMod/SlaverMod.mod.

Run from anywhere:  python tools/build_start.py
Re-running replaces the records it made before (string IDs N-SlaverMod.mod), so edit
the values below and run it again. The squad and characters start as copies of vanilla
templates from rebirth.mod (a player start squad, a player character and the Slave
Hunter NPC), then get the fields below.
"""
import os, struct, sys, copy

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, 're'))
import kmod

REPO = os.path.dirname(HERE)
MOD = os.path.join(REPO, 'mod', 'SlaverMod', 'SlaverMod.mod')
KENSHI = os.environ.get('KENSHI_DIR', r'C:\Program Files (x86)\Steam\steamapps\common\Kenshi')
REBIRTH = os.path.join(KENSHI, 'data', 'rebirth.mod')

SID_START, SID_SQUAD, SID_LEADER, SID_HUNTER = (b'%d-SlaverMod.mod' % i for i in (1, 2, 3, 4))
OURS = {SID_START, SID_SQUAD, SID_LEADER, SID_HUNTER}

START_NAME = 'Slave Hunters'
DESCRIPTION = (
    "You lead a small band of slave hunters working for the Slave Traders. Five hunters, "
    "a few cats and rusty clubs: knock people out, put them in shackles and make them work "
    "for you. Watch them, or they will run.\n\n"
    "Voce lidera um pequeno bando de cacadores de escravos a servico dos Traficantes de Escravos. "
    "Cinco cacadores, poucos cats e porretes enferrujados: derrube pessoas, coloque-as nos grilhoes "
    "e faca-as trabalhar para voce. Vigie-as, ou elas fogem.\n\n"
    "Starts at a random slaver camp. Needs the SlaverMod plugin (RE_Kenshi)."
)
MONEY = 500
TOWNS = [b'1073-gamedata.base',                     # Slave Farm
         b'1080-gamedata.base',                     # Slave Markets
         b'1785-gamedata.base',                     # Stone Camp
         b'1533492-__world reactions Slavers.mod',  # South Stone Camp
         b'1533495-__world reactions Slavers.mod']  # Slave Farm South
RELATIONS = [(b'1084-gamedata.base', 50),    # Slave Traders
             (b'55101-rebirth.mod', 50),     # Slave Hunters
             (b'58229-Dialogue.mod', -30)]   # Anti-Slavers
HUNTERS = 5

# The vanilla Slave Hunter outfit (Worn-out Shorts, Wooden Sandals, Armoured Face Plates) has
# no shirt; "shirt" adds one (hivers cannot wear shirts and just skip it).
LEADER = dict(name='Slaver Leader', combat=8, strength=10, unarmed=6, ranged=0, stealth=2, female=30,
              shirt=b'1169-gamedata.base')   # Leather Shirt
HUNTER = dict(name='Slave Hunter', combat=4, strength=6, unarmed=4, ranged=0, stealth=2, female=30,
              shirt=b'1168-gamedata.base')   # Cloth Shirt
IRON_CLUB = b'52295-rebirth.mod'
RUSTED = b'912-gamedata.base'   # weapon level: the worst manufacturer


def find(records, sid):
    for r in records:
        if r['sid'] == sid:
            return r
    raise KeyError(sid)


def set_int(r, key, value):
    key = key.encode()
    for i, (k, v) in enumerate(r['ints']):
        if k == key:
            r['ints'][i] = (k, struct.pack('<i', value))
            return
    r['ints'].append((key, struct.pack('<i', value)))


def set_bool(r, key, value):
    key = key.encode()
    for i, (k, v) in enumerate(r['bools']):
        if k == key:
            r['bools'][i] = (k, bytes([1 if value else 0]))
            return
    r['bools'].append((key, bytes([1 if value else 0])))


def set_string(r, key, value):
    key = key.encode()
    for e in r['strings']:
        if e[0] == key:
            e[1] = value.encode('utf-8')
            return
    r['strings'].append([key, value.encode('utf-8')])


def set_list(r, name, items):
    """items: [(sid bytes, (v0, v1, v2))]"""
    name = name.encode()
    packed = [(sid, struct.pack('<3i', *vals)) for sid, vals in items]
    for i, (c, _) in enumerate(r['extra']):
        if c == name:
            r['extra'][i] = (c, packed)
            return
    r['extra'].append((name, packed))


def character(base, gear_from, sid, inst, spec):
    c = copy.deepcopy(base)
    c['sid'] = sid
    c['inst'] = inst
    c['id'] = 0
    c['name'] = spec['name'].encode()
    for key, field in (('combat', 'combat stats'), ('strength', 'strength'), ('unarmed', 'unarmed stats'),
                       ('ranged', 'ranged stats'), ('stealth', 'stealth stats'), ('female', 'female chance')):
        set_int(c, field, spec[key])
    set_int(c, 'stats randomise', 2)
    set_int(c, 'money max', 0)
    set_int(c, 'money min', 0)
    set_int(c, 'slave', 0)
    set_bool(c, 'named', True)
    set_bool(c, 'unique', False)
    # clothes and supplies of the vanilla Slave Hunter, a rusty club
    for name in ('clothing', 'inventory'):
        src = [e for e in gear_from['extra'] if e[0] == name.encode()]
        if src:
            items = [(sid_, struct.unpack('<3i', v)) for sid_, v in src[0][1]]
            if name == 'clothing' and spec.get('shirt'):
                items.append((spec['shirt'], (1, 100, 0)))
            set_list(c, name, items)
    set_list(c, 'weapons', [(IRON_CLUB, (1, 0, 100))])
    set_list(c, 'weapon level', [(RUSTED, (0, 0, 0))])
    return c


def main():
    rebirth = kmod.read(REBIRTH)['records']
    start_base = find(rebirth, b'1533582-rebirth.mod')   # Holy Nation Citizen (start)
    squad_base = find(rebirth, b'1534417-rebirth.mod')   # its squad template
    player_base = find(rebirth, b'1534418-rebirth.mod')  # its player character
    hunter_gear = find(rebirth, b'55103-rebirth.mod')    # Slave Hunter NPC

    m = kmod.read(MOD)
    m['records'] = [r for r in m['records'] if r['sid'] not in OURS]
    inst = max([r['inst'] for r in m['records']] + [0]) + 1

    start = copy.deepcopy(start_base)
    start.update(sid=SID_START, inst=inst, id=0, name=START_NAME.encode())
    set_int(start, 'money', MONEY)
    set_bool(start, 'force start pos', False)
    set_string(start, 'description', DESCRIPTION)
    set_string(start, 'difficulty', 'Hard')
    set_string(start, 'style', 'RPG / Slaver')
    start['extra'] = []
    set_list(start, 'faction relations', [(sid, (v, 0, 0)) for sid, v in RELATIONS])
    set_list(start, 'squad', [(SID_SQUAD, (0, 0, 0))])
    set_list(start, 'town', [(t, (0, 0, 0)) for t in TOWNS])

    squad = copy.deepcopy(squad_base)
    squad.update(sid=SID_SQUAD, inst=inst + 1, id=0, name=b'startoff- Slave Hunters')
    squad['extra'] = [e for e in squad['extra'] if e[0] not in (b'leader', b'squad', b'squad2')]
    set_list(squad, 'leader', [(SID_LEADER, (1, 0, 0))])
    set_list(squad, 'squad', [(SID_HUNTER, (HUNTERS, 0, 0))])

    leader = character(player_base, hunter_gear, SID_LEADER, inst + 2, LEADER)
    hunter = character(player_base, hunter_gear, SID_HUNTER, inst + 3, HUNTER)

    m['records'] += [start, squad, leader, hunter]
    m['lastid'] = max(m['lastid'], 4)
    kmod.write(m, MOD)
    print('wrote %s: %d record(s), start "%s" with %d towns' % (MOD, len(m['records']), START_NAME, len(TOWNS)))


if __name__ == '__main__':
    main()
