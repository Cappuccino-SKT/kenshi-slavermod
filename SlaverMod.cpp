// SlaverMod - lets the player's faction own slaves using Kenshi's native slave system.
// Build: tools\build_plugin.bat . SlaverMod  (VC++ 2010 x64, KenshiLib; see README)

#include <Debug.h>

#include <kenshi/gui/ForgottenGUI.h>
#include <kenshi/gui/InventoryGUI.h>
#include <kenshi/Globals.h>
#include <kenshi/GameWorld.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/Character.h>
#include <kenshi/MedicalSystem.h>
#include <kenshi/Platoon.h>
#include <kenshi/Faction.h>
#include <kenshi/Inventory.h>
#include <kenshi/Item.h>
#include <kenshi/StateBroadcastData.h>
#include <kenshi/RootObjectFactory.h>
#include <kenshi/GameData.h>
#include <kenshi/FactionRelations.h>
#include <kenshi/CharStats.h>

#include <kenshi/Animation/AnimationClass.h>
#include <kenshi/util/hand.h>
#include <kenshi/util/lektor.h>

#include <mygui/common/baselayout/BaseLayout.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_Delegate.h>
#include <mygui/MyGUI_RenderManager.h>

#include <core/Functions.h>

#include "ModConfigMenu.h"

#include <map>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cmath>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// Copied from kenshi/gui/ContextMenu.h: that header also redefines class ContextMenu,
// which PlayerInterface.h already defines. Declarations must match it exactly.
class ContextMenuGUI : public wraps::BaseLayout, public Ogre::GeneralAllocatedObject
{
public:
    ContextMenuGUI();// public RVA = 0x796540
    virtual ~ContextMenuGUI();// public RVA = 0x793D70 vtable offset = 0x0
    MyGUI::Widget* getMainWidget() const;// public RVA = 0x7BF260
    bool getVisible() const;// public RVA = 0x7BF270
    void setVisible(bool visible);// public RVA = 0x7938B0
    void show(const lektor<int>& ordersList, const std::string& _name, bool offset);// public RVA = 0x7A6D80
    void optionSelected(MyGUI::Widget* _sender, int _left, int _top, MyGUI::MouseButton _id);// public RVA = 0x7A6CB0
    hand contextMenuTarget; // 0xA8 Member
    std::string name; // 0xC8 Member
    MyGUI::TextBox* nameText; // 0xF0 Member
    MyGUI::Widget* optionsList; // 0xF8 Member
    MyGUI::types::TCoord<int> optionCoords; // 0x100 Member
    MyGUI::types::TCoord<int> buttonCoords; // 0x110 Member
    MyGUI::types::TCoord<int> valueCoords; // 0x120 Member
};

// ---------------------------------------------------------------------------
// Logging: RE_Kenshi log + own trace file next to the DLL (flushed per line)
// ---------------------------------------------------------------------------

static std::string g_modDir;

static void InitModDir()
{
    char path[MAX_PATH] = {0};
    HMODULE mod = 0;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&InitModDir, &mod) && mod != 0)
    {
        GetModuleFileNameA(mod, path, MAX_PATH);
        std::string dir(path);
        size_t slash = dir.find_last_of("\\/");
        g_modDir = (slash != std::string::npos) ? dir.substr(0, slash + 1) : "";
    }
}

static bool g_debug = true;

static void Trace(const std::string& msg)
{
    DebugLog("SlaverMod: " + msg);
    if (!g_debug)
        return;
    std::ofstream file((g_modDir + "SlaverMod_trace.log").c_str(), std::ios::app);
    if (file)
        file << msg << "\n";
}

static std::string Num(float v)
{
    std::ostringstream s;
    s << v;
    return s.str();
}

// ---------------------------------------------------------------------------
// Language: English by default, Brazilian Portuguese with Language=pt
// ---------------------------------------------------------------------------

static bool g_pt = false;

static std::string T(const char* en, const char* pt)
{
    return g_pt ? pt : en;
}

// ---------------------------------------------------------------------------
// Settings (SlaverMod.ini next to the DLL)
// ---------------------------------------------------------------------------

struct Settings
{
    std::string language;
    std::string squadName;
    std::string hotkey;
    bool showButton;
    bool contextMenu;
    bool unchainOnEnslave;
    bool slaveAIJob;
    bool banWeapons;
    bool rebellionEnabled;
    float rebellionDays;
    float rebellionChanceMult;
    float rebellionMaxChance;
    float rebelMinStrength;
    std::string rebelFaction;
    std::string rebelWeapon;
    std::string rebelWeaponMaker;
    std::string rebelWeaponModel;
    bool rebellionHostile;
    float rebelChaseRange;
    std::string rebellionTestKey;
    bool rebellionTestForce;
    bool debugMode;   // test hotkeys (instant enslave/free, forced rebellion check)
    bool instantAction;
    float actionSeconds;
    float approachTimeout;
    float reachFactor;
    bool giveShackles;
    std::string shackleItem;
    std::string shackleItemAlt;
    std::string actionAnimation;
    bool progressBar;
    bool showNotices;   // 0 = only the rebellion alert is shown
    float relationLoss;
    float relationGain;
    bool slaveEscort;
    std::string escortMode;
};

static Settings g_cfg;

static std::string IniString(const char* key, const char* def)
{
    char buf[512] = {0};
    GetPrivateProfileStringA("SlaverMod", key, def, buf, sizeof(buf), (g_modDir + "SlaverMod.ini").c_str());
    return buf;
}

static float IniFloat(const char* key, float def)
{
    std::string s = IniString(key, "");
    if (s.empty())
        return def;
    return (float)atof(s.c_str());
}

static bool IniBool(const char* key, bool def)
{
    std::string s = IniString(key, def ? "1" : "0");
    return s == "1" || s == "true" || s == "TRUE";
}

static void LoadSettings()
{
    g_debug = IniBool("Debug", true);
    g_cfg.language = IniString("Language", "en");
    g_pt = g_cfg.language.compare(0, 2, "pt") == 0 || g_cfg.language.compare(0, 2, "PT") == 0;
    g_cfg.squadName = IniString("SquadName", g_pt ? "Escravos" : "Slaves");
    g_cfg.hotkey = IniString("EnslaveKey", "CTRL+K");
    g_cfg.showButton = IniBool("ShowButton", false);
    g_cfg.contextMenu = IniBool("ContextMenu", true);
    g_cfg.unchainOnEnslave = IniBool("UnchainOnEnslave", true);
    g_cfg.slaveAIJob = IniBool("SlaveAIJob", false);
    g_cfg.banWeapons = IniBool("BanWeapons", true);
    g_cfg.rebellionEnabled = IniBool("RebellionEnabled", true);
    g_cfg.rebellionDays = IniFloat("RebellionDays", 7.0f);
    g_cfg.rebellionChanceMult = IniFloat("RebellionChanceMult", 1.0f);
    g_cfg.rebellionMaxChance = IniFloat("RebellionMaxChance", 0.9f);
    g_cfg.rebelMinStrength = IniFloat("RebelMinStrength", 20.0f);
    g_cfg.rebelFaction = IniString("RebelFaction", "58229-Dialogue.mod");
    g_cfg.rebelWeapon = IniString("RebelWeapon", "52295-rebirth.mod");
    g_cfg.rebelWeaponMaker = IniString("RebelWeaponMaker", "912-gamedata.base");
    g_cfg.rebelWeaponModel = IniString("RebelWeaponModel", "913-gamedata.base");
    g_cfg.rebellionHostile = IniBool("RebellionHostile", true);
    g_cfg.rebelChaseRange = IniFloat("RebelChaseRange", 1000.0f);
    g_cfg.rebellionTestKey = IniString("RebellionTestKey", "CTRL+SHIFT+R");
    g_cfg.rebellionTestForce = IniBool("RebellionTestForce", false);
    g_cfg.debugMode = IniBool("DebugMode", false);
    g_cfg.instantAction = IniBool("InstantAction", false);
    g_cfg.actionSeconds = IniFloat("ActionSeconds", 4.0f);
    g_cfg.approachTimeout = IniFloat("ApproachTimeout", 120.0f);
    g_cfg.reachFactor = IniFloat("ReachFactor", 2.5f);
    g_cfg.giveShackles = IniBool("GiveShackles", true);
    g_cfg.shackleItem = IniString("ShackleItem", "SHACKLES");
    g_cfg.shackleItemAlt = IniString("ShackleItemAlt", "SHACKLES-BODY");
    g_cfg.actionAnimation = IniString("ActionAnimation", "medic");
    g_cfg.progressBar = IniBool("ProgressBar", true);
    g_cfg.showNotices = IniBool("ShowNotices", true);
    g_cfg.relationLoss = IniFloat("RelationLoss", 5.0f);
    g_cfg.relationGain = IniFloat("RelationGain", 2.0f);
    if (g_cfg.relationGain >= g_cfg.relationLoss && g_cfg.relationLoss > 0.0f)
        g_cfg.relationGain = g_cfg.relationLoss * 0.5f;   // freeing must be worth less than enslaving costs
    g_cfg.slaveEscort = IniBool("SlaveEscort", true);
    g_cfg.escortMode = IniString("EscortMode", "bodyguard");
    Trace("settings: language=" + std::string(g_pt ? "pt" : "en") + " squad='" + g_cfg.squadName + "' key=" + g_cfg.hotkey +
          " button=" + Num(g_cfg.showButton) + " menu=" + Num(g_cfg.contextMenu) + " unchain=" + Num(g_cfg.unchainOnEnslave) +
          " slaveAIJob=" + Num(g_cfg.slaveAIJob) + " banWeapons=" + Num(g_cfg.banWeapons) +
          " rebellion=" + Num(g_cfg.rebellionEnabled) + " every=" + Num(g_cfg.rebellionDays) + "d mult=" + Num(g_cfg.rebellionChanceMult) +
          " max=" + Num(g_cfg.rebellionMaxChance) + " minStr=" + Num(g_cfg.rebelMinStrength) + " faction=" + g_cfg.rebelFaction +
          " weapon=" + g_cfg.rebelWeapon + " by " + g_cfg.rebelWeaponMaker + "/" + g_cfg.rebelWeaponModel + " hostile=" + Num(g_cfg.rebellionHostile) +
          " testKey=" + g_cfg.rebellionTestKey + " testForce=" + Num(g_cfg.rebellionTestForce) +
          " debugMode=" + Num(g_cfg.debugMode) + " instant=" + Num(g_cfg.instantAction) +
          " actionSeconds=" + Num(g_cfg.actionSeconds) + " timeout=" + Num(g_cfg.approachTimeout) +
          " reach=" + Num(g_cfg.reachFactor) + " shackles=" + Num(g_cfg.giveShackles) +
          " item=" + g_cfg.shackleItem + "/" + g_cfg.shackleItemAlt + " anim=" + g_cfg.actionAnimation +
          " bar=" + Num(g_cfg.progressBar) + " notices=" + Num(g_cfg.showNotices) + " relationLoss=" + Num(g_cfg.relationLoss) + " relationGain=" + Num(g_cfg.relationGain) +
          " escort=" + Num(g_cfg.slaveEscort) + "/" + g_cfg.escortMode);
}

