"""Adds the slave caravan to mod/SlaverMod/SlaverMod.mod.

Run from anywhere:  python tools/build_caravan.py
Re-running replaces the records it made before, so edit the values below and run it again.

Records (string IDs N-SlaverMod.mod):
  5  squad template "SlaverMod caravan slaves": the shackled slaves the caravan sells
     (copy of vanilla "slaves 1-3"; the plugin also makes slaves when the squad comes without)
  6  squad template "SlaverMod slave caravan": Slave Traders, a caravan boss who talks like the
     slave platform sellers ("I'm looking to buy slaves" / "I want to sell some slaves", which
     open SlaverMod's trade window), a few guards and the slaves above
     (copy of vanilla "Slave platform sales" without its building)
  7  AI package "SlaverMod caravan waits": protect, heal, follow the leader, nothing else
  8-10 (the old campaign version) and the Slave Traders' campaign entry are removed: the
     SlaverMod plugin creates, keeps and removes the caravan itself (UpdateSlaveCaravan).
"""
import os, struct, sys, copy

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, 're'))
import kmod
from build_start import find, set_int, set_bool, set_string, set_list

REPO = os.path.dirname(HERE)
MOD = os.path.join(REPO, 'mod', 'SlaverMod', 'SlaverMod.mod')
KENSHI = os.environ.get('KENSHI_DIR', r'C:\Program Files (x86)\Steam\steamapps\common\Kenshi')
DATA = os.path.join(KENSHI, 'data')

SID_SLAVES, SID_CARAVAN, SID_CAMPAIGN, SID_ANNOUNCE, SID_LINE1, SID_LINE2 = (b'%d-SlaverMod.mod' % i for i in (5, 6, 7, 8, 9, 10))
SLAVE_TRADERS = b'1084-gamedata.base'
OURS = {SID_SLAVES, SID_CARAVAN, SID_CAMPAIGN, SID_ANNOUNCE, SID_LINE1, SID_LINE2}

CARAVAN_NAME = 'SlaverMod slave caravan'
DISPLAY_NAME = 'Slave Caravan'      # shown in the faction screen's events
SLAVES_MIN, SLAVES_MAX = 3, 6       # slaves for sale, leader of the slave squad included
BOSS = b'95766-rebirth.mod'         # Slaver Caravan Boss
HEAVY_GUARD = b'54544-rebirth.mod'  # Slaver Heavy Guard
GUARD = b'95765-rebirth.mod'        # Slaver Caravan Guard
GUARDS = (1, (3, 5, 0))             # heavy guards, (min, max) caravan guards
DIALOG_LEADER = b'10338-Dialogue (7).mod'   # Wandering Slaver Trader: buy / sell slaves
DIALOG_SQUAD = b'5244-gamedata.base'        # slaver guard MAIN
# The caravan's AI (7-SlaverMod.mod): no goal that walks anywhere, so it waits where SlaverMod
# puts it, beside the player's base; the plugin walks it away when its stay is over.
SID_WAIT_AI = b'7-SlaverMod.mod'
AI_PACKAGES = [(SID_WAIT_AI, (0, 0, 0))]
WAIT_LEADER = [(b'2825-gamedata.base', (0, 100, 0)),   # Protect squad
               (b'957-gamedata.base', (0, 100, 0)),    # Protect allies
               (b'62008-rebirth.mod', (0, 24, 0))]     # Medic non-urgent
WAIT_SQUAD = [(b'2825-gamedata.base', (0, 100, 0)),    # Protect squad
              (b'3391-gamedata.base', (0, 24, 0)),     # Put carried dude down when they can walk
              (b'2790-gamedata.base', (0, 24, 0)),     # Pickup downed leader
              (b'62008-rebirth.mod', (0, 24, 0)),      # Medic non-urgent
              (b'1166-gamedata.base', (0, 24, 0))]     # Follow leader
WAIT_SLAVES = [(b'3394-gamedata.base', (0, 24, 0))]    # Follow slavemaster
TRADE_VISIT_AI = b'62821-rebirth.mod'       # Base- trader visit AI
FORCES = 1                          # one caravan squad (4 made two caravans, two bosses)
# what the boss shouts when the caravan reaches the base (the game shows it like the Hive merchants')
ANNOUNCE_1 = ['Slave caravan! Fresh stock, chained and ready!', 'Slavers here! Come see the merchandise!',
              'Slaves for sale! Strong backs, cheap prices!']
ANNOUNCE_2 = ["Got slaves to sell? We're buying too.", 'Buying and selling, come talk to me.',
              "We pay good cats for good slaves."]
FORCE_SPEECH_TIMER = b'62749-Dialogue.mod'   # dialog action of the Hive merchants' announcement


def main():
    rebirth = kmod.read(os.path.join(DATA, 'rebirth.mod'))['records']
    dialogue = kmod.read(os.path.join(DATA, 'Dialogue.mod'))['records']

    m = kmod.read(MOD)
    m['records'] = [r for r in m['records'] if r['sid'] not in OURS and r['sid'] != SLAVE_TRADERS]
    inst = max([r['inst'] for r in m['records']] + [0]) + 1

    slaves = copy.deepcopy(find(rebirth, b'56180-rebirth.mod'))   # slaves 1-3
    slaves.update(sid=SID_SLAVES, inst=inst, id=0, name=b'SlaverMod caravan slaves', flags=0x10)
    set_int(slaves, 'num random chars', SLAVES_MIN - 1)
    set_int(slaves, 'num random chars max', SLAVES_MAX - 1)

    caravan = copy.deepcopy(find(rebirth, b'54571-rebirth.mod'))  # Slave platform sales
    caravan.update(sid=SID_CARAVAN, inst=inst + 1, id=0, name=CARAVAN_NAME.encode(), flags=0x10)
    caravan['extra'] = [e for e in caravan['extra'] if e[0] != b'building']
    set_string(caravan, 'building name', '')
    set_string(caravan, 'layout interior', '')
    set_string(caravan, 'layout exterior', '')
    set_list(caravan, 'AI packages', AI_PACKAGES)
    set_list(caravan, 'leader', [(BOSS, (1, 0, 0))])
    set_list(caravan, 'squad', [(HEAVY_GUARD, (GUARDS[0], 0, 0)), (GUARD, GUARDS[1])])
    set_list(caravan, 'slaves', [(SID_SLAVES, (0, 0, 0))])
    set_list(caravan, 'dialog leader', [(DIALOG_LEADER, (0, 0, 0))])
    set_list(caravan, 'dialog squad', [(DIALOG_SQUAD, (0, 0, 0))])

    wait = copy.deepcopy(find(rebirth, b'62933-rebirth.mod'))   # Casual Raid Retreat [passive], without its walking goals
    wait.update(sid=SID_WAIT_AI, inst=inst + 2, id=0, name=b'SlaverMod caravan waits', flags=0x10)
    wait['extra'] = []
    set_list(wait, 'Leader AI Goals', WAIT_LEADER)
    set_list(wait, 'Squad AI Goals', WAIT_SQUAD)
    set_list(wait, 'Slave AI Goals', WAIT_SLAVES)

    m['records'] += [slaves, caravan, wait]
    m['lastid'] = max(m['lastid'], 10)
    kmod.write(m, MOD)
    print('wrote %s: %d record(s), caravan with %d-%d slaves' % (MOD, len(m['records']), SLAVES_MIN, SLAVES_MAX))


if __name__ == '__main__':
    main()
