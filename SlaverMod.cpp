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
#include <kenshi/SaveManager.h>
#include <kenshi/FactionWarMgr.h>
#include <kenshi/Damages.h>
#include <kenshi/RaceData.h>

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
    float rebelMinStrength;          // strength needed at the RebelMaxSubmission limit
    float rebelMinStrengthDefiant;   // strength needed at submission 0
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
    bool showSubmission;
    float submissionStart;
    float submissionPrisonerBonus;
    float submissionDailyGain;
    float submissionGainCap;
    float rebelMaxSubmission;
    std::string submissionTestKey;
    bool supervisionEnabled;
    float supervisionRange;            // world units (a character's radius is about 4)
    float submissionUnsupervisedLoss;  // per in-game day without anyone watching
    bool escapeEnabled;
    float escapeMaxSubmission;         // slaves at or above it never run away
    float escapeDailyChance;           // for submission 0, unwatched, no shackles
    float escapeSupervisedMult;
    float escapeShackledMult;
    std::string escapeFaction;         // used when the slave's origin is unknown
    bool escapeMarkEscaped;            // native ESCAPING_SLAVE state (slavers recognise runaways)
    bool escapeRemoveShackles;         // runaways get out of their shackles
    std::string escapeGoal;            // forced | run_away | total | hometown
    float escapeSafeDistance;
    float escapeRecapturePenalty;
    float escapeForgetDays;            // runaways are forgotten after this many game days (0 = never)
    bool punishEnabled;
    float punishSubmission;            // first punishment; each one within PunishCooldownDays gives less
    float punishDamage;                // blunt damage on a limb; grows with repeated punishments
    float punishCooldownDays;
    std::string punishAnimation;       // performer
    std::string punishVictimAnimation; // slave
    bool rewardEnabled;
    float rewardSubmission;
    int rewardCost;                    // cats
    float rewardCooldownDays;
    std::string rewardAnimation;
    float cageDailyGain;               // extra submission per day in a cage
    float cageSubmissionCap;
    bool raceTraits;                   // races and subraces take slavery differently (SlaverMod_races.ini)
    std::string escapeTestKey;
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
    g_cfg.rebelMinStrengthDefiant = IniFloat("RebelMinStrengthDefiant", 10.0f);
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
    g_cfg.showSubmission = IniBool("ShowSubmission", true);
    g_cfg.submissionStart = IniFloat("SubmissionStart", 30.0f);
    g_cfg.submissionPrisonerBonus = IniFloat("SubmissionPrisonerBonus", 10.0f);
    g_cfg.submissionDailyGain = IniFloat("SubmissionDailyGain", 2.0f);
    g_cfg.submissionGainCap = IniFloat("SubmissionGainCap", 60.0f);
    g_cfg.rebelMaxSubmission = IniFloat("RebelMaxSubmission", 50.0f);
    g_cfg.submissionTestKey = IniString("SubmissionTestKey", "CTRL+SHIFT+S");
    g_cfg.supervisionEnabled = IniBool("SupervisionEnabled", true);
    g_cfg.supervisionRange = IniFloat("SupervisionRange", 500.0f);
    g_cfg.submissionUnsupervisedLoss = IniFloat("SubmissionUnsupervisedLoss", 5.0f);
    g_cfg.escapeEnabled = IniBool("EscapeEnabled", true);
    g_cfg.escapeMaxSubmission = IniFloat("EscapeMaxSubmission", 50.0f);
    g_cfg.escapeDailyChance = IniFloat("EscapeDailyChance", 0.5f);
    g_cfg.escapeSupervisedMult = IniFloat("EscapeSupervisedMult", 0.1f);
    g_cfg.escapeShackledMult = IniFloat("EscapeShackledMult", 0.5f);
    g_cfg.escapeFaction = IniString("EscapeFaction", "51707-rebirth.mod");
    g_cfg.escapeMarkEscaped = IniBool("EscapeMarkEscaped", true);
    g_cfg.escapeRemoveShackles = IniBool("EscapeRemoveShackles", true);
    g_cfg.escapeGoal = IniString("EscapeGoal", "forced");
    g_cfg.escapeSafeDistance = IniFloat("EscapeSafeDistance", 1500.0f);
    g_cfg.escapeRecapturePenalty = IniFloat("EscapeRecapturePenalty", 10.0f);
    g_cfg.escapeForgetDays = IniFloat("EscapeForgetDays", 30.0f);
    g_cfg.punishEnabled = IniBool("PunishEnabled", true);
    g_cfg.punishSubmission = IniFloat("PunishSubmission", 15.0f);
    g_cfg.punishDamage = IniFloat("PunishDamage", 10.0f);
    g_cfg.punishCooldownDays = IniFloat("PunishCooldownDays", 1.0f);
    g_cfg.punishAnimation = IniString("PunishAnimation", "");
    g_cfg.punishVictimAnimation = IniString("PunishVictimAnimation", "kneeling hostage idle");
    g_cfg.rewardEnabled = IniBool("RewardEnabled", true);
    g_cfg.rewardSubmission = IniFloat("RewardSubmission", 5.0f);
    g_cfg.rewardCost = (int)IniFloat("RewardCost", 100.0f);
    g_cfg.rewardCooldownDays = IniFloat("RewardCooldownDays", 1.0f);
    g_cfg.rewardAnimation = IniString("RewardAnimation", "");
    g_cfg.cageDailyGain = IniFloat("CageDailyGain", 10.0f);
    g_cfg.cageSubmissionCap = IniFloat("CageSubmissionCap", 75.0f);
    g_cfg.raceTraits = IniBool("RaceTraits", true);
    g_cfg.escapeTestKey = IniString("EscapeTestKey", "CTRL+SHIFT+E");
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
          " max=" + Num(g_cfg.rebellionMaxChance) + " minStr=" + Num(g_cfg.rebelMinStrengthDefiant) + "-" + Num(g_cfg.rebelMinStrength) + " faction=" + g_cfg.rebelFaction +
          " weapon=" + g_cfg.rebelWeapon + " by " + g_cfg.rebelWeaponMaker + "/" + g_cfg.rebelWeaponModel + " hostile=" + Num(g_cfg.rebellionHostile) +
          " testKey=" + g_cfg.rebellionTestKey + " testForce=" + Num(g_cfg.rebellionTestForce) +
          " debugMode=" + Num(g_cfg.debugMode) + " instant=" + Num(g_cfg.instantAction) +
          " actionSeconds=" + Num(g_cfg.actionSeconds) + " timeout=" + Num(g_cfg.approachTimeout) +
          " reach=" + Num(g_cfg.reachFactor) + " shackles=" + Num(g_cfg.giveShackles) +
          " item=" + g_cfg.shackleItem + "/" + g_cfg.shackleItemAlt + " anim=" + g_cfg.actionAnimation +
          " bar=" + Num(g_cfg.progressBar) + " notices=" + Num(g_cfg.showNotices) + " submission: show=" + Num(g_cfg.showSubmission) + " start=" + Num(g_cfg.submissionStart) +
          " prisonerBonus=" + Num(g_cfg.submissionPrisonerBonus) + " daily=" + Num(g_cfg.submissionDailyGain) + " cap=" + Num(g_cfg.submissionGainCap) +
          " rebelMax=" + Num(g_cfg.rebelMaxSubmission) + " supervision=" + Num(g_cfg.supervisionEnabled) + "/" + Num(g_cfg.supervisionRange) +
          " unwatchedLoss=" + Num(g_cfg.submissionUnsupervisedLoss) + " escape=" + Num(g_cfg.escapeEnabled) + " max=" + Num(g_cfg.escapeMaxSubmission) +
          " daily=" + Num(g_cfg.escapeDailyChance) + " watched=" + Num(g_cfg.escapeSupervisedMult) + " shackled=" + Num(g_cfg.escapeShackledMult) +
          " faction=" + g_cfg.escapeFaction + " mark=" + Num(g_cfg.escapeMarkEscaped) + " unshackle=" + Num(g_cfg.escapeRemoveShackles) + " goal=" + g_cfg.escapeGoal +
          " safe=" + Num(g_cfg.escapeSafeDistance) + " recapture=" + Num(g_cfg.escapeRecapturePenalty) + " forget=" + Num(g_cfg.escapeForgetDays) +
          " punish=" + Num(g_cfg.punishEnabled) + "/" + Num(g_cfg.punishSubmission) + "/" + Num(g_cfg.punishDamage) + "/" + Num(g_cfg.punishCooldownDays) +
          "d anim='" + g_cfg.punishAnimation + "'/'" + g_cfg.punishVictimAnimation + "' reward=" + Num(g_cfg.rewardEnabled) + "/" + Num(g_cfg.rewardSubmission) +
          "/" + Num((float)g_cfg.rewardCost) + "cats/" + Num(g_cfg.rewardCooldownDays) + "d cage=" + Num(g_cfg.cageDailyGain) + "/" + Num(g_cfg.cageSubmissionCap) + " races=" + Num(g_cfg.raceTraits) + " relationLoss=" + Num(g_cfg.relationLoss) + " relationGain=" + Num(g_cfg.relationGain) +
          " escort=" + Num(g_cfg.slaveEscort) + "/" + g_cfg.escortMode);
}