// ---------------------------------------------------------------------------
// On-screen notice (top centre, fades after a few seconds)
// ---------------------------------------------------------------------------

static MyGUI::Widget* g_noticePanel = NULL;
static MyGUI::TextBox* g_noticeText = NULL;
static DWORD g_noticeUntil = 0;

static void ShowNotice(const std::string& text, bool always = false)
{
    Trace("notice: " + text);
    if (!g_cfg.showNotices && !always)
        return;
    MyGUI::Gui* mg = MyGUI::Gui::getInstancePtr();
    if (mg == NULL)
        return;

    const int w = 520, h = 40;
    if (g_noticePanel == NULL)
    {
        g_noticePanel = mg->createWidget<MyGUI::Widget>("Kenshi_FloatingPanelSkin", 0, 0, w, h, MyGUI::Align::Default, "Popup", "SlaverModNotice");
        if (g_noticePanel == NULL)
            return;
        g_noticeText = g_noticePanel->createWidget<MyGUI::TextBox>("Kenshi_GenericTextBoxFlat", 8, 0, w - 16, h, MyGUI::Align::Default, "SlaverModNoticeText");
        g_noticeText->setTextAlign(MyGUI::Align::Center);
        g_noticePanel->setNeedMouseFocus(false);
        g_noticeText->setNeedMouseFocus(false);
    }

    const MyGUI::IntSize& view = MyGUI::RenderManager::getInstance().getViewSize();
    g_noticePanel->setPosition((view.width - w) / 2, 90);
    g_noticeText->setCaption(text);
    g_noticePanel->setVisible(true);
    g_noticeUntil = GetTickCount() + 4500;
}

