"""SlaverMod's own items in mod/SlaverMod/SlaverMod.mod.

Run from anywhere:  python tools/build_items.py

- 11-SlaverMod.mod  material of the slave collar
- 12-SlaverMod.mod  "Slave collar": locked neck collar (MCM > Restraint). Mesh, textures and icon are
                    Macropod's (his "Slave dress" mod), released by him for free use; they live in
                    mod/SlaverMod/items/collar/.
- 13-SlaverMod.mod  "Shackle kit": what RequireShackles=1 asks for to enslave someone. Sold in the
                    slave trade window (BUY SHACKLE KIT); freeing a slave gives one back.
Each run replaces these records only.
"""
import os, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, 're'))
import kmod

REPO = os.path.dirname(HERE)
MOD = os.path.join(REPO, 'mod', 'SlaverMod', 'SlaverMod.mod')
SID_MAT, SID_COLLAR, SID_KIT = b'11-SlaverMod.mod', b'12-SlaverMod.mod', b'13-SlaverMod.mod'
MATERIAL_SPEC, ARMOUR, ITEM = 47, 3, 4
ASSETS = b'.\\mods\\SlaverMod\\items\\collar\\'


def f(v): return struct.pack('<f', v)
def i(v): return struct.pack('<i', v)
def b(v): return b'\x01' if v else b'\x00'
REF0 = b'\x00' * 12


def record(sid, rtype, name, inst):
    return {'inst': inst, 'rtype': rtype, 'id': 0, 'name': name, 'sid': sid, 'flags': 0x10,
            'bools': [], 'floats': [], 'ints': [], 'vec3': [], 'vec4': [], 'strings': [], 'files': [],
            'extra': [], 'instances': []}


def main():
    m = kmod.read(MOD)
    mine = (SID_MAT, SID_COLLAR, SID_KIT)
    m['records'] = [r for r in m['records'] if r['sid'] not in mine]
    inst = max([r['inst'] for r in m['records']] + [0]) + 1

    mat = record(SID_MAT, MATERIAL_SPEC, b'SlaverMod collar material', inst)
    mat['floats'] = [(b'paint factor 1', f(0)), (b'paint factor 2', f(0)), (b'specular mult', f(1))]
    mat['ints'] = [(b'material type', i(0))]
    mat['files'] = [[b'color map', b''], [b'normal map', ASSETS + b'Collar_01_N.dds'], [b'texture map', ASSETS + b'Collar_01_D.dds']]

    collar = record(SID_COLLAR, ARMOUR, b'Slave collar', inst + 1)
    collar['bools'] = [(b'auto icon image', b(0)), (b'dont colorise', b(1)), (b'has collision', b(1)), (b'hide beard', b(0)),
                       (b'hide hair', b(0)), (b'hide part b', b(0)), (b'hide part g', b(0)), (b'hide part r', b(0)), (b'is locked', b(1))]
    collar['floats'] = [(k, f(v)) for k, v in [
        ('asassination mult', 1), ('athletics mult', 1), ('blunt def bonus', 0), ('boot height', 0), ('combat speed mult', 1),
        ('cut def bonus', 0), ('cut into stun', 0), ('damage output mult', 1), ('dexterity mult', 1), ('dodge mult', 1),
        ('fabrics amount', 0), ('fist injury mult', 1), ('hardness', 20), ('icon offset H', 0.1), ('icon offset pitch', 0),
        ('icon offset roll', 0), ('icon offset V', -17.8), ('icon offset yaw', 90), ('icon zoom', 1.05), ('pierce def mult', 1),
        ('ranged skill mult', 1), ('relative price mult', 1), ('stealth mult', 1), ('weather protection amount', 0), ('weight kg', 2)]]
    collar['floats'] = [(k.encode(), v) for k, v in collar['floats']]
    collar['ints'] = [(k.encode(), i(v)) for k, v in [
        ('class', 2), ('combat attk bonus', 0), ('combat def bonus', 0), ('combat skill bonus', 0), ('hide parts', 0), ('hide stump', 0),
        ('inventory footprint height', 2), ('inventory footprint width', 2), ('level bonus', 0), ('lock level', 20), ('material type', 3),
        ('perception bonus', 0), ('ranged bonus', 0), ('slot', 11), ('stigma', 6), ('unarmed bonus', 0), ('value', 0), ('weather protection0', 0)]]
    collar['strings'] = [[b'class name', b'SlaverMod'], [b'description', b'A locked iron collar. Made by Macropod.']]
    collar['files'] = [[b'ground mesh', b''], [b'icon', ASSETS + b'Collar_01_icon.png'], [b'mesh', ASSETS + b'Collar_01.mesh'],
                       [b'mesh female', ASSETS + b'collar_01_F.mesh'], [b'overlap mesh', b''], [b'overlap mesh female', b''],
                       [b'physics file', b''], [b'vest colormap', b''], [b'vest colormap female', b''], [b'vest normalmap', b''],
                       [b'vest normalmap female', b''], [b'vest texture', b''], [b'vest texture female', b'']]
    collar['extra'] = [(b'material', [(SID_MAT, REF0)])]

    kit = record(SID_KIT, ITEM, b'Shackle kit', inst + 2)
    kit['bools'] = [(b'artifact', b(0)), (b'auto icon image', b(0)), (b'food crop', b(0)), (b'has collision', b(1)), (b'trade item', b(1))]
    kit['floats'] = [(k.encode(), f(v)) for k, v in [
        ('charges', 1), ('icon offset H', 0), ('icon offset pitch', 0), ('icon offset roll', 0), ('icon offset V', 0), ('icon offset yaw', 0),
        ('icon zoom', 1), ('profitability', 0.2), ('quality', 1), ('weight kg', 1.5)]]
    kit['ints'] = [(k.encode(), i(v)) for k, v in [
        ('inventory footprint height', 2), ('inventory footprint width', 2), ('inventory sound', 19), ('item function', 0),
        ('persistent', 0), ('production time', 4), ('slot', 7), ('stackable', 10), ('value', 150)]]
    kit['strings'] = [[b'description', b'Chains, cuffs and a lock: everything needed to put one person in shackles.']]
    kit['files'] = [[b'ground mesh', b''], [b'icon', b'.\\data\\items\\icons\\shackles_icon.png'],
                    [b'mesh', b'.\\data\\items\\misc\\travelbarrel.mesh'], [b'physics file', b'.\\data\\items\\misc\\phys\\travelbarrel.xml']]
    kit['extra'] = [(b'material', [(b'1014-gamedata.base', REF0)])]

    m['records'] += [mat, collar, kit]
    m['lastid'] = max(m['lastid'], 13)
    kmod.write(m, MOD)
    print('wrote %s: collar %s, shackle kit %s' % (MOD, SID_COLLAR.decode(), SID_KIT.decode()))


if __name__ == '__main__':
    main()