// ---------------------------------------------------------------------------
// On-screen notice (top centre, fades after a few seconds)
// ---------------------------------------------------------------------------

static MyGUI::Widget* g_noticePanel = NULL;
static MyGUI::TextBox* g_noticeText = NULL;
static DWORD g_noticeUntil = 0;
static DWORD g_noticeShownAt = 0;
static std::vector<std::string> g_noticeLines;

// Messages sent together (one action, same moment) share the box, up to 3 lines.
static void ShowNotice(const std::string& text, bool always = false)
{
    Trace("notice: " + text);
    if (!g_cfg.showNotices && !always)
        return;
    MyGUI::Gui* mg = MyGUI::Gui::getInstancePtr();
    if (mg == NULL)
        return;

    DWORD now = GetTickCount();
    bool together = g_noticePanel != NULL && g_noticePanel->getVisible() && now - g_noticeShownAt < 200;
    if (!together)
        g_noticeLines.clear();
    g_noticeLines.push_back(text);
    if (g_noticeLines.size() > 3)
        g_noticeLines.erase(g_noticeLines.begin());
    g_noticeShownAt = now;
    std::string all;
    for (size_t i = 0; i < g_noticeLines.size(); i++)
        all += (i > 0 ? "\n" : "") + g_noticeLines[i];

    const int w = 520, h = 22 * (int)g_noticeLines.size() + 18;
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
    g_noticePanel->setCoord((view.width - w) / 2, 90, w, h);
    g_noticeText->setCoord(8, 0, w - 16, h);
    g_noticeText->setCaption(all);
    g_noticePanel->setVisible(true);
    g_noticeUntil = now + 4500 + 1500 * (DWORD)(g_noticeLines.size() - 1);
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
static Hotkey g_submissionKey = { 0, false, false, false, false };
static Hotkey g_escapeKey = { 0, false, false, false, false };

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
    g_submissionKey = ParseKey(g_cfg.submissionTestKey);
    g_escapeKey = ParseKey(g_cfg.escapeTestKey);
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
// Per-save slave records: submission and the faction each slave was taken from.
// Kept in memory, written to saves\<save name>.txt (next to the DLL) when the game
// saves and read back when it loads. Keyed by the character's handle, which the
// game's own saves keep.
// ---------------------------------------------------------------------------

struct SlaveRecord
{
    float submission;     // 0..100
    std::string origin;   // string ID of the faction the slave was taken from ("" = unknown)
    std::string name;     // for the log only
    bool escaped;         // ran away; kept so a recapture remembers them
    double escapedDay;    // game day of the escape (-1 = unknown); old runaways are forgotten
    std::string handle;   // runaways only: full handle, to find them again after a load
};

static std::map<std::string, SlaveRecord> g_records;
static std::string g_recordsSave;   // save the records in memory belong to

// Full handle as "type.container.containerSerial.index.serial" (no '|', safe in the data file).
static std::string HandleText(const hand& h)
{
    std::ostringstream s;
    s << (int)h.type << "." << h.container << "." << h.containerSerial << "." << h.index << "." << h.serial;
    return s.str();
}

static hand HandleFromText(const std::string& text)
{
    unsigned int v[5] = { 0, 0, 0, 0, 0 };
    std::istringstream in(text);
    char dot;
    in >> v[0] >> dot >> v[1] >> dot >> v[2] >> dot >> v[3] >> dot >> v[4];
    if (!in)
        return hand();
    return hand(v[3], v[4], (itemType)v[0], v[1], v[2]);
}

static std::string SlaveKey(Character* c)
{
    hand h(c);
    std::ostringstream s;
    s << h.index << "-" << h.serial;
    return s.str();
}

static std::string SaveDataFile(const std::string& save)
{
    std::string safe = save;
    for (size_t i = 0; i < safe.size(); i++)
        if ((unsigned char)safe[i] < 32 || strchr("\\/:*?\"<>|", safe[i]) != 0)
            safe[i] = '_';
    return g_modDir + "saves\\" + safe + ".txt";
}

static void WriteRecords(const std::string& save)
{
    CreateDirectoryA((g_modDir + "saves").c_str(), NULL);
    std::ofstream f(SaveDataFile(save).c_str(), std::ios::trunc);
    if (!f)
    {
        Trace("records: cannot write " + SaveDataFile(save));
        return;
    }
    f << "# SlaverMod data for save '" << save << "'. key=submission|origin faction|flags (e<day> = escaped on that game day)|handle|name\n";
    for (std::map<std::string, SlaveRecord>::const_iterator it = g_records.begin(); it != g_records.end(); ++it)
    {
        f << it->first << "=" << it->second.submission << "|" << it->second.origin << "|";
        if (it->second.escaped)
            f << "e" << it->second.escapedDay;
        f << "|" << it->second.handle << "|" << it->second.name << "\n";
    }
    Trace("records: wrote " + Num((float)g_records.size()) + " slave(s) for save '" + save + "'");
}

static void ReadRecords(const std::string& save)
{
    g_records.clear();
    g_recordsSave = save;
    std::ifstream f(SaveDataFile(save).c_str());
    std::string line;
    while (std::getline(f, line))
    {
        if (line.empty() || line[0] == '#')
            continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        // fields: v1.3 submission|origin|name, then flags and handle were added before the name
        std::vector<std::string> fields;
        size_t start = eq + 1;
        while (fields.size() < 4)
        {
            size_t bar = line.find('|', start);
            if (bar == std::string::npos)
                break;
            fields.push_back(line.substr(start, bar - start));
            start = bar + 1;
        }
        if (fields.size() < 2)
            continue;
        SlaveRecord r;
        r.submission = (float)atof(fields[0].c_str());
        r.origin = fields[1];
        r.escaped = fields.size() >= 3 && !fields[2].empty() && fields[2][0] == 'e';
        r.escapedDay = r.escaped && fields[2].size() > 1 ? atof(fields[2].c_str() + 1) : -1.0;
        if (fields.size() >= 4)
            r.handle = fields[3];
        r.name = line.substr(start);
        g_records[line.substr(0, eq)] = r;
    }
    Trace("records: read " + Num((float)g_records.size()) + " slave(s) for save '" + save + "'");
}

// Origin written by v1.0-1.2 in SlaverMod_origins.ini (shared by all saves); only read now.
static std::string LegacyOrigin(Character* c)
{
    char buf[512] = {0};
    GetPrivateProfileStringA("origins", SlaveKey(c).c_str(), "", buf, sizeof(buf), (g_modDir + "SlaverMod_origins.ini").c_str());
    std::string value = buf;
    return value.substr(0, value.find('|'));
}

static SlaveRecord* FindRecord(Character* c)
{
    std::map<std::string, SlaveRecord>::iterator it = g_records.find(SlaveKey(c));
    return it == g_records.end() ? 0 : &it->second;
}

static float ClampSubmission(float s)
{
    return s < 0.0f ? 0.0f : (s > 100.0f ? 100.0f : s);
}

static SlaveRecord& CreateRecord(Character* c, const std::string& origin, float submission)
{
    SlaveRecord& r = g_records[SlaveKey(c)];
    r.submission = ClampSubmission(submission);
    r.origin = origin;
    r.name = c->getName();
    r.escaped = false;
    r.escapedDay = -1.0;
    r.handle.clear();
    Trace("record [" + SlaveKey(c) + "] '" + r.name + "' submission=" + Num(r.submission) + " origin=" + (origin.empty() ? "?" : origin));
    return r;
}

static void ForgetRecord(Character* c)
{
    g_records.erase(SlaveKey(c));
}

static Faction* OriginFaction(const std::string& sid)
{
    return sid.empty() || ou->factionMgr == 0 ? 0 : ou->factionMgr->getFactionByStringID(sid);
}

// ---------------------------------------------------------------------------
// Race traits: how each race and subrace takes slavery. Built-in values below,
// overridden or extended by SlaverMod_races.ini (one [race string ID] section each).
// Start = starting submission, Cap = limit of the daily gain (-1 = SubmissionGainCap),
// Gain / Loss / Escape / Punish = multipliers on submission gained, submission lost
// unwatched, escape chance and punishment effect.
// ---------------------------------------------------------------------------

struct RaceTraits
{
    std::string name;
    float start, cap, gain, loss, escape, punish;
};

static std::map<std::string, RaceTraits> g_races;
static RaceTraits g_defaultRace;

static void AddRace(const char* sid, const char* name, float start, float cap, float gain, float loss, float escape, float punish)
{
    RaceTraits t;
    t.name = name;
    t.start = start; t.cap = cap; t.gain = gain; t.loss = loss; t.escape = escape; t.punish = punish;
    g_races[sid] = t;
}

static void LoadRaces()
{
    g_races.clear();
    g_defaultRace.name = "default";
    g_defaultRace.start = g_cfg.submissionStart;
    g_defaultRace.cap = -1.0f;
    g_defaultRace.gain = g_defaultRace.loss = g_defaultRace.escape = g_defaultRace.punish = 1.0f;

    //       string ID                   name                             start  cap  gain loss escape punish
    AddRace("17-gamedata.quack",        "Greenlander",                    30, -1, 1.0f, 1.0f, 1.0f, 1.0f);
    AddRace("18019-gamedata.base",      "Scorchlander",                   25, -1, 1.0f, 1.0f, 1.1f, 1.0f);
    AddRace("5276-chareditor.mod",      "Shek",                           10, 50, 0.6f, 1.5f, 1.4f, 0.5f);
    AddRace("18960-gamedata.base",      "Hive Worker Drone",              60, 85, 1.5f, 0.5f, 0.4f, 1.2f);
    AddRace("57772-Newwworld.mod",      "Hive Worker Drone South Hive",   60, 85, 1.5f, 0.5f, 0.4f, 1.2f);
    AddRace("18961-gamedata.base",      "Hive Soldier Drone",             45, 70, 1.2f, 0.8f, 0.7f, 1.0f);
    AddRace("57771-Newwworld.mod",      "Hive Soldier Drone South Hive",  45, 70, 1.2f, 0.8f, 0.7f, 1.0f);
    AddRace("5346-stick_people.mod",    "Hive Prince",                    20, -1, 0.8f, 1.2f, 1.2f, 0.8f);
    AddRace("57770-Newwworld.mod",      "Hive Prince South Hive",         20, -1, 0.8f, 1.2f, 1.2f, 0.8f);
    AddRace("57335-Newwworld.mod",      "Hive Queen (QueenBirth)",         5, 40, 0.4f, 1.5f, 1.5f, 0.4f);
    AddRace("17946-stick_people.mod",   "Skeleton",                       25, -1, 0.8f, 1.0f, 1.0f, 0.3f);
    AddRace("57778-Newwworld.mod",      "Skeleton P4 Unit",               25, -1, 0.8f, 1.0f, 1.0f, 0.3f);
    AddRace("57782-Newwworld.mod",      "Skeleton Soldierbot",            25, -1, 0.8f, 1.0f, 1.0f, 0.3f);
    AddRace("57774-Newwworld.mod",      "Skeleton Screamer MkI",          25, -1, 0.8f, 1.0f, 1.0f, 0.3f);

    // SlaverMod_races.ini: [default] and any [string ID] section
    std::string file = g_modDir + "SlaverMod_races.ini";
    char names[16384] = {0};
    DWORD n = GetPrivateProfileSectionNamesA(names, sizeof(names), file.c_str());
    int fromFile = 0;
    for (const char* sec = names; n > 0 && *sec != 0; sec += strlen(sec) + 1)
    {
        std::string sid = sec;
        bool isDefault = sid == "default";
        RaceTraits base = isDefault ? g_defaultRace : (g_races.count(sid) ? g_races[sid] : g_defaultRace);
        char buf[256];
        RaceTraits t = base;
        GetPrivateProfileStringA(sec, "Name", base.name.c_str(), buf, sizeof(buf), file.c_str()); t.name = buf;
        const char* keys[6] = { "Start", "Cap", "Gain", "Loss", "Escape", "Punish" };
        float* values[6] = { &t.start, &t.cap, &t.gain, &t.loss, &t.escape, &t.punish };
        for (int k = 0; k < 6; k++)
        {
            GetPrivateProfileStringA(sec, keys[k], "", buf, sizeof(buf), file.c_str());
            if (buf[0] != 0)
                *values[k] = (float)atof(buf);
        }
        if (isDefault)
            g_defaultRace = t;
        else
            g_races[sid] = t;
        fromFile++;
    }
    Trace("races: " + Num((float)g_races.size()) + " known, " + Num((float)fromFile) + " section(s) from SlaverMod_races.ini");
}

static std::string RaceID(Character* c)
{
    RaceData* race = c != 0 ? c->getRace() : 0;
    return race != 0 && race->data != 0 ? race->data->stringID : "";
}

static const RaceTraits& TraitsOf(Character* c)
{
    if (!g_cfg.raceTraits)
        return g_defaultRace;
    std::map<std::string, RaceTraits>::const_iterator it = g_races.find(RaceID(c));
    return it != g_races.end() ? it->second : g_defaultRace;
}

static float GainCapOf(const RaceTraits& t)
{
    return t.cap >= 0.0f ? t.cap : g_cfg.submissionGainCap;
}

static float SubmissionOf(Character* c)
{
    SlaveRecord* r = FindRecord(c);
    return r != 0 ? r->submission : TraitsOf(c).start;
}

static std::string SubmissionLevel(float s)
{
    if (s < 25.0f) return T("Defiant", "Rebelde");
    if (s < 50.0f) return T("Restless", "Inquieto");
    if (s < 75.0f) return T("Obedient", "Obediente");
    return T("Loyal", "Leal");
}

static std::string SubmissionText(float s)
{
    return T("Submission ", "Submissao ") + Num((float)(int)(s + 0.5f)) + " (" + SubmissionLevel(s) + ")";
}

// Matches the loaded world's slaves with the records: slaves without one (made by
// an older version, or a save from before this file existed) get the starting value.
// Records of runaways are kept: they are no longer in the player's faction.
static void SyncRecords()
{
    const lektor<Character*>& all = ou->player->getAllPlayerCharacters();
    std::map<std::string, SlaveRecord> kept;
    for (std::map<std::string, SlaveRecord>::const_iterator it = g_records.begin(); it != g_records.end(); ++it)
        if (it->second.escaped)
            kept.insert(*it);
    int created = 0;
    for (uint32_t i = 0; i < all.size(); i++)
    {
        Character* c = all.stuff[i];
        if (c == 0 || !IsOurSlave(c))
            continue;
        SlaveRecord* r = FindRecord(c);
        if (r == 0)
        {
            r = &CreateRecord(c, LegacyOrigin(c), TraitsOf(c).start);
            created++;
        }
        else if (r->name != c->getName())
            Trace("record [" + SlaveKey(c) + "] was '" + r->name + "', now '" + c->getName() + "'");
        r->name = c->getName();
        r->escaped = false;
        r->handle.clear();
        kept[SlaveKey(c)] = *r;
    }
    Trace("records synced: " + Num((float)kept.size()) + " slave(s), " + Num((float)created) + " new, " +
          Num((float)(g_records.size() - kept.size())) + " dropped");
    g_records.swap(kept);
}

// ---------------------------------------------------------------------------
// Submission over time and on screen
// ---------------------------------------------------------------------------

static double GameDays();
static double g_lastDriftDay = -1.0;
static std::map<std::string, float> g_unsupervisedDays;   // per slave key, since someone last watched them

// Watched: caged, or a free, awake member of the faction within SupervisionRange.
static bool IsSupervised(Character* c, float* distOut)
{
    float dist = 0.0f;
    Character* guard = NearestFreeMember(c->getPosition(), &dist, c, true);
    if (distOut != 0)
        *distOut = guard != 0 ? dist : -1.0f;
    if (c->inSomething == IN_PRISON)
        return true;
    return guard != 0 && dist <= g_cfg.supervisionRange;
}

static void TryEscape(Character* c, SlaveRecord& r, double days, bool supervised);
static void StartEscape(Character* c, const char* why);
static void PruneRunaways();

// About once an in-game hour: watched slaves slowly get used to their condition (up to
// SubmissionGainCap), unwatched ones lose submission and may run away.
static void UpdateSubmission()
{
    double now = GameDays();
    if (g_lastDriftDay < 0.0 || now < g_lastDriftDay || now - g_lastDriftDay > 1.0)
    {
        g_lastDriftDay = now;   // first frame, or a jump in time (loading): just resync
        return;
    }
    double days = now - g_lastDriftDay;
    if (days < 0.05)
        return;
    g_lastDriftDay = now;
    PruneRunaways();

    std::vector<Character*> slaves;   // copied: an escape changes the player's character list
    const lektor<Character*>& all = ou->player->getAllPlayerCharacters();
    for (uint32_t i = 0; i < all.size(); i++)
        if (IsOurSlave(all.stuff[i]) && !all.stuff[i]->isDead())
            slaves.push_back(all.stuff[i]);

    for (size_t i = 0; i < slaves.size(); i++)
    {
        Character* c = slaves[i];
        SlaveRecord* r = FindRecord(c);
        if (r == 0)
            continue;
        const RaceTraits& race = TraitsOf(c);
        float dist = 0.0f;
        bool supervised = !g_cfg.supervisionEnabled || IsSupervised(c, &dist);
        float& alone = g_unsupervisedDays[SlaveKey(c)];
        float before = r->submission;
        if (supervised)
        {
            alone = 0.0f;
            float cap = GainCapOf(race);
            if (r->submission < cap)
            {
                r->submission += (float)(g_cfg.submissionDailyGain * race.gain * days);
                if (r->submission > cap)
                    r->submission = cap;
            }
            // time in a cage breaks them faster, up to CageSubmissionCap
            if (c->inSomething == IN_PRISON && r->submission < g_cfg.cageSubmissionCap)
            {
                r->submission += (float)(g_cfg.cageDailyGain * race.gain * days);
                if (r->submission > g_cfg.cageSubmissionCap)
                    r->submission = g_cfg.cageSubmissionCap;
            }
        }
        else
        {
            alone += (float)days;
            r->submission = ClampSubmission(r->submission - (float)(g_cfg.submissionUnsupervisedLoss * race.loss * days));
        }
        if (!supervised || (int)before != (int)r->submission)
            Trace("slave '" + c->getName() + "' supervised=" + Num(supervised) + " guard dist=" + Num(dist) +
                  " submission " + Num(before) + " -> " + Num(r->submission) + " alone=" + Num(alone) + "d");
        if (g_cfg.escapeEnabled)
            TryEscape(c, *r, days, supervised);
    }
}

// One bar over the selected slave with its submission.
static FloatingProgressBar* g_submissionBar = NULL;

static void HideSubmissionBar()
{
    if (g_submissionBar != NULL && gui != 0)
        gui->destroy(g_submissionBar);
    g_submissionBar = NULL;
}

static void UpdateSubmissionBar()
{
    Character* c = g_cfg.showSubmission && gui != 0 ? ou->player->selectedCharacter.getCharacter() : 0;
    if (c == 0 || !IsOurSlave(c) || c->isDead())
    {
        HideSubmissionBar();
        return;
    }
    if (g_submissionBar == NULL)
        g_submissionBar = gui->createFloatingProgressBar();
    if (g_submissionBar == NULL)
        return;
    float s = SubmissionOf(c);
    std::string caption = SubmissionText(s);
    if (g_cfg.supervisionEnabled && !IsSupervised(c, 0))
        caption += T(" - unwatched", " - sem vigilancia");
    g_submissionBar->setCaption(caption);
    g_submissionBar->setProgress(s / 100.0f);
    g_submissionBar->setPosition(c->getPosition());
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

enum SlaveAction { ACT_ENSLAVE, ACT_FREE, ACT_PUNISH, ACT_REWARD };

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
static const std::string& PerformerAnimation(SlaveAction action)
{
    if (action == ACT_PUNISH)
        return g_cfg.punishAnimation;
    if (action == ACT_REWARD)
        return g_cfg.rewardAnimation;
    return g_cfg.actionAnimation;
}

static const std::string& TargetAnimation(SlaveAction action)
{
    static const std::string none;
    return action == ACT_PUNISH ? g_cfg.punishVictimAnimation : none;
}

static std::string ActingCaption(SlaveAction action)
{
    switch (action)
    {
    case ACT_FREE: return T("Freeing", "Libertando");
    case ACT_PUNISH: return T("Punishing", "Punindo");
    case ACT_REWARD: return T("Rewarding", "Recompensando");
    default: return T("Enslaving", "Escravizando");
    }
}

static void RunAnimation(Character* c, const std::string& name)
{
    AnimationClass* anim = c != 0 && !name.empty() ? c->getAnimationClass() : 0;
    if (anim != 0)
        anim->runAnimation(name, 1.0f, 1.0f);   // kept alive every frame, like the game's first aid
}

static void StopAnimation(Character* c, const std::string& name)
{
    AnimationClass* anim = c != 0 && !name.empty() ? c->getAnimationClass() : 0;
    if (anim != 0)
        anim->stopAnimation(name);
}

static void StartActingVisuals(PendingAction& p, Character* performer)
{
    if (g_cfg.progressBar && gui != 0 && p.bar == 0)
    {
        p.bar = gui->createFloatingProgressBar();
        if (p.bar != 0)
        {
            p.bar->setCaption(ActingCaption(p.action));
            p.bar->setProgress(0.0f);
            p.bar->setPosition(performer->getPosition());
        }
    }
}

static void UpdateActingVisuals(PendingAction& p, Character* performer)
{
    RunAnimation(performer, PerformerAnimation(p.action));
    RunAnimation(p.target.getCharacter(), TargetAnimation(p.action));
    if (p.bar != 0)
    {
        float progress = g_cfg.actionSeconds > 0.0f ? p.acting / g_cfg.actionSeconds : 1.0f;
        p.bar->setProgress(progress > 1.0f ? 1.0f : progress);
        p.bar->setPosition(performer->getPosition());
    }
}

static void StopActingVisuals(PendingAction& p, Character* performer)
{
    StopAnimation(performer, PerformerAnimation(p.action));
    StopAnimation(p.target.getCharacter(), TargetAnimation(p.action));
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
    if (action == ACT_FREE || action == ACT_PUNISH || action == ACT_REWARD)
    {
        if (!IsOurSlave(target)) { *why = target->getName() + T(" is no longer your slave", " nao e mais seu escravo"); return false; }
        if (action == ACT_REWARD && performer->getMoney() < g_cfg.rewardCost)
        {
            *why = T("not enough money (", "dinheiro insuficiente (") + Num((float)g_cfg.rewardCost) + T(" cats needed)", " cats)");
            return false;
        }
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

// Recent punishments / rewards per slave (memory only): repeating them soon gives less.
struct DisciplineState
{
    double lastPunishDay, lastRewardDay;
    int punishStreak;
};
static std::map<std::string, DisciplineState> g_discipline;

static DisciplineState& DisciplineOf(Character* c)
{
    std::map<std::string, DisciplineState>::iterator it = g_discipline.find(SlaveKey(c));
    if (it != g_discipline.end())
        return it->second;
    DisciplineState d = { -1000.0, -1000.0, 0 };
    return g_discipline[SlaveKey(c)] = d;
}

// A beating: blunt damage on a random flesh limb (more for each recent punishment),
// and submission, less for each recent punishment.
static void Punish(Character* performer, Character* target)
{
    SlaveRecord* r = FindRecord(target);
    if (r == 0)
        return;
    DisciplineState& d = DisciplineOf(target);
    double now = GameDays();
    d.punishStreak = now - d.lastPunishDay < g_cfg.punishCooldownDays ? d.punishStreak + 1 : 0;
    d.lastPunishDay = now;

    float gain = g_cfg.punishSubmission * TraitsOf(target).punish / (1.0f + d.punishStreak);
    float damage = g_cfg.punishDamage * (1.0f + d.punishStreak);
    MedicalSystem::HealthPartStatus* limbs[4] = { target->medical.leftArm, target->medical.rightArm, target->medical.leftLeg, target->medical.rightLeg };
    std::vector<MedicalSystem::HealthPartStatus*> flesh;
    for (int i = 0; i < 4; i++)
        if (limbs[i] != 0 && !limbs[i]->isRobotic())
            flesh.push_back(limbs[i]);
    if (!flesh.empty() && damage > 0.0f)
    {
        MedicalSystem::HealthPartStatus* part = flesh[rand() % flesh.size()];
        float before = part->flesh;
        part->applyDamage(Damages(0.0f, damage, 0.0f, 0.0f, 0.0f));
        Trace("punish: limb flesh " + Num(before) + " -> " + Num(part->flesh) + " (blunt " + Num(damage) + ")");
    }
    float before = r->submission;
    r->submission = ClampSubmission(r->submission + gain);
    Trace("punish '" + target->getName() + "' by '" + performer->getName() + "': streak=" + Num((float)d.punishStreak) +
          " submission " + Num(before) + " -> " + Num(r->submission));
    ShowNotice(performer->getName() + T(" punished ", " puniu ") + target->getName() + ". " + SubmissionText(r->submission) + ".");
}

// Some cats for a little submission; once per RewardCooldownDays per slave.
static void Reward(Character* performer, Character* target)
{
    SlaveRecord* r = FindRecord(target);
    if (r == 0)
        return;
    DisciplineState& d = DisciplineOf(target);
    double now = GameDays();
    if (now - d.lastRewardDay < g_cfg.rewardCooldownDays)
    {
        ShowNotice(target->getName() + T(" was rewarded recently.", " foi recompensado(a) ha pouco tempo."));
        return;
    }
    if (g_cfg.rewardCost > 0 && !performer->takeMoney(g_cfg.rewardCost))
    {
        ShowNotice(T("Not enough money to reward ", "Dinheiro insuficiente para recompensar ") + target->getName() + ".");
        return;
    }
    d.lastRewardDay = now;
    float before = r->submission;
    r->submission = ClampSubmission(r->submission + g_cfg.rewardSubmission * TraitsOf(target).gain);
    Trace("reward '" + target->getName() + "' by '" + performer->getName() + "' for " + Num((float)g_cfg.rewardCost) +
          " cats: submission " + Num(before) + " -> " + Num(r->submission));
    ShowNotice(performer->getName() + T(" rewarded ", " recompensou ") + target->getName() + " (" + Num((float)g_cfg.rewardCost) + " cats). " +
               SubmissionText(r->submission) + ".");
}

static void CompleteAction(SlaveAction action, Character* performer, Character* target)
{
    if (action == ACT_PUNISH)
    {
        Punish(performer, target);
        return;
    }
    if (action == ACT_REWARD)
    {
        Reward(performer, target);
        return;
    }
    if (action == ACT_FREE)
    {
        SlaveRecord* record = FindRecord(target);
        Faction* origin = OriginFaction(record != 0 ? record->origin : LegacyOrigin(target));
        std::string name = target->getName();
        RemoveShackles(target);
        FreeSlave(target);
        if (!IsOurSlave(target))
        {
            ForgetRecord(target);
            ApplyRelationGain(origin, name);
        }
        return;
    }
    if (IsPlayerFaction(target))
    {
        EnslaveOwnMember(target, performer);
        if (IsOurSlave(target))
        {
            Trace("race of '" + target->getName() + "': " + RaceID(target) + " -> " + TraitsOf(target).name);
            SlaveRecord& r = CreateRecord(target, "", TraitsOf(target).start);
            ShowNotice(target->getName() + T(" is now a slave. ", " agora e escravo(a). ") + SubmissionText(r.submission) + ".");
            GiveShackles(target);
            QueueEscort(target, performer);
        }
        return;
    }
    Faction* victimFaction = target->getFaction();   // before recruiting moves them to ours
    std::string origin = victimFaction != 0 && victimFaction->getData() != 0 ? victimFaction->getData()->stringID : "";
    bool wasPrisoner = target->inSomething == IN_PRISON || target->isChainedMode();   // before unchaining
    const RaceTraits& race = TraitsOf(target);
    float start = race.start + (wasPrisoner ? g_cfg.submissionPrisonerBonus : 0.0f);
    Trace("race of '" + target->getName() + "': " + RaceID(target) + " -> " + race.name);
    SlaveRecord* runaway = FindRecord(target);
    bool recaptured = runaway != 0 && runaway->escaped;
    if (recaptured)
    {
        origin = runaway->origin;   // where they really came from
        start = runaway->submission - g_cfg.escapeRecapturePenalty;
    }
    std::string name = target->getName();
    Enslave(target, performer);
    if (IsOurSlave(target))
    {
        SlaveRecord& r = CreateRecord(target, origin, start);
        if (recaptured)
            ShowNotice(name + T(" was recaptured. ", " foi recapturado(a). ") + SubmissionText(r.submission) + ".");
        else
            ShowNotice(name + T(" is now a slave. ", " agora e escravo(a). ") + SubmissionText(r.submission) + ".");
        GiveShackles(target);
        if (!recaptured)   // a runaway is still yours: no one to offend
            ApplyRelationLoss(victimFaction, name);
        QueueEscort(target, performer);
    }
}

// Free a slave, or enslave anyone else (own member or prisoner). The selected character
// does it (or the nearest awake free member) and becomes the owner.
static std::string WillDoText(SlaveAction action)
{
    switch (action)
    {
    case ACT_FREE: return T(" will take the shackles off ", " vai tirar os grilhoes de ");
    case ACT_PUNISH: return T(" will punish ", " vai punir ");
    case ACT_REWARD: return T(" will reward ", " vai recompensar ");
    default: return T(" will put shackles on ", " vai colocar grilhoes em ");
    }
}

static std::string DoingText(SlaveAction action)
{
    switch (action)
    {
    case ACT_FREE: return T(" is taking the shackles off ", " esta tirando os grilhoes de ");
    case ACT_PUNISH: return T(" is punishing ", " esta punindo ");
    case ACT_REWARD: return T(" is rewarding ", " esta recompensando ");
    default: return T(" is putting shackles on ", " esta colocando grilhoes em ");
    }
}

static void StartAction(Character* target, SlaveAction action)
{

    // the selected member closest to the target does it (with a group selected, the
    // "main" selected character can be anywhere)
    Character* nearestSelected = ou->player->getNearestSelectedCharacterTo(target->getPosition());
    Character* selected = SelectedPlayerCharacter();
    Character* performer = 0;
    float dist = 0.0f;
    if (nearestSelected != 0 && nearestSelected != target && IsActiveGuard(nearestSelected))
        performer = nearestSelected;
    else if (selected != 0 && selected != target && IsActiveGuard(selected))
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
    Trace("action " + Num((float)action) + ": '" + performer->getName() + "' -> '" + target->getName() +
          "' dist=" + Num(p.bestDist) + " reach=" + Num(ReachDistance(performer, target)) +
          " radii=" + Num(performer->getRadius()) + "/" + Num(target->getRadius()));
    ShowNotice(performer->getName() + WillDoText(action) + target->getName() + ".");
}

// Free a slave, or enslave anyone else (the hotkey).
static void ToggleSlave(Character* target)
{
    StartAction(target, IsOurSlave(target) ? ACT_FREE : ACT_ENSLAVE);
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
                ShowNotice(performer->getName() + DoingText(p.action) + target->getName() + "...");
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
    // test key: +25 submission on the selected slave, wrapping back to 0 after 100
    if (KeyPressed(g_submissionKey) && g_cfg.debugMode)
    {
        Character* c = ou->player->selectedCharacter.getCharacter();
        SlaveRecord* r = c != 0 && IsOurSlave(c) ? FindRecord(c) : 0;
        if (r == 0)
            ShowNotice(T("Select one of your slaves first.", "Selecione um escravo seu antes."));
        else
        {
            r->submission = r->submission >= 100.0f ? 0.0f : ClampSubmission(r->submission + 25.0f);
            Trace("submission test key: '" + c->getName() + "' -> " + Num(r->submission));
            ShowNotice(c->getName() + ": " + SubmissionText(r->submission));
        }
    }
    // test key: the selected slave runs away now
    if (KeyPressed(g_escapeKey) && g_cfg.debugMode)
    {
        Character* c = ou->player->selectedCharacter.getCharacter();
        if (c == 0 || !IsOurSlave(c) || FindRecord(c) == 0)
            ShowNotice(T("Select one of your slaves first.", "Selecione um escravo seu antes."));
        else
            StartEscape(c, "test key");
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

// Options offered for a target: enslave someone, or free / punish / reward one of your slaves.
static std::vector<SlaveAction> MenuActionsFor(Character* target)
{
    std::vector<SlaveAction> out;
    if (target == 0 || target->isDead())
        return out;
    if (IsOurSlave(target))
    {
        out.push_back(ACT_FREE);
        if (g_cfg.punishEnabled)
            out.push_back(ACT_PUNISH);
        if (g_cfg.rewardEnabled)
            out.push_back(ACT_REWARD);
        return out;
    }
    if (IsPlayerFaction(target))
    {
        if (target != SelectedPlayerCharacter() && CountFreeMembers() > 1)
            out.push_back(ACT_ENSLAVE);
        return out;
    }
    if (IsEnslaveCandidate(target, 0))
        out.push_back(ACT_ENSLAVE);
    return out;
}

static void OnMenuOption(MyGUI::Widget* sender, int left, int top, MyGUI::MouseButton id)
{
    Character* target = g_menuTarget.getCharacter();
    if (target != 0 && sender != 0)
    {
        SlaveAction action = (SlaveAction)atoi(sender->getUserString("SlaverModAction").c_str());
        Trace("menu option " + Num((float)action) + " on '" + target->getName() + "'");
        StartAction(target, action);
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

static std::string MenuCaption(SlaveAction action, bool upper)
{
    switch (action)
    {
    case ACT_FREE: return upper ? T("FREE", "LIBERTAR") : T("Free", "Libertar");
    case ACT_PUNISH: return upper ? T("PUNISH", "PUNIR") : T("Punish", "Punir");
    case ACT_REWARD: return upper ? T("REWARD", "RECOMPENSAR") : T("Reward", "Recompensar");
    default: return upper ? T("ENSLAVE", "ESCRAVIZAR") : T("Enslave", "Escravizar");
    }
}

// Appends our rows to the menu the game just built, copying the geometry of its last row.
static void AddSlaveOption(ContextMenuGUI* menu)
{
    if (!g_cfg.contextMenu || ou == 0 || ou->player == 0 || menu == 0)
        return;
    MyGUI::Widget* main = menu->getMainWidget();
    MyGUI::Widget* list = menu->optionsList;
    if (main == 0 || list == 0 || !main->getVisible() || list->getChildCount() == 0)
        return;

    Character* target = menu->contextMenuTarget.getCharacter();
    std::vector<SlaveAction> actions = MenuActionsFor(target);
    if (actions.empty())
        return;
    if (list->findWidget("SlaverModMenuRow0") != 0)
        return;

    MyGUI::Widget* lastRow = list->getChildAt(list->getChildCount() - 1);
    MyGUI::Widget* lastButton = lastRow->getChildCount() > 0 ? lastRow->getChildAt(0) : 0;
    if (lastButton == 0)
        return;
    const MyGUI::IntCoord rowCoord = lastRow->getCoord();
    MyGUI::Button* lastAsButton = lastButton->castType<MyGUI::Button>(false);
    bool upper = lastAsButton != 0 && IsUpperCase(lastAsButton->getCaption().asUTF8());
    // the game's options react to the release, not to a full click
    bool onRelease = !lastButton->eventMouseButtonReleased.empty() && lastButton->eventMouseButtonPressed.empty();

    if (!g_loggedMenu)
    {
        g_loggedMenu = true;
        Trace("menu: rows=" + Num((float)list->getChildCount()) + " last option='" + (lastAsButton != 0 ? lastAsButton->getCaption().asUTF8() : std::string("?")) +
              "' pressed=" + Num(!lastButton->eventMouseButtonPressed.empty()) + " released=" + Num(!lastButton->eventMouseButtonReleased.empty()));
    }

    for (size_t k = 0; k < actions.size(); k++)
    {
        MyGUI::Widget* row = list->createWidget<MyGUI::Widget>("PanelEmpty",
            MyGUI::IntCoord(rowCoord.left, rowCoord.top + rowCoord.height * (int)(k + 1), rowCoord.width, rowCoord.height),
            MyGUI::Align::Default, "SlaverModMenuRow" + Num((float)k));
        MyGUI::Button* button = row->createWidget<MyGUI::Button>("Kenshi_Button1", lastButton->getCoord(), MyGUI::Align::Default,
            "SlaverModMenuOption" + Num((float)k));
        button->setCaption(MenuCaption(actions[k], upper));
        button->setUserString("SlaverModAction", Num((float)actions[k]));
        if (onRelease)
            button->eventMouseButtonReleased += MyGUI::newDelegate(OnMenuOption);
        else
            button->eventMouseButtonPressed += MyGUI::newDelegate(OnMenuOption);
    }

    // grow the menu by our rows and keep it on screen
    int extra = rowCoord.height * (int)actions.size();
    int listHeight = list->getHeight();
    main->setSize(main->getWidth(), main->getHeight() + extra);
    if (list->getHeight() == listHeight)
        list->setSize(list->getWidth(), listHeight + extra);
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
        ForgetRecord(c);
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

// Strength a defiant slave needs to rise up: RebelMinStrengthDefiant at submission 0,
// growing in a straight line to RebelMinStrength at the RebelMaxSubmission limit.
static float RebelStrengthNeeded(float submission)
{
    float t = g_cfg.rebelMaxSubmission > 0.0f ? submission / g_cfg.rebelMaxSubmission : 1.0f;
    if (t > 1.0f)
        t = 1.0f;
    return g_cfg.rebelMinStrengthDefiant + (g_cfg.rebelMinStrength - g_cfg.rebelMinStrengthDefiant) * t;
}

// Free side: count x strongest member. Slave side: only the defiant slaves (submission
// below RebelMaxSubmission) take part: count x strongest x (100 - their average submission) / 100.
struct RebellionOdds
{
    int freeCount, slaveCount, defiantCount;
    float freeBest, slaveBest, defiantSubmission;
    float freePower, slavePower;
    float chance;
    std::vector<Character*> rebels;   // defiant slaves strong enough to rise up
};

static RebellionOdds ComputeOdds()
{
    RebellionOdds o;
    o.freeCount = o.slaveCount = o.defiantCount = 0;
    o.freeBest = o.slaveBest = o.defiantSubmission = 0.0f;
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
            float sub = SubmissionOf(c);
            if (sub >= g_cfg.rebelMaxSubmission)
                continue;
            o.defiantCount++;
            o.defiantSubmission += sub;
            Trace("  defiant '" + c->getName() + "' submission=" + Num(sub) + " str=" + Num(str) + " needs " + Num(RebelStrengthNeeded(sub)));
            if (str > o.slaveBest)
                o.slaveBest = str;
            if (str >= RebelStrengthNeeded(sub))
                o.rebels.push_back(c);
        }
        else if (IsFreePlayerMember(c))
        {
            o.freeCount++;
            if (str > o.freeBest)
                o.freeBest = str;
        }
    }
    if (o.defiantCount > 0)
        o.defiantSubmission /= o.defiantCount;
    o.freePower = o.freeCount * o.freeBest;
    o.slavePower = o.defiantCount * o.slaveBest * (100.0f - o.defiantSubmission) / 100.0f;
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
          " | slaves " + Num((float)slaveCount) + ", defiant " + Num((float)o.defiantCount) + " (avg submission " + Num(o.defiantSubmission) +
          ") x str " + Num(slaveBest) + " = " + Num(slavePower) +
          " | chance=" + Num(chance) + " roll=" + Num(roll) + " force=" + Num(force) + " rebels(str>=" + Num(g_cfg.rebelMinStrengthDefiant) + ".." + Num(g_cfg.rebelMinStrength) + ")" +
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
    if (o.defiantCount == 0)
    {
        ShowNotice(T("The slaves thought about rising up, but they are too submissive.", "Os escravos pensaram em se rebelar, mas sao submissos demais."));
        return;
    }
    if (rebels.empty())
    {
        ShowNotice(T("The slaves tried to rise up, but none was strong enough.", "Os escravos tentaram se rebelar, mas nenhum era forte o bastante."));
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
// Escape: unwatched, defiant slaves may run away. They leave the faction (back to
// the one they were taken from, or EscapeFaction), keep the locked shackles and are
// pushed to flee from the nearest free member until they are far enough. Knocking
// them out lets them be enslaved again, with less submission.
// ---------------------------------------------------------------------------

static std::vector<hand> g_escapees;
static float g_escapeeTimer = 0.0f;

static TaskType EscapeGoal()
{
    const std::string& g = g_cfg.escapeGoal;
    if (g == "run_away")
        return RUN_AWAY;
    if (g == "total")
        return TOTAL_ESCAPE;
    if (g == "hometown")
        return RUN_AWAY_HOMETOWN;
    return RUN_AWAY_FORCED;
}

static void StartEscape(Character* c, const char* why)
{
    SlaveRecord* found = FindRecord(c);
    if (found == 0)
        return;
    SlaveRecord r = *found;   // the map entry is moved below
    std::string name = c->getName();
    Faction* f = OriginFaction(r.origin);
    if (f == 0)
        f = OriginFaction(g_cfg.escapeFaction);
    if (f == 0)
    {
        Trace("escape: no faction for '" + name + "' (origin '" + r.origin + "', fallback '" + g_cfg.escapeFaction + "')");
        return;
    }
    std::string keyBefore = SlaveKey(c);

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
    for (size_t i = 0; i < g_escorts.size(); )
    {
        if (g_escorts[i].slave.getCharacter() == c)
            g_escorts.erase(g_escorts.begin() + i);
        else
            i++;
    }

    Platoon* platoon = f->createNewEmptyActivePlatoon(0, false, c->getPosition());
    ActivePlatoon* squad = platoon != 0 ? platoon->getActivePlatoon() : 0;
    c->setSlaveAIJob(false);
    c->changeSlaveOwner(hand());
    c->setFaction(f, squad);
    SetSlaveState(c, g_cfg.escapeMarkEscaped ? ESCAPING_SLAVE : NOT_SLAVE);
    if (g_cfg.escapeRemoveShackles)
        RemoveShackles(c);   // they got out of them to run

    // a home for the new squad, so the game's own AI has somewhere to take them
    TownBase* home = f->warMgr != 0 ? f->warMgr->getNearestTown(c->getPosition()) : 0;
    if (home != 0 && platoon != 0)
        platoon->ownerships.setHomeTown(home, SQ_ROAMING);
    Trace("escape: home town " + std::string(home != 0 ? "found" : "none") + " for faction '" + f->getName() + "'");

    r.escaped = true;
    r.escapedDay = GameDays();
    r.name = name;
    r.handle = HandleText(hand(c));
    g_records.erase(keyBefore);
    g_records[SlaveKey(c)] = r;
    g_unsupervisedDays.erase(keyBefore);
    g_escapees.push_back(hand(c));
    g_escapeeTimer = 2.0f;   // start running right away
    Trace("escape (" + std::string(why) + "): '" + name + "' -> '" + f->getName() + "' squad=" + Num(squad != 0) +
          " key " + keyBefore + (SlaveKey(c) != keyBefore ? " -> " + SlaveKey(c) : "") + " player=" + Num(IsPlayerFaction(c)) +
          " slaveState=" + Num((float)c->isSlave()) + " shackles=" + Num(WearsShackles(c)));
    ShowNotice(name + T(" ran away! ", " fugiu! ") + SubmissionText(r.submission) + ".", true);
}

// Rolls for one slave; called about once an in-game hour with the time that passed.
static void TryEscape(Character* c, SlaveRecord& r, double days, bool supervised)
{
    if (c->medical.isUnconcious() || c->inSomething == IN_PRISON || c->isChainedMode())
        return;
    if (g_cfg.escapeMaxSubmission <= 0.0f || r.submission >= g_cfg.escapeMaxSubmission)
        return;
    for (size_t i = 0; i < g_pending.size(); i++)
        if (g_pending[i].target.getCharacter() == c)
            return;
    float daily = g_cfg.escapeDailyChance * TraitsOf(c).escape * (1.0f - r.submission / g_cfg.escapeMaxSubmission);
    if (supervised)
        daily *= g_cfg.escapeSupervisedMult;
    bool shackled = WearsShackles(c);
    if (shackled)
        daily *= g_cfg.escapeShackledMult;
    if (daily <= 0.0f)
        return;
    if (daily > 1.0f)
        daily = 1.0f;
    float chance = 1.0f - (float)pow(1.0 - daily, days);
    float roll = (float)rand() / (float)RAND_MAX;
    Trace("escape roll '" + c->getName() + "': submission=" + Num(r.submission) + " supervised=" + Num(supervised) +
          " shackles=" + Num(shackled) + " daily=" + Num(daily) + " chance=" + Num(chance) + " roll=" + Num(roll));
    if (roll < chance)
        StartEscape(c, "roll");
}

// Forgets runaways that died or escaped more than EscapeForgetDays ago (after that, a
// capture counts as a new slave), so the data file does not keep growing.
static void PruneRunaways()
{
    double now = GameDays();
    for (std::map<std::string, SlaveRecord>::iterator it = g_records.begin(); it != g_records.end(); )
    {
        SlaveRecord& r = it->second;
        if (!r.escaped)
        {
            ++it;
            continue;
        }
        if (r.escapedDay < 0.0)
            r.escapedDay = now;   // escaped before the day was recorded: start counting now
        Character* c = r.handle.empty() ? 0 : HandleFromText(r.handle).getCharacter();
        const char* why = 0;
        if (c != 0 && c->isDead())
            why = "dead";
        else if (g_cfg.escapeForgetDays > 0.0f && now - r.escapedDay > g_cfg.escapeForgetDays)
            why = "escaped too long ago";
        if (why != 0)
        {
            Trace("runaway '" + r.name + "' forgotten: " + why);
            g_records.erase(it++);
        }
        else
            ++it;
    }
}

// After a load: runaways from the records go back on the list (the game does not keep
// the goals the plugin gave them).
static void RestoreRunaways()
{
    PruneRunaways();
    for (std::map<std::string, SlaveRecord>::const_iterator it = g_records.begin(); it != g_records.end(); ++it)
    {
        if (!it->second.escaped || it->second.handle.empty())
            continue;
        hand h = HandleFromText(it->second.handle);
        Character* c = h.getCharacter();
        bool ok = c != 0 && !c->isDead() && !IsPlayerFaction(c);
        Trace("runaway '" + it->second.name + "' [" + it->second.handle + "] " + (ok ? "found" : "not around"));
        if (ok)
            g_escapees.push_back(h);
    }
    g_escapeeTimer = 2.0f;
}

// Keeps runaways fleeing from the nearest free member until they are out of reach.
static void UpdateEscapees(float dt)
{
    if (g_escapees.empty())
        return;
    g_escapeeTimer += dt;
    if (g_escapeeTimer < 2.0f)
        return;
    g_escapeeTimer = 0.0f;

    for (size_t i = 0; i < g_escapees.size(); )
    {
        Character* c = g_escapees[i].getCharacter();
        if (c == 0 || c->isDead() || IsPlayerFaction(c))
        {
            g_escapees.erase(g_escapees.begin() + i);
            continue;
        }
        if (c->medical.isUnconcious())
        {
            i++;
            continue;
        }
        float dist = 0.0f;
        Character* chaser = NearestFreeMember(c->getPosition(), &dist, 0, true);
        if (chaser == 0 || dist > g_cfg.escapeSafeDistance)
        {
            Faction* f = c->getFaction();
            TownBase* home = f != 0 && f->warMgr != 0 ? f->warMgr->getNearestTown(c->getPosition()) : 0;
            if (home != 0)   // TownBase derives from RootObject first, so the pointer is the same
                c->addGoal(TRAVEL_TO_TARGET_TOWN, reinterpret_cast<RootObjectBase*>(home));
            Trace("runaway '" + c->getName() + "' got away (nearest member " + Num(dist) + "), heading " + (home != 0 ? "to a town" : "nowhere"));
            g_escapees.erase(g_escapees.begin() + i);
            continue;
        }
        c->addGoal(EscapeGoal(), chaser);
        Trace("runaway '" + c->getName() + "' flees from '" + chaser->getName() + "' dist=" + Num(dist));
        i++;
    }
}

// ---------------------------------------------------------------------------
// Hooks
// ---------------------------------------------------------------------------

static bool g_loggedSlaves = false;
static bool g_recordsReady = false;   // the load / new-game hook prepared the records for the next world

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
        // fallback in case neither the load nor the new-game hook ran for this world
        SaveManager* sm = SaveManager::getSingleton();
        if (!g_recordsReady && sm != 0)
        {
            Trace("records: no load hook for this world, using current game '" + sm->getCurrentGame() + "'");
            ReadRecords(sm->getCurrentGame());
        }
        g_recordsReady = false;
        SyncRecords();
        RestoreRunaways();
        // slaves made before shackles existed in this mod
        for (uint32_t i = 0; i < all.size(); i++)
            if (IsOurSlave(all.stuff[i]))
                GiveShackles(all.stuff[i]);
    }

    HandleHotkey();
    UpdateRebellion();
    UpdateInventoryButton();
    UpdateNotice();
    UpdateSubmissionBar();
    if (!ou->isPaused())
    {
        UpdateSubmission();
        ApplySlaveRules(time);
        UpdatePendingActions(time);
        UpdateRebels(time);
        UpdateEscapees(time);
        UpdateEscorts(time);
        CalibrateTime(time);
    }
}

// Forgets everything that points into the current world, before another one is loaded.
static void ResetWorldState(const char* why)
{
    Trace(std::string("reset world state: ") + why);
    for (size_t i = 0; i < g_pending.size(); i++)
        StopActingVisuals(g_pending[i], g_pending[i].performer.getCharacter());
    g_pending.clear();
    g_escorts.clear();
    g_rebels.clear();
    g_escapees.clear();
    g_unsupervisedDays.clear();
    g_discipline.clear();
    HideSubmissionBar();
    g_loggedSlaves = false;
    g_lastRebellionPeriod = -1.0;
    g_lastDriftDay = -1.0;
}

// The game writes a save: write the slave records next to it, under the same name.
int (*SaveManager_saveGame_orig)(SaveManager*, const std::string&, const std::string&) = NULL;
int SaveManager_saveGame_hook(SaveManager* thisptr, const std::string& location, const std::string& name)
{
    int result = SaveManager_saveGame_orig(thisptr, location, name);
    Trace("game saved: '" + name + "' result=" + Num((float)result));
    WriteRecords(name);
    g_recordsSave = name;
    return result;
}

int (*SaveManager_loadGame_orig)(SaveManager*, const std::string&, const std::string&) = NULL;
int SaveManager_loadGame_hook(SaveManager* thisptr, const std::string& location, const std::string& name)
{
    ResetWorldState("loading a save");
    ReadRecords(name);
    g_recordsReady = true;
    return SaveManager_loadGame_orig(thisptr, location, name);
}

void (*SaveManager_newGame_orig)(SaveManager*, const std::string&) = NULL;
void SaveManager_newGame_hook(SaveManager* thisptr, const std::string& startId)
{
    ResetWorldState("new game");
    g_records.clear();
    g_recordsSave = "";
    g_recordsReady = true;
    SaveManager_newGame_orig(thisptr, startId);
}

// ---------------------------------------------------------------------------
// ModConfigMenu (optional): an in-game settings page in Options > MCM.
// MCM writes SlaverMod.ini and calls ReloadSettings; without MCM nothing changes.
// ---------------------------------------------------------------------------

static void __cdecl ReloadSettings()
{
    LoadSettings();
    LoadRaces();
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

    api->section(api, L("Submission", "Submissão"));
    api->toggle(api, sec, "ShowSubmission", L("Show over the selected slave", "Mostrar sobre o escravo selecionado"),
                L("A bar with the submission over the slave you select.", "Uma barra com a submissão sobre o escravo que você seleciona."), 1, 0);
    api->integer(api, sec, "SubmissionStart", L("Starting submission", "Submissão inicial"),
                 L("Submission (0-100) of a new slave whose race has no value of its own.", "Submissão (0-100) de um escravo novo cuja raça não tem valor próprio."), 30, 0, 100, 0);
    api->integer(api, sec, "SubmissionPrisonerBonus", L("Bonus if caged or chained", "Bônus se estava preso ou acorrentado"),
                 L("Added when the slave was a prisoner when enslaved.", "Somado quando o escravo estava preso ao ser escravizado."), 10, 0, 100, 0);
    api->number(api, sec, "SubmissionDailyGain", L("Gain per day", "Ganho por dia"),
                L("Slaves slowly get used to their condition.", "Os escravos se acostumam aos poucos com a condição."), 2.0f, 0.0f, 20.0f, 1, 0);
    api->toggle(api, sec, "RaceTraits", L("Race differences", "Diferenças entre raças"),
                L("Each race and subrace starts with its own submission and gains, loses and escapes at its own pace. Table: SlaverMod_races.ini.",
                  "Cada raça e sub-raça começa com sua própria submissão e ganha, perde e foge no seu ritmo. Tabela: SlaverMod_races.ini."), 1, 0);
    api->integer(api, sec, "SubmissionGainCap", L("Limit of the daily gain", "Limite do ganho diário"),
                 L("Time alone does not raise submission above this.", "Só o tempo não faz a submissão passar disto."), 60, 0, 100, 0);

    api->section(api, L("Supervision and escape", "Supervisão e fuga"));
    api->toggle(api, sec, "SupervisionEnabled", L("Supervision", "Supervisão"),
                L("Slaves need a free, awake member nearby (or a cage). Unwatched ones lose submission.", "Escravos precisam de um membro livre e acordado por perto (ou de uma jaula). Sem vigilância, perdem submissão."), 1, 0);
    api->integer(api, sec, "SupervisionRange", L("Supervision distance", "Distância de supervisão"),
                 L("In game units; 500 is about a short walk.", "Em unidades do jogo; 500 é uma caminhada curta."), 500, 50, 5000, 0);
    api->number(api, sec, "SubmissionUnsupervisedLoss", L("Loss per day unwatched", "Perda por dia sem vigilância"), NULL, 5.0f, 0.0f, 50.0f, 1, 0);
    api->toggle(api, sec, "EscapeEnabled", L("Escapes", "Fugas"),
                L("Defiant slaves may run away, mostly when unwatched.", "Escravos rebeldes podem fugir, principalmente sem vigilância."), 1, 0);
    api->integer(api, sec, "EscapeMaxSubmission", L("Never escape from submission", "Nunca foge a partir da submissão"), NULL, 50, 0, 101, 0);
    api->number(api, sec, "EscapeDailyChance", L("Daily escape chance", "Chance de fuga por dia"),
                L("For submission 0, unwatched and without shackles (0 to 1).", "Para submissão 0, sem vigilância e sem grilhões (0 a 1)."), 0.5f, 0.0f, 1.0f, 2, 0);
    api->number(api, sec, "EscapeSupervisedMult", L("Multiplier when watched", "Multiplicador com vigilância"), NULL, 0.1f, 0.0f, 1.0f, 2, 0);
    api->number(api, sec, "EscapeShackledMult", L("Multiplier with shackles", "Multiplicador com grilhões"), NULL, 0.5f, 0.0f, 1.0f, 2, 0);
    api->integer(api, sec, "EscapeRecapturePenalty", L("Submission lost when recaptured", "Submissão perdida ao recapturar"), NULL, 10, 0, 100, 0);
    api->integer(api, sec, "EscapeForgetDays", L("Forget runaways after (days)", "Esquecer fugitivos depois de (dias)"),
                 L("After this, a captured runaway counts as a new slave. 0 = never. Dead runaways are always forgotten.",
                   "Depois disso, um fugitivo capturado conta como escravo novo. 0 = nunca. Fugitivos mortos são sempre esquecidos."), 30, 0, 365, 0);
    api->integer(api, sec, "EscapeSafeDistance", L("Runaway safe distance", "Distância segura do fugitivo"),
                 L("Runaways flee until your nearest member is this far.", "O fugitivo corre até seu membro mais próximo estar a esta distância."), 1500, 200, 10000, 0);
    api->toggle(api, sec, "EscapeRemoveShackles", L("Runaways lose the shackles", "Fugitivo perde os grilhões"),
                L("Off: they run away still wearing them.", "Desligado: ele foge ainda com eles."), 1, 0);
    api->text(api, sec, "EscapeFaction", L("Runaway faction if origin unknown (ID)", "Facção do fugitivo sem origem (ID)"), NULL, "51707-rebirth.mod", 0);

    api->section(api, L("Punishment and reward", "Punição e recompensa"));
    api->toggle(api, sec, "PunishEnabled", L("Punish", "Punir"),
                L("Punish option on your slaves: submission, but it hurts.", "Opção Punir nos seus escravos: dá submissão, mas machuca."), 1, 0);
    api->integer(api, sec, "PunishSubmission", L("Submission per punishment", "Submissão por punição"),
                 L("Halved, then a third... for each punishment within the cooldown.", "Cai pela metade, depois um terço... a cada punição dentro do intervalo."), 15, 0, 100, 0);
    api->integer(api, sec, "PunishDamage", L("Punishment damage", "Dano da punição"),
                 L("Blunt damage on a limb; grows with repeated punishments.", "Dano de impacto num membro; cresce com punições repetidas."), 10, 0, 100, 0);
    api->number(api, sec, "PunishCooldownDays", L("Punishment interval (days)", "Intervalo entre punições (dias)"), NULL, 1.0f, 0.0f, 10.0f, 1, 0);
    api->text(api, sec, "PunishVictimAnimation", L("Slave animation while punished", "Animação do escravo ao ser punido"), NULL, "kneeling hostage idle", 0);
    api->toggle(api, sec, "RewardEnabled", L("Reward", "Recompensar"),
                L("Reward option on your slaves: a little submission for some cats.", "Opção Recompensar nos seus escravos: um pouco de submissão por alguns cats."), 1, 0);
    api->integer(api, sec, "RewardSubmission", L("Submission per reward", "Submissão por recompensa"), NULL, 5, 0, 100, 0);
    api->integer(api, sec, "RewardCost", L("Reward cost (cats)", "Custo da recompensa (cats)"), NULL, 100, 0, 100000, 0);
    api->number(api, sec, "RewardCooldownDays", L("Reward interval (days)", "Intervalo entre recompensas (dias)"), NULL, 1.0f, 0.0f, 10.0f, 1, 0);
    api->integer(api, sec, "CageDailyGain", L("Cage: submission per day", "Jaula: submissão por dia"),
                 L("Extra submission for each day in a cage.", "Submissão extra por dia numa jaula."), 10, 0, 100, 0);
    api->integer(api, sec, "CageSubmissionCap", L("Cage: limit", "Jaula: limite"),
                 L("A cage does not raise submission above this.", "A jaula não faz a submissão passar disto."), 75, 0, 100, 0);

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
                 L("Strength a slave just below the submission limit needs to rise up.", "Força que um escravo logo abaixo do limite de submissão precisa para se rebelar."), 20, 0, 100, 0);
    api->integer(api, sec, "RebelMinStrengthDefiant", L("Rebel minimum strength at submission 0", "Força mínima do rebelde com submissão 0"),
                 L("The less submissive, the less strength needed: from this value at 0 up to the one above.", "Quanto menos submisso, menos força precisa: deste valor com 0 até o de cima."), 10, 0, 100, 0);
    api->integer(api, sec, "RebelMaxSubmission", L("Rebel maximum submission", "Submissão máxima do rebelde"),
                 L("Slaves with this submission or more never rebel.", "Escravos com esta submissão ou mais nunca se rebelam."), 50, 0, 101, 0);
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
    api->hotkey(api, sec, "SubmissionTestKey", L("Hotkey: +25 submission", "Atalho: +25 de submissão"),
                L("Debug mode only. Raises the selected slave's submission (wraps to 0 after 100).", "Só no modo debug. Aumenta a submissão do escravo selecionado (volta a 0 depois de 100)."), "CTRL+SHIFT+S", 0);
    api->hotkey(api, sec, "EscapeTestKey", L("Hotkey: escape now", "Atalho: fugir agora"),
                L("Debug mode only. The selected slave runs away.", "Só no modo debug. O escravo selecionado foge."), "CTRL+SHIFT+E", 0);
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
    LoadRaces();
    ParseHotkey();
    srand(GetTickCount());
    Trace("startPlugin v1.6");

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&GameWorld::_NV_mainLoop_GPUSensitiveStuff), GameWorld_update_hook, (void**)&GameWorld_update_orig))
        ErrorLog("SlaverMod: could not add GameWorld update hook!");

    FindMenuFlags();
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&ContextMenuGUI::show), ContextMenuGUI_show_hook, (void**)&ContextMenuGUI_show_orig))
        ErrorLog("SlaverMod: could not add context menu hook!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&SaveManager::saveGame), SaveManager_saveGame_hook, (void**)&SaveManager_saveGame_orig))
        ErrorLog("SlaverMod: could not add save hook!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&SaveManager::loadGame), SaveManager_loadGame_hook, (void**)&SaveManager_loadGame_orig))
        ErrorLog("SlaverMod: could not add load hook!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&SaveManager::newGame), SaveManager_newGame_hook, (void**)&SaveManager_newGame_orig))
        ErrorLog("SlaverMod: could not add new game hook!");
}