static void UpdateNotice()
{
    if (g_noticePanel != NULL && g_noticePanel->getVisible() && GetTickCount() > g_noticeUntil)
        g_noticePanel->setVisible(false);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static Faction* PlayerFaction()
{
    if (ou == 0 || ou->player == 0)
        return 0;
    return ou->player->getFaction();
}

static bool IsPlayerFaction(Character* c)
{
    Faction* pf = PlayerFaction();
    return c != 0 && pf != 0 && c->getFaction() == pf;
}

static bool IsOurSlave(Character* c)
{
    if (c == 0 || !IsPlayerFaction(c))
        return false;
    return c->isSlave() == IS_SLAVE;
}

// A living, non-slave member. Being knocked out does not stop someone from owning slaves.
static bool IsFreePlayerMember(Character* c)
{
    return c != 0 && !c->isDead() && IsPlayerFaction(c) && c->isSlave() != IS_SLAVE;
}

// A free member that is awake and can actually watch the slaves.
static bool IsActiveGuard(Character* c)
{
    return IsFreePlayerMember(c) && !c->medical.isUnconcious();
}

static void DumpMembers(const std::string& reason)
{
    Trace("members (" + reason + "):");
    const lektor<Character*>& all = ou->player->getAllPlayerCharacters();
    for (uint32_t i = 0; i < all.size(); i++)
    {
        Character* c = all.stuff[i];
        if (c == 0)
            continue;
        Trace("  '" + c->getName() + "' faction=" + Num(IsPlayerFaction(c)) + " slaveState=" + Num((float)c->isSlave()) +
              " dead=" + Num(c->isDead()) + " unconscious=" + Num(c->medical.isUnconcious()));
    }
}

static int CountFreeMembers()
{
    int n = 0;
    const lektor<Character*>& all = ou->player->getAllPlayerCharacters();
    for (uint32_t i = 0; i < all.size(); i++)
        if (IsFreePlayerMember(all.stuff[i]))
            n++;
    return n;
}

// The native flag the game itself uses for slaves; changeSlaveOwner() only records the owner.
static void SetSlaveState(Character* c, SlaveStateEnum s)
{
    StateBroadcastData* sb = c->getStateBroadcast();
    if (sb != 0)
        sb->setSlaveState(s);
}

// A non-player character that is currently a prisoner (caged or chained) or knocked out.
static bool IsEnslaveCandidate(Character* c, std::string* why)
{
    if (c == 0) { if (why) *why = T("no character", "nenhum personagem"); return false; }
    if (c->isDead()) { if (why) *why = T("the character is dead", "personagem morto"); return false; }
    if (IsPlayerFaction(c)) { if (why) *why = T("already in your faction", "ja e da sua faccao"); return false; }

    bool prisoner = c->inSomething == IN_PRISON;
    bool chained = c->isChainedMode();
    bool down = c->medical.isUnconcious();
    if (!prisoner && !chained && !down)
    {
        if (why) *why = T("must be unconscious, chained or caged", "precisa estar inconsciente, acorrentado ou preso");
        return false;
    }
    return true;
}

// Nearest free member other than `exclude`; with awakeOnly, only members that can guard.
static Character* NearestFreeMember(const Ogre::Vector3& pos, float* distOut, Character* exclude, bool awakeOnly)
{
    Character* best = 0;
    float bestD = 1e30f;
    const lektor<Character*>& all = ou->player->getAllPlayerCharacters();
    for (uint32_t i = 0; i < all.size(); i++)
    {
        Character* c = all.stuff[i];
        if (c == exclude || !(awakeOnly ? IsActiveGuard(c) : IsFreePlayerMember(c)))
            continue;
        float d = (c->getPosition() - pos).length();
        if (d < bestD)
        {
            bestD = d;
            best = c;
        }
    }
    if (distOut)
        *distOut = bestD;
    return best;
}

// Owner for a new slave: `preferred` (the selected character) when it can own slaves,
// else the nearest awake free member, or any free member if all are knocked out.
static Character* PickOwner(Character* target, Character* preferred, float* distOut)
{
    if (preferred != 0 && preferred != target && IsFreePlayerMember(preferred))
    {
        if (distOut)
            *distOut = (preferred->getPosition() - target->getPosition()).length();
        return preferred;
    }
    Character* owner = NearestFreeMember(target->getPosition(), distOut, target, true);
    if (owner == 0)
        owner = NearestFreeMember(target->getPosition(), distOut, target, false);
    return owner;
}

// Find the player's existing slave squad by name, or create it.
static ActivePlatoon* GetSlaveSquad()
{
    const lektor<Character*>& all = ou->player->getAllPlayerCharacters();
    for (uint32_t i = 0; i < all.size(); i++)
    {
        Character* c = all.stuff[i];
        if (c == 0)
            continue;
        ActivePlatoon* p = c->getPlatoon();
        if (p != 0 && p->getName() == g_cfg.squadName)
            return p;
    }
    ActivePlatoon* created = ou->player->createSquad();
    if (created != 0)
    {
        created->setName(g_cfg.squadName);
        Trace("created squad '" + g_cfg.squadName + "'");
    }
    return created;
}

// ---------------------------------------------------------------------------
// Enslave / free
// ---------------------------------------------------------------------------

// Marks an already-recruited member of the player's faction as a slave.
static void MakeSlave(Character* target, Character* owner)
{
    hand ownerHand(owner);
    target->changeSlaveOwner(ownerHand);
    SetSlaveState(target, IS_SLAVE);
    if (g_cfg.unchainOnEnslave && target->isChainedMode())
        target->setChainedMode(false, ownerHand);
    target->setSlaveAIJob(g_cfg.slaveAIJob);
    Trace("after enslave: slaveState=" + Num((float)target->isSlave()) + " chained=" + Num(target->isChainedMode()));
}

static void EnslaveOwnMember(Character* target, Character* preferredOwner)
{
    std::string name = target->getName();
    if (target->isDead())
        return;
    float dist = 0.0f;
    Character* owner = CountFreeMembers() > 1 ? PickOwner(target, preferredOwner, &dist) : 0;
    if (owner == 0)
    {
        DumpMembers("last free member");
        ShowNotice(T("You cannot enslave the last free member of your faction.", "Nao da para escravizar o ultimo membro livre da faccao."));
        return;
    }
    Trace("enslave own member '" + name + "' owner='" + owner->getName() + "' dist=" + Num(dist));
    MakeSlave(target, owner);
    if (target->isSlave() == IS_SLAVE)
        ShowNotice(name + T(" is now a slave of your faction.", " agora e escravo(a) da sua faccao."));
    else
        ShowNotice(T("The game did not accept ", "O jogo nao aceitou marcar ") + name + T(" as a slave.", " como escravo(a)."));
}

static void Enslave(Character* target, Character* preferredOwner)
{
    std::string why;
    if (!IsEnslaveCandidate(target, &why))
    {
        ShowNotice(T("Cannot enslave: ", "Nao da para escravizar: ") + why + ".");
        return;
    }

    std::string name = target->getName();
    float dist = 0.0f;
    Character* owner = PickOwner(target, preferredOwner, &dist);
    if (owner == 0)
    {
        DumpMembers("no owner");
        ShowNotice(T("No free member of your faction to be the owner.", "Nenhum membro livre da sua faccao para ser o dono."));
        return;
    }
    Trace("enslave '" + name + "' owner='" + owner->getName() + "' dist=" + Num(dist) +
          " prisoner=" + Num(target->inSomething == IN_PRISON) + " chained=" + Num(target->isChainedMode()) +
          " unconscious=" + Num(target->medical.isUnconcious()) + " slaveState(before)=" + Num((float)target->isSlave()));

    // 1) bring into the player's faction, inside the slave squad
    ActivePlatoon* squad = GetSlaveSquad();
    Platoon* previous = ou->player->getCurrentPlatoon();
    if (squad != 0 && squad->me != 0)
        ou->player->setCurrentPlatoon(squad->me);

    Character* arr[1] = { target };
    lektor<Character*> list;
    list.stuff = arr;
    list.count = 1;
    list.maxSize = 1;
    bool recruited = ou->player->recruit(list, false);
    list.stuff = nullptr;
    list.count = 0;
    list.maxSize = 0;

    if (previous != 0)
        ou->player->setCurrentPlatoon(previous);
    Trace("recruit returned " + Num(recruited) + ", faction now player=" + Num(IsPlayerFaction(target)));

    if (!IsPlayerFaction(target))
    {
        ShowNotice(T("The game refused to recruit ", "O jogo recusou o recrutamento de ") + name + ".");
        return;
    }

    // 2) native slave state, owned by the nearest free member
    MakeSlave(target, owner);
    if (target->isSlave() == IS_SLAVE)
        ShowNotice(name + T(" is now a slave of your faction.", " agora e escravo(a) da sua faccao."));
    else
        ShowNotice(name + T(" joined your faction, but the game did not accept them as a slave.", " entrou na faccao, mas o jogo nao aceitou marca-lo(a) como escravo(a)."));
}

static void FreeSlave(Character* target)
{
    if (!IsOurSlave(target))
    {
        ShowNotice(T("This character is not your slave.", "Esse personagem nao e seu escravo."));
        return;
    }
    std::string name = target->getName();
    target->setSlaveAIJob(false);
    SetSlaveState(target, NOT_SLAVE);
    target->changeSlaveOwner(hand());
    Trace("freed '" + name + "' slaveState=" + Num((float)target->isSlave()));
    ShowNotice(name + T(" was freed and stays in your faction.", " foi libertado(a) e continua na sua faccao."));
}

// ---------------------------------------------------------------------------
// Shackles: locked "Prisoner Shackles" armour put on new slaves, removed on release
// ---------------------------------------------------------------------------

static bool IsShackleItem(Item* it)
{
    if (it == 0)
        return false;
    if (it->isLockedArmour() != 0)
        return true;
    if (it->data == 0)
        return false;
    const std::string& sid = it->data->stringID;
    return sid == g_cfg.shackleItem || sid == g_cfg.shackleItemAlt;
}

static bool WearsShackles(Character* c)
{
    if (c == 0 || c->inventory == 0)
        return false;
    lektor<Item*> armour;
    c->inventory->getEquippedArmour(armour);
    for (uint32_t i = 0; i < armour.size(); i++)
        if (IsShackleItem(armour.stuff[i]))
            return true;
    return false;
}

static bool TryEquipShackles(Character* c, const std::string& sid)
{
    if (sid.empty())
        return false;
    GameData* gd = ou->gamedata.getData(sid, ARMOUR);
    if (gd == 0)
    {
        Trace("shackles: item '" + sid + "' not found in game data");
        return false;
    }
    Item* item = ou->theFactory->createItem(gd, hand(), 0, 0, 0, 0);
    if (item == 0)
    {
        Trace("shackles: could not create '" + sid + "'");
        return false;
    }
    Inventory* inv = c->inventory;

    // take off whatever occupies the shackles' slot (clothes, boots...)
    std::vector<Item*> displaced;
    lektor<InventorySection*> sections;
    inv->getAllSectionsOfType(sections, item->slotType);
    for (uint32_t s = 0; s < sections.size(); s++)
    {
        const Ogre::vector<InventorySection::SectionItem>::type& items = sections.stuff[s]->getItems();
        std::vector<Item*> inSlot;
        for (size_t k = 0; k < items.size(); k++)
            inSlot.push_back(items[k].item);
        for (size_t k = 0; k < inSlot.size(); k++)
        {
            Item* worn = inv->removeItemDontDestroy_returnsItem(inSlot[k], inSlot[k]->quantity, false);
            if (worn != 0)
                displaced.push_back(worn);
        }
    }

    bool ok = inv->addItem(item, 1, false, true) && inv->equipItem(item) && WearsShackles(c);
    if (ok)
        Trace("shackles: '" + c->getName() + "' now wears '" + sid + "' (slot " + Num((float)item->slotType) + ")");
    else
    {
        Trace("shackles: '" + c->getName() + "' cannot wear '" + sid + "' (slot " + Num((float)item->slotType) + ")");
        if (inv->hasItem(item))
            inv->removeItemAutoDestroy(item, 1);
    }

    // what was taken off goes to the inventory, or to the ground if it does not fit
    for (size_t k = 0; k < displaced.size(); k++)
    {
        Item* worn = displaced[k];
        std::string name = worn->getName();
        inv->addItem(worn, worn->quantity, true, false);
        if (!ok)
            inv->equipItem(worn);   // put it back on
        else
            Trace("shackles: took off '" + name + "' from '" + c->getName() + "'");
    }
    if (ok && !displaced.empty())
        ShowNotice(c->getName() + T(" had ", " teve ") + Num((float)displaced.size()) + T(" piece(s) of clothing removed for the shackles.", " peca(s) de roupa tirada(s) para os grilhoes."));
    return ok;
}

static void GiveShackles(Character* c)
{
    if (!g_cfg.giveShackles || c == 0 || c->inventory == 0 || WearsShackles(c))
        return;
    if (!TryEquipShackles(c, g_cfg.shackleItem) && !TryEquipShackles(c, g_cfg.shackleItemAlt))
        ShowNotice(T("Could not put shackles on ", "Nao deu para colocar grilhoes em ") + c->getName() + ".");
}

static void RemoveShackles(Character* c)
{
    if (c == 0 || c->inventory == 0)
        return;
    lektor<Item*> armour;
    c->inventory->getEquippedArmour(armour);
    for (uint32_t i = 0; i < armour.size(); i++)
    {
        Item* it = armour.stuff[i];
        if (!IsShackleItem(it))
            continue;
        Trace("shackles: removed '" + it->getName() + "' from '" + c->getName() + "'");
        c->inventory->removeItemAutoDestroy(it, 1);
    }
}

// ---------------------------------------------------------------------------
// Slave rules, checked every couple of game seconds
// ---------------------------------------------------------------------------

static float g_ruleTimer = 0.0f;

static void ApplySlaveRules(float dt)
{
    g_ruleTimer += dt;
    if (g_ruleTimer < 2.0f)
        return;
    g_ruleTimer = 0.0f;

    const lektor<Character*>& all = ou->player->getAllPlayerCharacters();
    for (uint32_t i = 0; i < all.size(); i++)
    {
        Character* c = all.stuff[i];
        if (!IsOurSlave(c) || c->isDead())
            continue;

        // weapons: slaves drop any equipped weapon
        if (g_cfg.banWeapons && c->inventory != 0 && c->inventory->hasWeaponEquipped())
        {
            lektor<Item*> weapons;
            c->inventory->getEquippedWeapons(weapons);
            for (uint32_t w = 0; w < weapons.size(); w++)
            {
                Item* it = weapons.stuff[w];
                if (it == 0)
                    continue;
                Trace("'" + c->getName() + "' dropped weapon '" + it->getName() + "'");
                c->inventory->dropItem(it);
            }
            ShowNotice(c->getName() + T(" is a slave and cannot use weapons.", " e escravo(a) e nao pode usar armas."));
        }
    }
}

// ---------------------------------------------------------------------------
// Hotkey (default CTRL+K) acting on the clicked/selected non-player character
// ---------------------------------------------------------------------------

struct Hotkey
{
    int vk;
    bool ctrl, shift, alt;
    bool wasDown;
};

static Hotkey g_enslaveKey = { 0, false, false, false, false };
static Hotkey g_rebellionKey = { 0, false, false, false, false };

static Hotkey ParseKey(const std::string& text)
{
    Hotkey h = { 0, false, false, false, false };
    std::string k = text;
    for (size_t i = 0; i < k.size(); i++)
        k[i] = (char)toupper((unsigned char)k[i]);
    h.ctrl = k.find("CTRL+") != std::string::npos;
    h.shift = k.find("SHIFT+") != std::string::npos;
    h.alt = k.find("ALT+") != std::string::npos;
    size_t plus = k.find_last_of('+');
    std::string key = (plus == std::string::npos) ? k : k.substr(plus + 1);
    if (key.size() == 1 && ((key[0] >= 'A' && key[0] <= 'Z') || (key[0] >= '0' && key[0] <= '9')))
        h.vk = key[0];
    else if (key.size() >= 2 && key[0] == 'F')
    {
        int n = atoi(key.c_str() + 1);
        if (n >= 1 && n <= 12)
            h.vk = VK_F1 + n - 1;
    }
    return h;
}

static void ParseHotkey()
{
    g_enslaveKey = ParseKey(g_cfg.hotkey);
    g_rebellionKey = ParseKey(g_cfg.rebellionTestKey);
    Trace("hotkeys parsed: enslave vk=" + Num((float)g_enslaveKey.vk) + " rebellion test vk=" + Num((float)g_rebellionKey.vk));
}

static bool KenshiHasFocus()
{
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}

// True once per press, only while Kenshi has focus.
static bool KeyPressed(Hotkey& h)
{
    if (h.vk == 0)
        return false;
    bool down = (GetAsyncKeyState(h.vk) & 0x8000) != 0
        && (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 == h.ctrl
        && (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0 == h.shift
        && (GetAsyncKeyState(VK_MENU) & 0x8000) != 0 == h.alt;
    bool pressed = down && !h.wasDown && KenshiHasFocus();
    h.wasDown = down;
    return pressed;
}

static Character* SelectedOtherCharacter()
{
    if (ou == 0 || ou->player == 0)
        return 0;
    Character* c = ou->player->selectedObject.getCharacter();
    if (c == 0)
        c = ou->player->selectedCharacter.getCharacter();
    return c;
}

// The character whose inventory window is under the mouse, if any.
static Character* MouseInventoryCharacter()
{
    if (gui == 0)
        return 0;
    InventoryGUI* inv = gui->getInventoryWindowWithMouse();
    return inv != 0 ? inv->getCallbackCharacter() : 0;
}

static Character* SelectedPlayerCharacter()
{
    return ou->player->selectedCharacter.getCharacter();
}

// ---------------------------------------------------------------------------
// Enslave / free as an action: the performer walks to the target, works for a
// few seconds next to it (shackling / unshackling), then the change happens
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Where each slave came from (SlaverMod_origins.ini), so freeing them can please
// their old faction. Keyed by the character's handle, which saves keep.
// ---------------------------------------------------------------------------

static std::string OriginKey(Character* c)
{
    hand h(c);
    std::ostringstream s;
    s << h.index << "-" << h.serial;
    return s.str();
}

static std::string OriginsFile()
{
    return g_modDir + "SlaverMod_origins.ini";
}

static void SaveOrigin(Character* c, Faction* f)
{
    if (c == 0 || f == 0 || f->getData() == 0)
        return;
    std::string value = f->getData()->stringID + "|" + c->getName();
    WritePrivateProfileStringA("origins", OriginKey(c).c_str(), value.c_str(), OriginsFile().c_str());
    Trace("origin of '" + c->getName() + "' [" + OriginKey(c) + "] = " + f->getName());
}

// Returns the faction a slave was taken from (and forgets it), or 0 if unknown.
static Faction* TakeOrigin(Character* c)
{
    char buf[512] = {0};
    std::string key = OriginKey(c);
    GetPrivateProfileStringA("origins", key.c_str(), "", buf, sizeof(buf), OriginsFile().c_str());
    std::string value = buf;
    if (value.empty())
    {
        Trace("origin of '" + c->getName() + "' [" + key + "] unknown");
        return 0;
    }
    WritePrivateProfileStringA("origins", key.c_str(), NULL, OriginsFile().c_str());
    std::string sid = value.substr(0, value.find('|'));
    return ou->factionMgr != 0 ? ou->factionMgr->getFactionByStringID(sid) : 0;
}

// ---------------------------------------------------------------------------
// New slaves follow and guard whoever enslaved them (once they are awake)
// ---------------------------------------------------------------------------

struct EscortOrder
{
    hand slave;
    hand owner;
};
static std::vector<EscortOrder> g_escorts;
static float g_escortTimer = 0.0f;

static void QueueEscort(Character* slave, Character* owner)
{
    if (!g_cfg.slaveEscort || slave == 0 || owner == 0)
        return;
    EscortOrder e;
    e.slave = hand(slave);
    e.owner = hand(owner);
    g_escorts.push_back(e);
}

static void UpdateEscorts(float dt)
{
    if (g_escorts.empty())
        return;
    g_escortTimer += dt;
    if (g_escortTimer < 2.0f)
        return;
    g_escortTimer = 0.0f;
    for (size_t i = 0; i < g_escorts.size(); )
    {
        Character* slave = g_escorts[i].slave.getCharacter();
        Character* owner = g_escorts[i].owner.getCharacter();
        if (slave == 0 || owner == 0 || slave->isDead() || owner->isDead() || !IsOurSlave(slave))
        {
            g_escorts.erase(g_escorts.begin() + i);
            continue;
        }
        if (slave->medical.isUnconcious())
        {
            i++;
            continue;
        }
        TaskType task = g_cfg.escortMode == "follow" ? FOLLOW_PLAYER_ORDER : BODYGUARD;
        slave->addOrder(0, task, owner, false, true, owner->getPosition());
        Trace("escort: '" + slave->getName() + "' " + g_cfg.escortMode + " '" + owner->getName() + "'");
        ShowNotice(slave->getName() + (g_cfg.escortMode == "follow" ? T(" now follows ", " agora segue ") : T(" now follows and guards ", " agora segue e protege ")) + owner->getName() + ".");
        g_escorts.erase(g_escorts.begin() + i);
    }
}

enum SlaveAction { ACT_ENSLAVE, ACT_FREE };

struct PendingAction
{
    hand performer;
    hand target;
    SlaveAction action;
    float elapsed;      // seconds since the order
    float acting;       // seconds working next to the target; < 0 while still walking
    float bestDist;     // closest distance reached, to notice when the walk is stuck
    float stallTimer;
    Ogre::Vector3 orderedPos;
    FloatingProgressBar* bar;   // shown over the performer while working
};

static std::vector<PendingAction> g_pending;
static bool g_loggedReach = false;

// Crouching first-aid animation + floating progress bar while the performer works.
static void StartActingVisuals(PendingAction& p, Character* performer)
{
    if (g_cfg.progressBar && gui != 0 && p.bar == 0)
    {
        p.bar = gui->createFloatingProgressBar();
        if (p.bar != 0)
        {
            p.bar->setCaption(p.action == ACT_FREE ? T("Freeing", "Libertando") : T("Enslaving", "Escravizando"));
            p.bar->setProgress(0.0f);
            p.bar->setPosition(performer->getPosition());
        }
    }
}

static void UpdateActingVisuals(PendingAction& p, Character* performer)
{
    AnimationClass* anim = performer->getAnimationClass();
    if (anim != 0 && !g_cfg.actionAnimation.empty())
        anim->runAnimation(g_cfg.actionAnimation, 1.0f, 1.0f);   // kept alive every frame, like the game's first aid
    if (p.bar != 0)
    {
        float progress = g_cfg.actionSeconds > 0.0f ? p.acting / g_cfg.actionSeconds : 1.0f;
        p.bar->setProgress(progress > 1.0f ? 1.0f : progress);
        p.bar->setPosition(performer->getPosition());
    }
}

static void StopActingVisuals(PendingAction& p, Character* performer)
{
    if (performer != 0 && !g_cfg.actionAnimation.empty())
    {
        AnimationClass* anim = performer->getAnimationClass();
        if (anim != 0)
            anim->stopAnimation(g_cfg.actionAnimation);
    }
    if (p.bar != 0 && gui != 0)
        gui->destroy(p.bar);
    p.bar = 0;
}

// Small relation hit with the faction of someone the player enslaves.
static void ApplyRelationLoss(Faction* victimFaction, const std::string& victimName)
{
    Faction* pf = PlayerFaction();
    if (g_cfg.relationLoss <= 0.0f || victimFaction == 0 || pf == 0 || victimFaction == pf || victimFaction->relations == 0)
        return;
    float before = victimFaction->relations->getFactionRelation(pf);
    victimFaction->relations->affectRelations(pf, -g_cfg.relationLoss, 1.0f);
    float after = victimFaction->relations->getFactionRelation(pf);
    Trace("relation with '" + victimFaction->getName() + "': " + Num(before) + " -> " + Num(after));
    if (after < before)
        ShowNotice(victimFaction->getName() + T(" did not like seeing ", " nao gostou de ver ") + victimName + T(" enslaved. Relation: ", " escravizado(a). Relacao: ") + Num((float)(int)after) + ".");
}

// Smaller relation gain with the old faction of someone the player frees.
static void ApplyRelationGain(Faction* originFaction, const std::string& name)
{
    Faction* pf = PlayerFaction();
    if (g_cfg.relationGain <= 0.0f || originFaction == 0 || pf == 0 || originFaction == pf || originFaction->relations == 0)
        return;
    float before = originFaction->relations->getFactionRelation(pf);
    originFaction->relations->affectRelations(pf, g_cfg.relationGain, 1.0f);
    float after = originFaction->relations->getFactionRelation(pf);
    Trace("relation with '" + originFaction->getName() + "' (freed): " + Num(before) + " -> " + Num(after));
    if (after > before)
        ShowNotice(originFaction->getName() + T(" liked seeing ", " gostou de ver ") + name + T(" freed. Relation: ", " libertado(a). Relacao: ") + Num((float)(int)after) + ".");
}

static void OrderMove(Character* who, const Ogre::Vector3& where)
{
    who->addOrder(0, MOVE_CUS_ORDERED, 0, false, true, where);
}

static float ReachDistance(Character* performer, Character* target)
{
    return (performer->getRadius() + target->getRadius()) * g_cfg.reachFactor;
}

static bool ActionStillValid(SlaveAction action, Character* performer, Character* target, std::string* why)
{
    if (performer == 0 || performer->isDead()) { *why = T("whoever was acting is no longer available", "quem ia agir nao esta mais disponivel"); return false; }
    if (performer->medical.isUnconcious()) { *why = performer->getName() + T(" was knocked out", " desmaiou"); return false; }
    if (!IsFreePlayerMember(performer)) { *why = performer->getName() + T(" is no longer a free member", " nao e mais um membro livre"); return false; }
    if (target == 0 || target->isDead()) { *why = T("the target is no longer available", "o alvo nao esta mais disponivel"); return false; }
    if (action == ACT_FREE)
    {
        if (!IsOurSlave(target)) { *why = target->getName() + T(" is no longer your slave", " nao e mais seu escravo"); return false; }
        return true;
    }
    if (IsOurSlave(target)) { *why = target->getName() + T(" is already a slave", " ja e escravo(a)"); return false; }
    if (IsPlayerFaction(target))
    {
        if (CountFreeMembers() <= 1) { *why = T("they would be the last free member of your faction", "seria o ultimo membro livre da faccao"); return false; }
        return true;
    }
    return IsEnslaveCandidate(target, why);
}

static void CompleteAction(SlaveAction action, Character* performer, Character* target)
{
    if (action == ACT_FREE)
    {
        Faction* origin = TakeOrigin(target);
        std::string name = target->getName();
        RemoveShackles(target);
        FreeSlave(target);
        if (!IsOurSlave(target))
            ApplyRelationGain(origin, name);
        return;
    }
    if (IsPlayerFaction(target))
    {
        EnslaveOwnMember(target, performer);
        if (IsOurSlave(target))
        {
            GiveShackles(target);
            QueueEscort(target, performer);
        }
        return;
    }
    Faction* victimFaction = target->getFaction();   // before recruiting moves them to ours
    std::string name = target->getName();
    Enslave(target, performer);
    if (IsOurSlave(target))
    {
        GiveShackles(target);
        ApplyRelationLoss(victimFaction, name);
        SaveOrigin(target, victimFaction);
        QueueEscort(target, performer);
    }
}

// Free a slave, or enslave anyone else (own member or prisoner). The selected character
// does it (or the nearest awake free member) and becomes the owner.
static void ToggleSlave(Character* target)
{
    SlaveAction action = IsOurSlave(target) ? ACT_FREE : ACT_ENSLAVE;

    Character* selected = SelectedPlayerCharacter();
    Character* performer = 0;
    float dist = 0.0f;
    if (selected != 0 && selected != target && IsActiveGuard(selected))
        performer = selected;
    else
        performer = NearestFreeMember(target->getPosition(), &dist, target, true);
    if (performer == 0)
    {
        DumpMembers("no performer");
        ShowNotice(T("No free, conscious member can do that.", "Nenhum membro livre e acordado para fazer isso."));
        return;
    }

    std::string why;
    if (!ActionStillValid(action, performer, target, &why))
    {
        ShowNotice(T("Cannot: ", "Nao da: ") + why + ".");
        return;
    }

    if (g_cfg.instantAction)
    {
        CompleteAction(action, performer, target);
        return;
    }

    // one action per performer and per target
    for (size_t i = 0; i < g_pending.size(); )
    {
        Character* other = g_pending[i].performer.getCharacter();
        if (other == performer || g_pending[i].target.getCharacter() == target)
        {
            StopActingVisuals(g_pending[i], other);
            g_pending.erase(g_pending.begin() + i);
        }
        else
            i++;
    }

    PendingAction p;
    p.performer = hand(performer);
    p.target = hand(target);
    p.action = action;
    p.elapsed = 0.0f;
    p.acting = -1.0f;
    p.bestDist = (performer->getPosition() - target->getPosition()).length();
    p.stallTimer = 0.0f;
    p.orderedPos = target->getPosition();
    p.bar = 0;
    g_pending.push_back(p);

    OrderMove(performer, target->getPosition());
    Trace("action " + std::string(action == ACT_FREE ? "free" : "enslave") + ": '" + performer->getName() + "' -> '" + target->getName() +
          "' dist=" + Num(p.bestDist) + " reach=" + Num(ReachDistance(performer, target)) +
          " radii=" + Num(performer->getRadius()) + "/" + Num(target->getRadius()));
    ShowNotice(performer->getName() + (action == ACT_FREE ? T(" will take the shackles off ", " vai tirar os grilhoes de ") : T(" will put shackles on ", " vai colocar grilhoes em ")) + target->getName() + ".");
}

static void UpdatePendingActions(float dt)
{
    for (size_t i = 0; i < g_pending.size(); )
    {
        PendingAction& p = g_pending[i];
        Character* performer = p.performer.getCharacter();
        Character* target = p.target.getCharacter();

        std::string why;
        if (!ActionStillValid(p.action, performer, target, &why))
        {
            Trace("action cancelled: " + why);
            ShowNotice(T("Action cancelled: ", "Acao cancelada: ") + why + ".");
            StopActingVisuals(p, performer);
            g_pending.erase(g_pending.begin() + i);
            continue;
        }

        p.elapsed += dt;
        float dist = (performer->getPosition() - target->getPosition()).length();
        float reach = ReachDistance(performer, target);

        if (p.acting < 0.0f)
        {
            // walking
            if (dist < p.bestDist - reach * 0.1f)
            {
                p.bestDist = dist;
                p.stallTimer = 0.0f;
            }
            else
                p.stallTimer += dt;

            // close enough, or stuck right next to it (cage bars, bodies in the way)
            bool arrived = dist <= reach || (p.stallTimer >= 4.0f && dist <= reach * 3.0f);
            if (arrived)
            {
                if (!g_loggedReach)
                {
                    g_loggedReach = true;
                    Trace("arrived: dist=" + Num(dist) + " reach=" + Num(reach) + " after " + Num(p.elapsed) + "s");
                }
                OrderMove(performer, performer->getPosition());
                p.acting = 0.0f;
                StartActingVisuals(p, performer);
                ShowNotice(performer->getName() + (p.action == ACT_FREE ? T(" is taking the shackles off ", " esta tirando os grilhoes de ") : T(" is putting shackles on ", " esta colocando grilhoes em ")) + target->getName() + "...");
                i++;
                continue;
            }

            if (p.elapsed > g_cfg.approachTimeout)
            {
                Trace("action timeout: dist=" + Num(dist) + " reach=" + Num(reach));
                ShowNotice(performer->getName() + T(" could not reach ", " nao conseguiu chegar ate ") + target->getName() + ".");
                g_pending.erase(g_pending.begin() + i);
                continue;
            }

            // follow a moving target, or retry a blocked path
            if ((target->getPosition() - p.orderedPos).length() > reach || p.stallTimer >= 6.0f)
            {
                p.orderedPos = target->getPosition();
                p.stallTimer = 0.0f;
                OrderMove(performer, p.orderedPos);
            }
            i++;
            continue;
        }

        // working next to the target
        if (dist > reach * 4.0f)
        {
            StopActingVisuals(p, performer);
            p.acting = -1.0f;
            p.bestDist = dist;
            p.stallTimer = 0.0f;
            p.orderedPos = target->getPosition();
            OrderMove(performer, p.orderedPos);
            i++;
            continue;
        }
        p.acting += dt;
        UpdateActingVisuals(p, performer);
        if (p.acting >= g_cfg.actionSeconds)
        {
            SlaveAction action = p.action;
            StopActingVisuals(p, performer);
            g_pending.erase(g_pending.begin() + i);
            CompleteAction(action, performer, target);
            continue;
        }
        i++;
    }
}

static void HandleHotkey()
{
    if (KeyPressed(g_enslaveKey) && g_cfg.debugMode)
    {
        Character* c = MouseInventoryCharacter();
        const char* source = "inventory under mouse";
        if (c == 0)
        {
            c = SelectedOtherCharacter();
            source = "selection";
        }
        if (c == 0)
            ShowNotice(T("Select a character (click on it) first.", "Selecione um personagem (clique nele) antes."));
        else
        {
            Trace("hotkey on '" + c->getName() + "' (" + source + ")");
            ToggleSlave(c);
        }
    }
}

// ---------------------------------------------------------------------------
// ESCRAVIZAR / LIBERTAR button in every open character inventory window
// ---------------------------------------------------------------------------

static const float BTN_X = 0.03f, BTN_Y = 0.920518f, BTN_W = 0.26f, BTN_H = 0.0499076f;

// button -> character of the window it lives in
static std::map<MyGUI::Widget*, hand> g_buttonOwner;
static std::map<MyGUI::Window*, bool> g_dumpedWindows;

static void OnSlaveButton(MyGUI::WidgetPtr sender)
{
    std::map<MyGUI::Widget*, hand>::iterator it = g_buttonOwner.find(sender);
    Character* c = it != g_buttonOwner.end() ? it->second.getCharacter() : 0;
    if (c == 0)
        c = MouseInventoryCharacter();
    if (c == 0)
    {
        Trace("button: no character for this window");
        return;
    }
    Trace("button on '" + c->getName() + "'");
    ToggleSlave(c);
}

// Logs the window's widgets once, to see what shares the bottom row with the button.
static void DumpWindow(MyGUI::Window* win, Character* c)
{
    if (g_dumpedWindows[win])
        return;
    g_dumpedWindows[win] = true;
    const MyGUI::IntSize& size = win->getSize();
    Trace("inventory window of '" + c->getName() + "' size=" + Num((float)size.width) + "x" + Num((float)size.height));
    MyGUI::EnumeratorWidgetPtr children = win->getEnumerator();
    while (children.next())
    {
        MyGUI::Widget* w = children.current();
        const MyGUI::IntCoord& r = w->getCoord();
        Trace("  widget '" + w->getName() + "' at " + Num((float)r.left) + "," + Num((float)r.top) + " " +
              Num((float)r.width) + "x" + Num((float)r.height) + " visible=" + Num(w->getVisible()));
    }
}

static void UpdateButtonIn(InventoryGUI* invGUI)
{
    if (invGUI == 0 || invGUI->layoutMgr == 0)
        return;
    Character* c = invGUI->getCallbackCharacter();
    if (c == 0)
        return;
    MyGUI::Window* win = invGUI->layoutMgr->getWindow();
    if (win == 0 || !win->getVisible())
        return;

    MyGUI::Button* button = 0;
    MyGUI::Widget* found = win->findWidget("SlaverModButton");
    if (found != 0)
        button = found->castType<MyGUI::Button>(false);
    if (button == 0)
    {
        if (g_debug)
            DumpWindow(win, c);
        button = win->createWidgetReal<MyGUI::Button>("Kenshi_Button1", BTN_X, BTN_Y, BTN_W, BTN_H, MyGUI::Align::Default, "SlaverModButton");
        button->eventMouseButtonClick = MyGUI::newDelegate(OnSlaveButton);
        Trace("button created in inventory of '" + c->getName() + "'");
    }
    button->setRealCoord(BTN_X, BTN_Y, BTN_W, BTN_H);
    g_buttonOwner[button] = hand(c);

    bool slave = IsOurSlave(c);
    bool show = slave || IsPlayerFaction(c) || IsEnslaveCandidate(c, 0);
    button->setVisible(show && !c->isDead());
    button->setCaption(slave ? T("FREE", "LIBERTAR") : T("ENSLAVE", "ESCRAVIZAR"));
}

static void UpdateInventoryButton()
{
    if (!g_cfg.showButton || gui == 0 || ou == 0 || ou->player == 0)
        return;
    if (!gui->isAnyInventoryWindowOpen())
        return;
    UpdateButtonIn(gui->getInventoryWindowWithMouse());
    if (ou->player->selectedCharacter.getCharacter() != 0)
        UpdateButtonIn(gui->getInventoryWindow(ou->player->selectedCharacter));
    if (ou->player->selectedObject.getCharacter() != 0)
        UpdateButtonIn(gui->getInventoryWindow(ou->player->selectedObject));
}

// ---------------------------------------------------------------------------
// Escravizar / Libertar option in the hold-right-click order menu
// ---------------------------------------------------------------------------

enum MenuAction { MENU_NONE, MENU_ENSLAVE, MENU_FREE };

static hand g_menuTarget;
static bool g_loggedMenu = false;

// Flags ContextMenuGUI::optionSelected clears after an order is picked (read from its code at startup).
static unsigned char* g_menuFlags[3] = { 0, 0, 0 };

static void FindMenuFlags()
{
    unsigned char* f = (unsigned char*)KenshiLib::GetRealAddress(&ContextMenuGUI::optionSelected);
    const int offsets[3] = { 0x97, 0x9E, 0xA5 };   // three "mov byte ptr [rip+x], 0"
    int found = 0;
    for (int i = 0; i < 3 && f != 0; i++)
    {
        unsigned char* p = f + offsets[i];
        if (p[0] == 0xC6 && p[1] == 0x05 && p[6] == 0x00)
        {
            g_menuFlags[i] = p + 7 + *(int*)(p + 2);
            found++;
        }
    }
    Trace("menu flags found: " + Num((float)found) + "/3");
}

static MenuAction MenuActionFor(Character* target)
{
    if (target == 0 || target->isDead())
        return MENU_NONE;
    if (IsOurSlave(target))
        return MENU_FREE;
    if (IsPlayerFaction(target))
        return (target != SelectedPlayerCharacter() && CountFreeMembers() > 1) ? MENU_ENSLAVE : MENU_NONE;
    return IsEnslaveCandidate(target, 0) ? MENU_ENSLAVE : MENU_NONE;
}

static void OnMenuOption(MyGUI::Widget* sender, int left, int top, MyGUI::MouseButton id)
{
    Character* target = g_menuTarget.getCharacter();
    if (target != 0)
    {
        Trace("menu option on '" + target->getName() + "'");
        ToggleSlave(target);
    }
    // close the menu the same way the game's own options do
    ou->player->contextMenu.showContextMenu(false, 0);
    for (int i = 0; i < 3; i++)
        if (g_menuFlags[i] != 0)
            *g_menuFlags[i] = 0;
}

static bool IsUpperCase(const std::string& s)
{
    bool letters = false;
    for (size_t i = 0; i < s.size(); i++)
    {
        unsigned char ch = (unsigned char)s[i];
        if (ch >= 'a' && ch <= 'z')
            return false;
        if (ch >= 'A' && ch <= 'Z')
            letters = true;
    }
    return letters;
}

// Appends one row to the menu the game just built, copying the geometry of its last row.
static void AddSlaveOption(ContextMenuGUI* menu)
{
    if (!g_cfg.contextMenu || ou == 0 || ou->player == 0 || menu == 0)
        return;
    MyGUI::Widget* main = menu->getMainWidget();
    MyGUI::Widget* list = menu->optionsList;
    if (main == 0 || list == 0 || !main->getVisible() || list->getChildCount() == 0)
        return;

    Character* target = menu->contextMenuTarget.getCharacter();
    MenuAction action = MenuActionFor(target);
    if (action == MENU_NONE)
        return;
    if (list->findWidget("SlaverModMenuRow") != 0)
        return;

    MyGUI::Widget* lastRow = list->getChildAt(list->getChildCount() - 1);
    MyGUI::Widget* lastButton = lastRow->getChildCount() > 0 ? lastRow->getChildAt(0) : 0;
    if (lastButton == 0)
        return;
    const MyGUI::IntCoord rowCoord = lastRow->getCoord();

    if (!g_loggedMenu)
    {
        g_loggedMenu = true;
        Trace("menu: rows=" + Num((float)list->getChildCount()) + " last option='" + lastButton->castType<MyGUI::Button>(false)->getCaption().asUTF8() +
              "' pressed=" + Num(!lastButton->eventMouseButtonPressed.empty()) + " released=" + Num(!lastButton->eventMouseButtonReleased.empty()));
    }

    MyGUI::Widget* row = list->createWidget<MyGUI::Widget>("PanelEmpty",
        MyGUI::IntCoord(rowCoord.left, rowCoord.top + rowCoord.height, rowCoord.width, rowCoord.height), MyGUI::Align::Default, "SlaverModMenuRow");
    MyGUI::Button* button = row->createWidget<MyGUI::Button>("Kenshi_Button1", lastButton->getCoord(), MyGUI::Align::Default, "SlaverModMenuOption");

    std::string caption = action == MENU_FREE ? T("Free", "Libertar") : T("Enslave", "Escravizar");
    MyGUI::Button* lastAsButton = lastButton->castType<MyGUI::Button>(false);
    if (lastAsButton != 0 && IsUpperCase(lastAsButton->getCaption().asUTF8()))
        caption = action == MENU_FREE ? T("FREE", "LIBERTAR") : T("ENSLAVE", "ESCRAVIZAR");
    button->setCaption(caption);

    // the game's options react to the press, not to a full click
    if (!lastButton->eventMouseButtonReleased.empty() && lastButton->eventMouseButtonPressed.empty())
        button->eventMouseButtonReleased += MyGUI::newDelegate(OnMenuOption);
    else
        button->eventMouseButtonPressed += MyGUI::newDelegate(OnMenuOption);

    // grow the menu by one row and keep it on screen
    int listHeight = list->getHeight();
    main->setSize(main->getWidth(), main->getHeight() + rowCoord.height);
    if (list->getHeight() == listHeight)
        list->setSize(list->getWidth(), listHeight + rowCoord.height);
    const MyGUI::IntSize& view = MyGUI::RenderManager::getInstance().getViewSize();
    if (main->getBottom() > view.height)
        main->setPosition(main->getLeft(), view.height - main->getHeight());

    g_menuTarget = menu->contextMenuTarget;
}

void (*ContextMenuGUI_show_orig)(ContextMenuGUI*, const lektor<int>&, const std::string&, bool) = NULL;
void ContextMenuGUI_show_hook(ContextMenuGUI* thisptr, const lektor<int>& orders, const std::string& name, bool offset)
{
    ContextMenuGUI_show_orig(thisptr, orders, name, offset);
    AddSlaveOption(thisptr);
}

// ---------------------------------------------------------------------------
// Rebellion: every few in-game days the slaves' strength is weighed against the
// free members'; if the slaves are stronger they may rise up and join the
// anti-slaver faction, armed with a poor iron club
// ---------------------------------------------------------------------------

static double g_lastRebellionPeriod = -1.0;

// Rebels sit in a squad with no AI package, so they only defend themselves;
// they are pushed to attack the nearest of the player's free members until they fall.
static std::vector<hand> g_rebels;
static float g_rebelTimer = 0.0f;

static void UpdateRebels(float dt)
{
    if (g_rebels.empty())
        return;
    g_rebelTimer += dt;
    if (g_rebelTimer < 2.0f)
        return;
    g_rebelTimer = 0.0f;

    for (size_t i = 0; i < g_rebels.size(); )
    {
        Character* c = g_rebels[i].getCharacter();
        if (c == 0 || c->isDead() || IsPlayerFaction(c))
        {
            g_rebels.erase(g_rebels.begin() + i);
            continue;
        }
        if (c->medical.isUnconcious() || c->isInCombatMode(true, true))
        {
            i++;
            continue;
        }
        float dist = 0.0f;
        Character* target = NearestFreeMember(c->getPosition(), &dist, 0, true);
        if (target == 0 || dist > g_cfg.rebelChaseRange)
        {
            Trace("rebel '" + c->getName() + "' has nobody to fight (dist " + Num(dist) + "), leaving");
            g_rebels.erase(g_rebels.begin() + i);
            continue;
        }
        c->addGoal(MELEE_ATTACK, target);
        Trace("rebel '" + c->getName() + "' attacks '" + target->getName() + "' dist=" + Num(dist));
        i++;
    }
}

static float StrengthOf(Character* c)
{
    CharStats* stats = c->getStats();
    return stats != 0 ? stats->getStat(STAT_STRENGTH, true) : 0.0f;
}

static double GameDays()
{
    return ou->getTimeStamp_inGameHours().getTotalDays();
}

static void GiveRebelWeapon(Character* c)
{
    if (g_cfg.rebelWeapon.empty() || c->inventory == 0)
        return;
    // the factory builds weapons from the manufacturer: (manufacturer, weapon type, quality model)
    GameData* manufacturer = ou->gamedata.getData(g_cfg.rebelWeaponMaker, WEAPON_MANUFACTURER);
    GameData* type = ou->gamedata.getData(g_cfg.rebelWeapon, WEAPON);
    GameData* model = g_cfg.rebelWeaponModel.empty() ? 0 : ou->gamedata.getData(g_cfg.rebelWeaponModel, MATERIAL_SPECS_WEAPON);
    if (manufacturer == 0 || type == 0)
    {
        Trace("rebellion: weapon data missing: maker '" + g_cfg.rebelWeaponMaker + "'=" + Num(manufacturer != 0) +
              " type '" + g_cfg.rebelWeapon + "'=" + Num(type != 0) + " model '" + g_cfg.rebelWeaponModel + "'=" + Num(model != 0));
        return;
    }
    Item* weapon = ou->theFactory->createItem(manufacturer, hand(), type, model, 0, 0);
    if (weapon == 0)
    {
        Trace("rebellion: the game did not create the weapon");
        return;
    }
    std::string name = weapon->getName();
    if (!c->inventory->addItem(weapon, 1, false, true))
    {
        Trace("rebellion: no room for '" + name + "' on '" + c->getName() + "'");
        return;
    }
    bool equipped = c->inventory->equipItem(weapon);
    Trace("rebellion: '" + c->getName() + "' got '" + name + "' equipped=" + Num(equipped));
}

static void StartRebellion(const std::vector<Character*>& rebels)
{
    Faction* rebelFaction = ou->factionMgr != 0 ? ou->factionMgr->getFactionByStringID(g_cfg.rebelFaction) : 0;
    if (rebelFaction == 0)
    {
        Trace("rebellion: faction '" + g_cfg.rebelFaction + "' not found");
        return;
    }
    Platoon* platoon = rebelFaction->createNewEmptyActivePlatoon(0, false, rebels[0]->getPosition());
    ActivePlatoon* squad = platoon != 0 ? platoon->getActivePlatoon() : 0;
    Trace("rebellion: " + Num((float)rebels.size()) + " rebels join '" + rebelFaction->getName() + "' squad=" + Num(squad != 0));

    for (size_t r = 0; r < rebels.size(); r++)
    {
        Character* c = rebels[r];
        // drop any shackling still in progress on them
        for (size_t i = 0; i < g_pending.size(); )
        {
            if (g_pending[i].target.getCharacter() == c || g_pending[i].performer.getCharacter() == c)
            {
                StopActingVisuals(g_pending[i], g_pending[i].performer.getCharacter());
                g_pending.erase(g_pending.begin() + i);
            }
            else
                i++;
        }
        c->setSlaveAIJob(false);
        SetSlaveState(c, NOT_SLAVE);
        c->changeSlaveOwner(hand());
        RemoveShackles(c);
        if (c->isChainedMode())
            c->setChainedMode(false, hand());
        c->setFaction(rebelFaction, squad);
        GiveRebelWeapon(c);
        g_rebels.push_back(hand(c));
        Trace("rebellion: '" + c->getName() + "' str=" + Num(StrengthOf(c)) + " now player=" + Num(IsPlayerFaction(c)) +
              " slaveState=" + Num((float)c->isSlave()));
    }

    Faction* pf = PlayerFaction();
    if (g_cfg.rebellionHostile && pf != 0 && rebelFaction->relations != 0)
    {
        bool wasEnemy = rebelFaction->relations->isEnemy(pf);
        if (!wasEnemy)
            rebelFaction->relations->declareWar(pf);
        Trace("rebellion: '" + rebelFaction->getName() + "' wasEnemy=" + Num(wasEnemy) + " now enemy=" + Num(rebelFaction->relations->isEnemy(pf)) +
              " relation=" + Num(rebelFaction->relations->getFactionRelation(pf)));
    }
    ShowNotice(T("REBELLION! ", "REBELIAO! ") + Num((float)rebels.size()) + T(" slave(s) rose up and joined ", " escravo(s) se rebelaram e se juntaram a ") + rebelFaction->getName() + "!", true);
    g_rebelTimer = 2.0f;   // attack right away
}

// Both sides of the rebellion scale: count x strongest member.
struct RebellionOdds
{
    int freeCount, slaveCount;
    float freeBest, slaveBest;
    float freePower, slavePower;
    float chance;
    std::vector<Character*> rebels;   // slaves strong enough to rise up
};

static RebellionOdds ComputeOdds()
{
    RebellionOdds o;
    o.freeCount = o.slaveCount = 0;
    o.freeBest = o.slaveBest = 0.0f;
    const lektor<Character*>& all = ou->player->getAllPlayerCharacters();
    for (uint32_t i = 0; i < all.size(); i++)
    {
        Character* c = all.stuff[i];
        if (c == 0 || c->isDead())
            continue;
        float str = StrengthOf(c);
        if (IsOurSlave(c))
        {
            o.slaveCount++;
            if (str > o.slaveBest)
                o.slaveBest = str;
            if (str >= g_cfg.rebelMinStrength)
                o.rebels.push_back(c);
        }
        else if (IsFreePlayerMember(c))
        {
            o.freeCount++;
            if (str > o.freeBest)
                o.freeBest = str;
        }
    }
    o.freePower = o.freeCount * o.freeBest;
    o.slavePower = o.slaveCount * o.slaveBest;
    o.chance = 0.0f;
    if (o.slavePower > o.freePower)
    {
        o.chance = (o.slavePower - o.freePower) / o.slavePower * g_cfg.rebellionChanceMult;
        if (o.chance > g_cfg.rebellionMaxChance)
            o.chance = g_cfg.rebellionMaxChance;
    }
    return o;
}

// Weighs both sides and rolls for a rebellion. force = always rebel (test key option).
static void CheckRebellion(bool force)
{
    RebellionOdds o = ComputeOdds();
    if (o.slaveCount == 0)
    {
        Trace("rebellion check: no slaves");
        return;
    }
    int freeCount = o.freeCount, slaveCount = o.slaveCount;
    float freeBest = o.freeBest, slaveBest = o.slaveBest;
    float freePower = o.freePower, slavePower = o.slavePower;
    float chance = o.chance;
    std::vector<Character*>& rebels = o.rebels;
    float roll = (float)rand() / (float)RAND_MAX;
    bool rebel = force || roll < chance;
    Trace("rebellion check: free " + Num((float)freeCount) + " x str " + Num(freeBest) + " = " + Num(freePower) +
          " | slaves " + Num((float)slaveCount) + " x str " + Num(slaveBest) + " = " + Num(slavePower) +
          " | chance=" + Num(chance) + " roll=" + Num(roll) + " force=" + Num(force) + " rebels>=" + Num(g_cfg.rebelMinStrength) +
          ": " + Num((float)rebels.size()));

    std::string balance = T(" (free ", " (livres ") + Num((float)(int)freePower) + T(" x slaves ", " x escravos ") + Num((float)(int)slavePower) + ")";
    if (!rebel)
    {
        if (chance > 0.0f)
            ShowNotice(T("The slaves are restless, but did not rise up", "Os escravos estao inquietos, mas nao se rebelaram") + balance + ".");
        else
            ShowNotice(T("The slaves are under control", "Os escravos estao sob controle") + balance + ".");
        return;
    }
    if (rebels.empty())
    {
        ShowNotice(T("The slaves tried to rise up, but none has strength ", "Os escravos tentaram se rebelar, mas nenhum tem forca ") + Num(g_cfg.rebelMinStrength) + T(" or more.", " ou mais."));
        return;
    }
    StartRebellion(rebels);
}

static void UpdateRebellion()
{
    if (KeyPressed(g_rebellionKey) && g_cfg.debugMode)
    {
        Trace("rebellion test key");
        CheckRebellion(g_cfg.rebellionTestForce);
    }
    if (!g_cfg.rebellionEnabled || g_cfg.rebellionDays <= 0.0f)
        return;

    double period = floor(GameDays() / g_cfg.rebellionDays);
    if (period != g_lastRebellionPeriod)
    {
        // only a period that passed while playing counts; loading a save just resyncs
        bool passed = g_lastRebellionPeriod >= 0.0 && period == g_lastRebellionPeriod + 1.0;
        Trace("rebellion period " + Num((float)period) + " (day " + Num((float)GameDays()) + ")" + (passed ? " - checking" : " - sync"));
        g_lastRebellionPeriod = period;
        if (passed)
            CheckRebellion(false);
    }
}

// ---------------------------------------------------------------------------
// Hooks
// ---------------------------------------------------------------------------

static bool g_loggedSlaves = false;

// Logs once how the hook's time step compares to real time (for tuning the durations).
static float g_calibGame = 0.0f;
static DWORD g_calibStart = 0;
static void CalibrateTime(float dt)
{
    if (g_calibStart == 1)
        return;
    if (g_calibStart == 0)
        g_calibStart = GetTickCount();
    g_calibGame += dt;
    DWORD real = GetTickCount() - g_calibStart;
    if (real >= 20000)
    {
        Trace("time step: " + Num(g_calibGame) + " units in " + Num(real / 1000.0f) + " real seconds");
        g_calibStart = 1;
    }
}

void (*GameWorld_update_orig)(GameWorld*, float) = NULL;
void GameWorld_update_hook(GameWorld* thisptr, float time)
{
    if (GameWorld_update_orig)
        GameWorld_update_orig(thisptr, time);

    if (ou == 0 || ou->player == 0)
        return;

    if (!g_loggedSlaves && ou->player->getAllPlayerCharacters().size() > 0)
    {
        g_loggedSlaves = true;
        int n = 0;
        const lektor<Character*>& all = ou->player->getAllPlayerCharacters();
        for (uint32_t i = 0; i < all.size(); i++)
            if (IsOurSlave(all.stuff[i]))
                n++;
        Trace("world loaded: player characters=" + Num((float)all.size()) + " slaves=" + Num((float)n));
        DumpMembers("world loaded");
        // slaves made before shackles existed in this mod
        for (uint32_t i = 0; i < all.size(); i++)
            if (IsOurSlave(all.stuff[i]))
                GiveShackles(all.stuff[i]);
    }

    HandleHotkey();
    UpdateRebellion();
    UpdateInventoryButton();
    UpdateNotice();
    if (!ou->isPaused())
    {
        ApplySlaveRules(time);
        UpdatePendingActions(time);
        UpdateRebels(time);
        UpdateEscorts(time);
        CalibrateTime(time);
    }
}

// ---------------------------------------------------------------------------
// ModConfigMenu (optional): an in-game settings page in Options > MCM.
// MCM writes SlaverMod.ini and calls ReloadSettings; without MCM nothing changes.
// ---------------------------------------------------------------------------

static void __cdecl ReloadSettings()
{
    LoadSettings();
    ParseHotkey();
}

extern "C" __declspec(dllexport) void MCM_Describe(MCM_Api* api)
{
    static const std::string ini = g_modDir + "SlaverMod.ini";   // must outlive the call
    const char* const sec = "SlaverMod";
#define L(en, pt) T(en, pt).c_str()

    api->beginMod(api, "SlaverMod", "SlaverMod", ini.c_str(), &ReloadSettings);
    if (api->version >= 2)
        api->info(api, L("Your faction can own slaves: enslave, shackles, relations and rebellion. Translated to Brazilian Portuguese.",
                         "Sua facção pode ter escravos: escravizar, grilhões, relações e rebelião. Traduzido para português do Brasil."));

    api->section(api, L("General", "Geral"));
    static const char* const langValues[] = { "en", "pt" };
    static const char* const langLabels[] = { "English", "Português (Brasil)" };
    api->choice(api, sec, "Language", L("Language", "Idioma"), L("Language of the mod's messages and of this page.", "Idioma das mensagens do mod e desta página."),
                "en", langValues, langLabels, 2, MCM_RESTART);
    api->toggle(api, sec, "ContextMenu", L("Option in the right-click menu", "Opção no menu do botão direito"),
                L("Enslave/Free in the order menu (hold the right mouse button over the target).", "Escravizar/Libertar no menu de ordens (segure o botão direito sobre o alvo)."), 1, 0);
    api->toggle(api, sec, "ShowButton", L("Inventory button", "Botão no inventário"),
                L("Also shows the ENSLAVE/FREE button in the inventory window.", "Mostra também o botão ESCRAVIZAR/LIBERTAR na janela de inventário."), 0, 0);
    api->toggle(api, sec, "ShowNotices", L("On-screen messages", "Mensagens na tela"),
                L("Messages at the top of the screen (enslaved, freed, rebellion check...). The REBELLION alert is always shown.",
                  "Mensagens no topo da tela (escravizado, libertado, verificação de rebelião...). O alerta de REBELIÃO sempre aparece."), 1, 0);
    api->text(api, sec, "SquadName", L("Slave squad name", "Nome do esquadrão de escravos"),
              L("Squad new slaves are put in (created if missing).", "Esquadrão onde os novos escravos entram (criado se não existir)."), L("Slaves", "Escravos"), 0);

    api->section(api, L("Enslaving and freeing", "Escravizar e libertar"));
    api->toggle(api, sec, "InstantAction", L("Instant action", "Ação instantânea"),
                L("Enslave/free at once, without the owner walking to the target.", "Escraviza/liberta na hora, sem o dono ir até o alvo."), 0, 0);
    api->number(api, sec, "ActionSeconds", L("Action duration", "Duração da ação"),
                L("Time the owner spends next to the target putting on or taking off the shackles.", "Tempo que o dono passa ao lado do alvo colocando ou tirando os grilhões."), 4.0f, 0.5f, 20.0f, 1, 0);
    api->integer(api, sec, "ApproachTimeout", L("Time limit to arrive", "Tempo máximo para chegar"),
                 L("Time for the owner to reach the target before giving up.", "Tempo para o dono chegar até o alvo antes de desistir."), 120, 10, 600, 0);
    api->number(api, sec, "ReachFactor", L("Reach distance", "Distância para agir"),
                L("How close the owner must get (multiple of the characters' size).", "Quão perto o dono precisa chegar (múltiplo do tamanho dos personagens)."), 2.5f, 1.0f, 6.0f, 1, 0);
    api->toggle(api, sec, "ProgressBar", L("Progress bar", "Barra de progresso"),
                L("Shows the bar over the owner while acting.", "Mostra a barra sobre o dono enquanto ele age."), 1, 0);
    api->text(api, sec, "ActionAnimation", L("Action animation", "Animação da ação"),
              L("Owner's animation while acting (medic = first aid). Empty = none.", "Animação do dono enquanto age (medic = primeiros socorros). Vazio = nenhuma."), "medic", 0);
    api->toggle(api, sec, "UnchainOnEnslave", L("Unchain when enslaving", "Tirar correntes ao escravizar"),
                L("Removes the chains of whoever was chained.", "Tira as correntes de quem estava acorrentado."), 1, 0);

    api->section(api, L("Slaves", "Escravos"));
    api->toggle(api, sec, "GiveShackles", L("Shackles", "Grilhões"),
                L("Slaves get locked shackles; freeing removes them.", "O escravo ganha grilhões trancados; ao ser liberto eles somem."), 1, 0);
    api->toggle(api, sec, "BanWeapons", L("Ban weapons", "Proibir armas"),
                L("Slaves drop any equipped weapon.", "Escravos largam qualquer arma equipada."), 1, 0);
    api->toggle(api, sec, "SlaveEscort", L("Follow the enslaver", "Seguir quem escravizou"),
                L("A new slave follows whoever enslaved them.", "O recém-escravizado passa a seguir quem o escravizou."), 1, 0);
    static const char* const escortValues[] = { "bodyguard", "follow" };
    const char* escortLabels[] = { g_pt ? "Seguir e proteger" : "Follow and guard", g_pt ? "Só seguir" : "Follow only" };
    api->choice(api, sec, "EscortMode", L("Follow mode", "Modo de seguir"), NULL, "bodyguard", escortValues, escortLabels, 2, 0);
    api->toggle(api, sec, "SlaveAIJob", L("Native slave work", "Trabalho escravo nativo"),
                L("Turns on the game's slave work behaviour. Experimental.", "Liga o comportamento de trabalho escravo do jogo. Experimental."), 0, 0);
    api->text(api, sec, "ShackleItem", L("Shackles (ID)", "Grilhão (ID)"), L("Shackles item used.", "Item de grilhão usado."), "SHACKLES", 0);
    api->text(api, sec, "ShackleItemAlt", L("Alternative shackles (ID)", "Grilhão alternativo (ID)"),
              L("Used when the first one does not fit (e.g. hivers).", "Usado quando o primeiro não serve (ex.: colmeia)."), "SHACKLES-BODY", 0);

    api->section(api, L("Relations", "Relações"));
    api->number(api, sec, "RelationLoss", L("Loss when enslaving", "Perda ao escravizar"),
                L("How much the victim's faction relation drops.", "Quanto a relação com a facção da vítima cai."), 5.0f, 0.0f, 50.0f, 1, 0);
    api->number(api, sec, "RelationGain", L("Gain when freeing", "Ganho ao libertar"),
                L("How much the original faction relation rises. Must be lower than the loss.", "Quanto a relação com a facção de origem sobe. Precisa ser menor que a perda."), 2.0f, 0.0f, 50.0f, 1, 0);

    api->section(api, L("Rebellion", "Rebelião"));
    api->toggle(api, sec, "RebellionEnabled", L("Rebellion", "Rebelião"),
                L("Periodically checks whether the slaves rise up.", "Verifica periodicamente se os escravos se rebelam."), 1, 0);
    api->integer(api, sec, "RebellionDays", L("Days between checks", "Dias entre verificações"), NULL, 7, 1, 60, 0);
    api->number(api, sec, "RebellionChanceMult", L("Chance multiplier", "Multiplicador da chance"),
                L("Chance = (slaves - free) / slaves x this value.", "Chance = (escravos - livres) / escravos x este valor."), 1.0f, 0.0f, 5.0f, 2, 0);
    api->number(api, sec, "RebellionMaxChance", L("Maximum chance", "Chance máxima"),
                L("Upper limit of the rebellion chance (0 to 1).", "Limite da chance de rebelião (0 a 1)."), 0.9f, 0.0f, 1.0f, 2, 0);
    api->integer(api, sec, "RebelMinStrength", L("Rebel minimum strength", "Força mínima do rebelde"),
                 L("Only slaves with this strength or more rise up.", "Só escravos com esta força ou mais se rebelam."), 20, 0, 100, 0);
    api->toggle(api, sec, "RebellionHostile", L("War with the rebels", "Guerra com os rebeldes"),
                L("The Anti-Slavers go to war with you on a rebellion.", "Os Abolicionistas entram em guerra com você na rebelião."), 1, 0);
    api->integer(api, sec, "RebelChaseRange", L("Chase distance", "Distância de perseguição"),
                 L("How far rebels chase your free members.", "Até onde os rebeldes perseguem seus membros livres."), 1000, 100, 5000, 0);
    api->text(api, sec, "RebelFaction", L("Rebel faction (ID)", "Facção dos rebeldes (ID)"), NULL, "58229-Dialogue.mod", 0);
    api->text(api, sec, "RebelWeapon", L("Rebel weapon (ID)", "Arma dos rebeldes (ID)"), NULL, "52295-rebirth.mod", 0);
    api->text(api, sec, "RebelWeaponMaker", L("Weapon manufacturer (ID)", "Fabricante da arma (ID)"), NULL, "912-gamedata.base", 0);
    api->text(api, sec, "RebelWeaponModel", L("Weapon quality (ID)", "Qualidade da arma (ID)"), NULL, "913-gamedata.base", 0);

    api->section(api, "Debug");
    api->toggle(api, sec, "DebugMode", L("Debug mode", "Modo debug"),
                L("Turns on the test hotkeys (instant enslave and forced rebellion check).", "Liga os atalhos de teste (escravizar na hora e forçar a verificação de rebelião)."), 0, 0);
    api->hotkey(api, sec, "EnslaveKey", L("Hotkey: enslave/free", "Atalho: escravizar/libertar"),
                L("Debug mode only. Acts on the selected character.", "Só no modo debug. Age sobre o personagem selecionado."), "CTRL+K", 0);
    api->hotkey(api, sec, "RebellionTestKey", L("Hotkey: rebellion check", "Atalho: verificar rebelião"),
                L("Debug mode only. Runs the rebellion check right away.", "Só no modo debug. Faz a verificação de rebelião na hora."), "CTRL+SHIFT+R", 0);
    api->toggle(api, sec, "RebellionTestForce", L("Force rebellion on test", "Forçar rebelião no teste"),
                L("The test hotkey always causes a rebellion.", "O atalho de teste sempre causa rebelião."), 0, 0);
    api->toggle(api, sec, "Debug", L("Write detailed log", "Gravar log detalhado"),
                L("Writes SlaverMod_trace.log in the mod folder.", "Grava SlaverMod_trace.log na pasta do mod."), 0, 0);
#undef L
}

__declspec(dllexport) void startPlugin()
{
    InitModDir();
    LoadSettings();
    ParseHotkey();
    srand(GetTickCount());
    Trace("startPlugin v1.2");

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&GameWorld::_NV_mainLoop_GPUSensitiveStuff), GameWorld_update_hook, (void**)&GameWorld_update_orig))
        ErrorLog("SlaverMod: could not add GameWorld update hook!");

    FindMenuFlags();
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&ContextMenuGUI::show), ContextMenuGUI_show_hook, (void**)&ContextMenuGUI_show_orig))
        ErrorLog("SlaverMod: could not add context menu hook!");
}
