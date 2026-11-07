"""Makes the vanilla prisoner shackles light in mod/SlaverMod/SlaverMod.mod.

Run from anywhere:  python tools/build_shackles.py

The vanilla "Prisoner Shackles" (SHACKLES) and "Hive Prisoner Shackles" (SHACKLES-BODY) weigh
20 kg: carrying that trains strength fast (and running with it, athletics), so shackles were
a training tool. SlaverMod changes only their weight (changed records, every other field stays
the game's). The plugin's ShacklesNoRunning stops the running part.
"""
import os, struct, sys, copy

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, 're'))
import kmod

REPO = os.path.dirname(HERE)
MOD = os.path.join(REPO, 'mod', 'SlaverMod', 'SlaverMod.mod')

SHACKLES = [b'SHACKLES', b'SHACKLES-BODY']
NAMES = {b'SHACKLES': b'Prisoner Shackles', b'SHACKLES-BODY': b'Hive Prisoner Shackles'}   # a changed record carries the name too
WEIGHT_KG = 3.0
ARMOUR = 3   # item type of the shackles


def main():
    m = kmod.read(MOD)
    m['records'] = [r for r in m['records'] if r['sid'] not in SHACKLES]
    for sid in SHACKLES:
        r = {'inst': 0, 'rtype': ARMOUR, 'id': 0, 'name': NAMES[sid], 'sid': sid, 'flags': 0x11,
             'bools': [], 'floats': [(b'weight kg', struct.pack('<f', WEIGHT_KG))], 'ints': [],
             'vec3': [], 'vec4': [], 'strings': [], 'files': [], 'extra': [], 'instances': []}
        m['records'].append(r)
    kmod.write(m, MOD)
    print('wrote %s: shackles weigh %g kg' % (MOD, WEIGHT_KG))


if __name__ == '__main__':
    main()
