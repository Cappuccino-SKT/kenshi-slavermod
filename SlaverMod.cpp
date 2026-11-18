// SlaverMod - lets the player's faction own slaves using Kenshi's native slave system.
// Build: tools\build_plugin.bat . SlaverMod  (VC++ 2010 x64, KenshiLib; see README)

#include <Debug.h>

#include <kenshi/gui/ForgottenGUI.h>
#include <kenshi/gui/InventoryGUI.h>
#include <kenshi/gui/NewGameWindow.h>
#include <kenshi/gui/TitleScreen.h>
#include <kenshi/Globals.h>
#include <kenshi/GlobalConstants.h>
#include <kenshi/GameWorld.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/Character.h>
#include <kenshi/MedicalSystem.h>
#include <kenshi/Platoon.h>
class GunClassPersonal;   // GunClass.h uses it before declaring it
#include <kenshi/GunClass.h>
#include <kenshi/Appearance.h>
#include <kenshi/AppearanceManager.h>
#include <kenshi/CameraClass.h>
#include <kenshi/CharMovement.h>
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
#include <kenshi/Dialogue.h>
#include <kenshi/AI/AITaskSystem.h>

#include <kenshi/Animation/AnimationClass.h>
#include <kenshi/util/hand.h>
#include <kenshi/util/lektor.h>

#include <mygui/common/baselayout/BaseLayout.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_Delegate.h>
#include <mygui/MyGUI_RenderManager.h>
#include <mygui/MyGUI_Window.h>
#include <mygui/MyGUI_MultiListBox.h>

#include <core/Functions.h>

#include "ModConfigMenu.h"

#include <map>
#include <set>
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
// Language: Language=auto (default) follows the game's language (settings.cfg). English and
// Brazilian Portuguese are built in (T(en, pt)); any other language is a text file
// lang/<code>.txt next to the DLL ("English" = "translation" lines, looked up by the English
// text), so players can translate the mod without touching the code. See docs/TRANSLATING.md.
// ---------------------------------------------------------------------------

static bool g_pt = false;
static bool g_langFile = false;                        // a lang file is in use
static std::string g_langCode = "en";                  // en, pt or the lang file in use
static std::map<std::string, std::string> g_langText;  // English -> translation
static std::map<std::string, std::string> g_langLines; // speech key -> "line|line"
static std::map<std::string, std::string> g_langInfo;  // name, squad, author
static std::map<std::string, std::string> g_langMod;   // "record id|field" -> translation of SlaverMod.mod texts

static std::string T(const char* en, const char* pt)
{
    if (g_langFile)
    {
        std::map<std::string, std::string>::const_iterator it = g_langText.find(en);
        return it != g_langText.end() ? it->second : std::string(en);   // untranslated: English
    }
    return g_pt ? pt : en;
}

// "quoted text" with \" \\ \n escapes; p is moved past the closing quote.
static bool ParseQuoted(const std::string& line, size_t& p, std::string& out)
{
    while (p < line.size() && (line[p] == ' ' || line[p] == '\t'))
        p++;
    if (p >= line.size() || line[p] != '"')
        return false;
    out.clear();
    for (p++; p < line.size(); p++)
    {
        char c = line[p];
        if (c == '"')
        {
            p++;
            return true;
        }
        if (c == '\\' && p + 1 < line.size())
        {
            char n = line[++p];
            out += n == 'n' ? '\n' : n;
        }
        else
            out += c;
    }
    return false;
}

static std::string NumStr(size_t n)
{
    std::ostringstream s;
    s << n;
    return s.str();
}

// One line of a lang file: [text] "English" = "translation"; [info]/[lines] key = "value".
static bool ParseLangLine(const std::string& section, const std::string& line, size_t p, std::string& key, std::string& value)
{
    if (section == "text" || section == "mod")
    {
        if (!ParseQuoted(line, p, key))
            return false;
    }
    else
    {
        size_t eq = line.find('=', p);
        if (eq == std::string::npos)
            return false;
        key = line.substr(p, eq - p);
        while (!key.empty() && (key[key.size() - 1] == ' ' || key[key.size() - 1] == '\t'))
            key.erase(key.size() - 1);
        p = eq;
    }
    p = line.find('=', p);
    if (p == std::string::npos)
        return false;
    p++;
    return ParseQuoted(line, p, value);
}

// Reads lang/<code>.txt (UTF-8). Empty translations are skipped (they stay in English).
static bool LoadLangFile(const std::string& code)
{
    std::ifstream f((g_modDir + "lang\\" + code + ".txt").c_str(), std::ios::binary);
    if (!f)
        return false;
    g_langText.clear();
    g_langLines.clear();
    g_langInfo.clear();
    g_langMod.clear();
    std::string line, section;
    int bad = 0;
    bool first = true;
    while (std::getline(f, line))
    {
        if (first && line.size() >= 3 && (unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF)
            line = line.substr(3);   // UTF-8 BOM
        first = false;
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        size_t p = line.find_first_not_of(" \t");
        if (p == std::string::npos || line[p] == ';' || line[p] == '#')
            continue;
        if (line[p] == '[')
        {
            size_t e = line.find(']', p);
            section = e == std::string::npos ? "" : line.substr(p + 1, e - p - 1);
            continue;
        }
        std::string key, value;
        if (!ParseLangLine(section, line, p, key, value))
        {
            bad++;
            Trace("lang: cannot read line in " + code + ".txt: " + line);
            continue;
        }
        if (value.empty())
            continue;
        if (section == "text")
            g_langText[key] = value;
        else if (section == "lines")
            g_langLines[key] = value;
        else if (section == "info")
            g_langInfo[key] = value;
        else if (section == "mod")
            g_langMod[key] = value;
    }
    Trace("lang: lang\\" + code + ".txt: " + NumStr(g_langText.size()) + " texts, " + NumStr(g_langLines.size()) +
          " speech keys, " + NumStr(g_langMod.size()) + " game data texts, " + NumStr(bad) + " unreadable line(s)");
    return true;
}

// Picks the language: a lang file for the full code (zh_cn) or its first two letters (zh),
// else the built-in Portuguese or English.
static void SelectLanguage(const std::string& wanted)
{
    g_langText.clear();
    g_langLines.clear();
    g_langInfo.clear();
    g_langMod.clear();
    g_langFile = false;
    g_pt = false;
    g_langCode = "en";
    std::string lower = wanted;
    for (size_t i = 0; i < lower.size(); i++)
        lower[i] = (char)tolower((unsigned char)lower[i]);
    std::string shortCode = lower.substr(0, 2);
    if (lower.empty() || shortCode == "en")
        return;
    if (shortCode == "pt")
    {
        // Portuguese is built in; lang/pt.txt only adds the game start texts ([mod])
        g_pt = true;
        g_langCode = "pt";
        LoadLangFile("pt");
        g_langText.clear();
        g_langLines.clear();
        return;
    }
    if (lower.find_first_of("\\/.:") == std::string::npos)
    {
        if (LoadLangFile(lower))
            g_langCode = lower;
        else if (shortCode != lower && LoadLangFile(shortCode))
            g_langCode = shortCode;
        if (g_langCode != "en")
        {
            g_langFile = true;
            return;
        }
    }
}

// The lang files present: (code, name from [info]), for the MCM language list.
static std::vector<std::pair<std::string, std::string> > ListLangFiles()
{
    std::vector<std::pair<std::string, std::string> > out;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((g_modDir + "lang\\*.txt").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE)
        return out;
    do
    {
        std::string file = fd.cFileName;
        if (file.size() < 5 || file[0] == '_')
            continue;   // _template.txt
        std::string code = file.substr(0, file.size() - 4);
        std::string name = code;
        std::ifstream f((g_modDir + "lang\\" + file).c_str(), std::ios::binary);
        std::string line, section;
        while (std::getline(f, line))
        {
            if (!line.empty() && line[line.size() - 1] == '\r')
                line.erase(line.size() - 1);
            size_t p = line.find('[');
            if (p != std::string::npos && line.find_first_not_of(" \t\xEF\xBB\xBF") == p)
            {
                section = line.substr(p + 1, line.find(']', p) - p - 1);
                if (section != "info")
                    break;
                continue;
            }
            std::string key, value;
            p = line.find_first_not_of(" \t");
            if (section == "info" && p != std::string::npos && ParseLangLine(section, line, p, key, value) && key == "name" && !value.empty())
                name = value;
        }
        out.push_back(std::make_pair(code, name));
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return out;
}

// The game's language, from settings.cfg two folders above the mod ("language=pt_BR").
static std::string GameLanguage()
{
    std::ifstream f((g_modDir + "..\\..\\settings.cfg").c_str());
    std::string line;
    while (std::getline(f, line))
        if (line.compare(0, 9, "language=") == 0)
            return line.substr(9);
    return "";
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
    bool enabled;       // 0 = the mod leaves every character alone (e.g. while your own people are enslaved)
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
    std::string shacklesComeOff;       // runaways and rebels: always | skill | never
    float shackleBreakStrength;        // skill: strength that breaks the shackles
    float shackleBreakLockpicking;     // skill: lockpicking that opens them
    std::string escapeGoal;            // forced | run_away | total | hometown
    float escapeSafeDistance;
    float escapeRecapturePenalty;
    float escapeForgetDays;            // runaways are forgotten after this many game days (0 = never)
    float freeStayMinSubmission;       // freed slaves at or above it stay in the faction; below it they go home
    std::string freedSquadTemplate;    // squad template whose AI the freed get (default: UC escaped slave, like runaways)
    std::string escapeSquadTemplate;   // squad template whose AI runaways get (default: UC escaped slave)
    std::string freedGoal;             // how the freed who leave move away: run_away | forced | total | hometown
    bool punishEnabled;
    bool skeletonThrall;               // unconscious skeletons are reprogrammed into free members (robotics roll) instead of enslaved
    float thrallBaseChance, thrallChancePerLevel, thrallMaxChance;   // % ; per robotics level
    std::string thrallRace;            // race a reprogrammed skeleton becomes (empty = keeps theirs)
    std::string soldSquadTemplate;     // squad template whose AI sold slaves get when the trader has no slaves around
    float tradeMissingLimbPenalty;     // price lost per missing or crushed limb (0.25 = -25%)
    float punishSubmission;            // first punishment; each one within PunishCooldownDays gives less
    float punishDamage;                // blunt damage on a limb; grows with repeated punishments
    float punishCooldownDays;
    std::string punishAnimation;       // performer
    std::string punishVictimAnimation; // slave
    std::string punishAnimBlade, punishAnimHeavy, punishAnimUnarmed;   // swings by weapon type ('|' = first that exists)
    std::string punishHitAnimation;    // the slave flinches at each swing (one picked at random)
    bool speech;                       // speech bubbles for slavers and slaves
    bool rewardEnabled;
    float rewardSubmission;
    int rewardCost;                    // cats
    float rewardCooldownDays;
    std::string rewardAnimation;
    float cageDailyGain;               // extra submission per day in a cage
    float cageSubmissionCap;
    bool raceTraits;                   // races and subraces take slavery differently (SlaverMod_races.ini)
    bool tradeWindow;                  // our buy / sell window at slave traders
    bool slaveCaravan;                 // the Slave Traders' caravan visits the player's base
    float slaveCaravanDays;            // days between visits
    float slaveCaravanStayHours;       // game hours it stays at the base
    float caravanSpawnDistance;        // the caravan appears this far from the base (0 = where the game puts it)
    bool caravanCamera;                // the camera goes to the caravan boss when it arrives
    bool shacklesNoRunning;            // your shackled slaves can only walk
    int caravanSlavesMin, caravanSlavesMax;              // slaves the caravan sells
    std::vector<std::string> caravanSlaveTemplates;      // character templates of those slaves
    std::string caravanFaction, caravanSquad, caravanBoss;
    float caravanArriveDistance;       // the caravan has arrived this close to the base
    float caravanLeash;                // while coming / staying, members farther than this walk back   // data IDs
    bool protectSlaves;                // other factions' slavers leave the player's slaves alone
    bool sparedByNeutrals;             // characters (and turrets) of factions not at war with the player never target our slaves
    bool clearSlaveBounties;           // new slaves lose their bounties (guards and bounty hunters stop going after them)
    bool requireShackles;              // enslaving uses up a shackle kit (or loose prisoner shackles) carried by the enslaver or a member nearby
    std::string shackleKitItem;        // ITEM string ID of the shackle kit (13-SlaverMod.mod)
    int shackleKitPrice;               // price of one kit in the slave trade window
    bool rebelsFlee;                   // rebels with nobody left to fight run away instead of standing around
    bool rebelsDistrust;               // former rebels never join the player again through dialogue
    std::string slaveFood;             // any (vanilla) | own (only food they carry) | none (never eat, the game keeps slaves from starving)
    bool cageAtNight;                  // slaves walk into a free cage at night and come out in the morning
    float cageNightHour, cageMorningHour;
    float cageSearchRange;             // how far a slave looks for a free cage
    bool allowStealingSlaves;          // enslave other factions' slaves (off: they are their owners' property)
    bool animalSlaves;                 // animals can be enslaved and sold (they never act as owners)
    bool disableCarriedSale;           // hide the fixed 400 cats offer for a carried person
    float tradeBaseValue, tradeStatValue, tradeBuyMult, tradeSellMult, tradeBoughtBonus, tradeRadius;
    std::string escapeTestKey;
    std::string carryTestKey;
    std::string knockoutTestKey;
    float relationLoss;
    float relationGain;
    bool enslaveIsCrime;               // putting shackles on someone is kidnapping for anyone who sees it
    bool slaveEscort;
    std::string escortMode;
};

static Settings g_cfg;

// Settings live in two files: SlaverMod.ini ships with the mod (defaults, replaced by every Workshop
// update) and SlaverMod_user.ini holds the player's choices (MCM writes there; the mod never ships it,
// so updates keep it). A key in the user file wins.
static std::string UserIniPath()
{
    return g_modDir + "SlaverMod_user.ini";
}

static std::string IniString(const char* key, const char* def)
{
    char buf[512] = {0};
    const char* unset = "\x01unset";
    GetPrivateProfileStringA("SlaverMod", key, unset, buf, sizeof(buf), UserIniPath().c_str());
    if (strcmp(buf, unset) != 0)
        return buf;
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
    g_cfg.language = IniString("Language", "auto");
    std::string lang = g_cfg.language == "auto" ? GameLanguage() : g_cfg.language;
    SelectLanguage(lang);
    std::string squadDefault = g_langInfo.count("squad") ? g_langInfo["squad"] : (g_pt ? "Escravos" : "Slaves");
    g_cfg.squadName = IniString("SquadName", squadDefault.c_str());
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
    g_cfg.shacklesNoRunning = IniBool("ShacklesNoRunning", true);
    g_cfg.shackleItem = IniString("ShackleItem", "SHACKLES");
    if (g_cfg.shackleItem == "21-Slave dress.mod")
        g_cfg.shackleItem = "12-SlaverMod.mod";   // v1.18: Macropod's collar is built in
    g_cfg.shackleItemAlt = IniString("ShackleItemAlt", "SHACKLES-BODY");
    g_cfg.actionAnimation = IniString("ActionAnimation", "medic");
    g_cfg.progressBar = IniBool("ProgressBar", true);
    g_cfg.enabled = IniBool("Enabled", true);
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
    g_cfg.shacklesComeOff = IniString("ShacklesComeOff", "skill");
    g_cfg.shackleBreakStrength = IniFloat("ShackleBreakStrength", 50.0f);
    g_cfg.shackleBreakLockpicking = IniFloat("ShackleBreakLockpicking", 40.0f);
    g_cfg.escapeGoal = IniString("EscapeGoal", "forced");
    g_cfg.escapeSafeDistance = IniFloat("EscapeSafeDistance", 1500.0f);
    g_cfg.escapeRecapturePenalty = IniFloat("EscapeRecapturePenalty", 10.0f);
    g_cfg.escapeForgetDays = IniFloat("EscapeForgetDays", 30.0f);
    g_cfg.freeStayMinSubmission = IniFloat("FreeStayMinSubmission", 90.0f);
    g_cfg.freedSquadTemplate = IniString("FreedSquadTemplate", "54294-rebirth.mod");
    g_cfg.escapeSquadTemplate = IniString("EscapeSquadTemplate", "54294-rebirth.mod");
    g_cfg.freedGoal = IniString("FreedGoal", "forced");
    g_cfg.punishEnabled = IniBool("PunishEnabled", true);
    g_cfg.skeletonThrall = IniBool("SkeletonThrall", false);
    g_cfg.thrallBaseChance = IniFloat("ThrallBaseChance", 20.0f);
    g_cfg.thrallChancePerLevel = IniFloat("ThrallChancePerLevel", 0.75f);
    g_cfg.thrallMaxChance = IniFloat("ThrallMaxChance", 95.0f);
    g_cfg.thrallRace = IniString("ThrallRace", "");
    g_cfg.soldSquadTemplate = IniString("SoldSquadTemplate", "55376-rebirth.mod");
    g_cfg.tradeMissingLimbPenalty = IniFloat("TradeMissingLimbPenalty", 0.25f);
    g_cfg.punishSubmission = IniFloat("PunishSubmission", 15.0f);
    g_cfg.punishDamage = IniFloat("PunishDamage", 10.0f);
    g_cfg.punishCooldownDays = IniFloat("PunishCooldownDays", 1.0f);
    g_cfg.punishAnimation = IniString("PunishAnimation", "");
    g_cfg.punishVictimAnimation = IniString("PunishVictimAnimation", "kneeling hostage idle");
    g_cfg.punishAnimBlade = IniString("PunishAnimBlade", "chop down static");
    g_cfg.punishAnimHeavy = IniString("PunishAnimHeavy", "chop down static");
    g_cfg.punishAnimUnarmed = IniString("PunishAnimUnarmed", "chop down static");
    g_cfg.punishHitAnimation = IniString("PunishHitAnimation", "");
    g_cfg.speech = IniBool("Speech", true);
    g_cfg.rewardEnabled = IniBool("RewardEnabled", true);
    g_cfg.rewardSubmission = IniFloat("RewardSubmission", 5.0f);
    g_cfg.rewardCost = (int)IniFloat("RewardCost", 100.0f);
    g_cfg.rewardCooldownDays = IniFloat("RewardCooldownDays", 1.0f);
    g_cfg.rewardAnimation = IniString("RewardAnimation", "");
    g_cfg.cageDailyGain = IniFloat("CageDailyGain", 10.0f);
    g_cfg.cageSubmissionCap = IniFloat("CageSubmissionCap", 75.0f);
    g_cfg.raceTraits = IniBool("RaceTraits", true);
    g_cfg.tradeWindow = IniBool("TradeWindow", true);
    g_cfg.slaveCaravan = IniBool("SlaveCaravan", true);
    g_cfg.slaveCaravanDays = IniFloat("SlaveCaravanDays", 7.0f);
    if (g_cfg.slaveCaravanDays < 0.5f)
        g_cfg.slaveCaravanDays = 0.5f;
    g_cfg.slaveCaravanStayHours = IniFloat("SlaveCaravanStayHours", 6.0f);
    g_cfg.caravanSpawnDistance = IniFloat("SlaveCaravanSpawnDistance", 2000.0f);
    g_cfg.caravanCamera = IniBool("SlaveCaravanCamera", true);
    g_cfg.caravanArriveDistance = IniFloat("SlaveCaravanArriveDistance", 1500.0f);
    g_cfg.caravanLeash = IniFloat("SlaveCaravanLeash", 3000.0f);
    g_cfg.caravanSlavesMin = (int)IniFloat("SlaveCaravanSlavesMin", 3.0f);
    g_cfg.caravanSlavesMax = (int)IniFloat("SlaveCaravanSlavesMax", 6.0f);
    if (g_cfg.caravanSlavesMax < g_cfg.caravanSlavesMin)
        g_cfg.caravanSlavesMax = g_cfg.caravanSlavesMin;
    {
        // the vanilla "Slave" characters of the slave squads, comma separated
        std::string list = IniString("SlaveCaravanSlaveTemplates", "5258-gamedata.base,96355-__Southern hive.mod,96357-__Southern hive.mod,96358-__Southern hive.mod,96359-__Southern hive.mod,96360-__Southern hive.mod");
        g_cfg.caravanSlaveTemplates.clear();
        size_t start = 0;
        while (start <= list.size())
        {
            size_t comma = list.find(',', start);
            std::string one = list.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
            while (!one.empty() && one[0] == ' ')
                one.erase(0, 1);
            if (!one.empty())
                g_cfg.caravanSlaveTemplates.push_back(one);
            if (comma == std::string::npos)
                break;
            start = comma + 1;
        }
    }
    g_cfg.caravanFaction = IniString("SlaveCaravanFaction", "1084-gamedata.base");
    g_cfg.caravanSquad = IniString("SlaveCaravanSquad", "6-SlaverMod.mod");
    g_cfg.caravanBoss = IniString("SlaveCaravanBoss", "95766-rebirth.mod");
    g_cfg.protectSlaves = IniBool("ProtectSlaves", true);
    g_cfg.sparedByNeutrals = IniBool("SparedByNeutrals", true);
    g_cfg.clearSlaveBounties = IniBool("ClearSlaveBounties", true);
    g_cfg.requireShackles = IniBool("RequireShackles", false);
    g_cfg.shackleKitItem = IniString("ShackleKitItem", "13-SlaverMod.mod");
    g_cfg.shackleKitPrice = (int)IniFloat("ShackleKitPrice", 150.0f);
    g_cfg.rebelsFlee = IniBool("RebelsFlee", true);
    g_cfg.rebelsDistrust = IniBool("RebelsDistrust", true);
    g_cfg.slaveFood = IniString("SlaveFood", "any");
    if (g_cfg.slaveFood != "own" && g_cfg.slaveFood != "none")
        g_cfg.slaveFood = "any";
    g_cfg.cageAtNight = IniBool("CageAtNight", false);
    g_cfg.cageNightHour = IniFloat("CageNightHour", 21.0f);
    g_cfg.cageMorningHour = IniFloat("CageMorningHour", 7.0f);
    g_cfg.cageSearchRange = IniFloat("CageSearchRange", 3000.0f);
    g_cfg.allowStealingSlaves = IniBool("AllowStealingSlaves", false);
    g_cfg.animalSlaves = IniBool("AnimalSlaves", false);
    g_cfg.disableCarriedSale = IniBool("DisableCarriedSale", true);
    g_cfg.tradeBaseValue = IniFloat("TradeBaseValue", 300.0f);
    g_cfg.tradeStatValue = IniFloat("TradeStatValue", 12.0f);
    g_cfg.tradeBuyMult = IniFloat("TradeBuyMult", 2.0f);
    g_cfg.tradeSellMult = IniFloat("TradeSellMult", 1.0f);
    g_cfg.tradeBoughtBonus = IniFloat("TradeBoughtBonus", 15.0f);
    g_cfg.tradeRadius = IniFloat("TradeRadius", 1500.0f);
    g_cfg.escapeTestKey = IniString("EscapeTestKey", "CTRL+SHIFT+E");
    g_cfg.carryTestKey = IniString("CarryTestKey", "CTRL+SHIFT+C");
    g_cfg.knockoutTestKey = IniString("KnockoutTestKey", "CTRL+SHIFT+K");
    g_cfg.relationLoss = IniFloat("RelationLoss", 5.0f);
    g_cfg.relationGain = IniFloat("RelationGain", 2.0f);
    g_cfg.enslaveIsCrime = IniBool("EnslaveIsCrime", true);
    if (g_cfg.relationGain >= g_cfg.relationLoss && g_cfg.relationLoss > 0.0f)
        g_cfg.relationGain = g_cfg.relationLoss * 0.5f;   // freeing must be worth less than enslaving costs
    g_cfg.slaveEscort = IniBool("SlaveEscort", true);
    g_cfg.escortMode = IniString("EscortMode", "bodyguard");
    if (g_cfg.escortMode == "off")
        g_cfg.slaveEscort = false;
    Trace("settings: language=" + g_cfg.language + " -> " + g_langCode + " squad='" + g_cfg.squadName + "' key=" + g_cfg.hotkey +
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
          " faction=" + g_cfg.escapeFaction + " mark=" + Num(g_cfg.escapeMarkEscaped) + " unshackle=" + g_cfg.shacklesComeOff + " goal=" + g_cfg.escapeGoal +
          " safe=" + Num(g_cfg.escapeSafeDistance) + " recapture=" + Num(g_cfg.escapeRecapturePenalty) + " forget=" + Num(g_cfg.escapeForgetDays) + " freeStay=" + Num(g_cfg.freeStayMinSubmission) + " freedAI=" + g_cfg.freedSquadTemplate + " escapeAI=" + g_cfg.escapeSquadTemplate + " soldAI=" + g_cfg.soldSquadTemplate +
          " thrallRace=" + g_cfg.thrallRace + " limbPenalty=" + Num(g_cfg.tradeMissingLimbPenalty) +
          " punish=" + Num(g_cfg.punishEnabled) + "/" + Num(g_cfg.punishSubmission) + "/" + Num(g_cfg.punishDamage) + "/" + Num(g_cfg.punishCooldownDays) +
          "d anim='" + g_cfg.punishAnimation + "'/'" + g_cfg.punishVictimAnimation + "' speech=" + Num(g_cfg.speech) + " reward=" + Num(g_cfg.rewardEnabled) + "/" + Num(g_cfg.rewardSubmission) +
          "/" + Num((float)g_cfg.rewardCost) + "cats/" + Num(g_cfg.rewardCooldownDays) + "d cage=" + Num(g_cfg.cageDailyGain) + "/" + Num(g_cfg.cageSubmissionCap) + " races=" + Num(g_cfg.raceTraits) + " trade=" + Num(g_cfg.tradeWindow) + " protect=" + Num(g_cfg.protectSlaves) + " spared=" + Num(g_cfg.sparedByNeutrals) + " bounties=" + Num(g_cfg.clearSlaveBounties) + " requireShackles=" + Num(g_cfg.requireShackles) + "/" + g_cfg.shackleKitItem + "/" + Num((float)g_cfg.shackleKitPrice) +
          " rebelsFlee=" + Num(g_cfg.rebelsFlee) + " distrust=" + Num(g_cfg.rebelsDistrust) + " food=" + g_cfg.slaveFood +
          " cageAtNight=" + Num(g_cfg.cageAtNight) + " " + Num(g_cfg.cageNightHour) + "-" + Num(g_cfg.cageMorningHour) + "h range=" + Num(g_cfg.cageSearchRange) + " steal=" + Num(g_cfg.allowStealingSlaves) + " animals=" + Num(g_cfg.animalSlaves) + " caravan=" + Num(g_cfg.slaveCaravan) + "/" + Num(g_cfg.slaveCaravanDays) + "d/stay " + Num(g_cfg.slaveCaravanStayHours) + "h " + " noCarriedSale=" + Num(g_cfg.disableCarriedSale) +
          " value=" + Num(g_cfg.tradeBaseValue) + "+" + Num(g_cfg.tradeStatValue) + "/stat buy x" + Num(g_cfg.tradeBuyMult) + " sell x" + Num(g_cfg.tradeSellMult) +
          " bought+" + Num(g_cfg.tradeBoughtBonus) + " radius=" + Num(g_cfg.tradeRadius) + " relationLoss=" + Num(g_cfg.relationLoss) + " relationGain=" + Num(g_cfg.relationGain) + " crime=" + Num(g_cfg.enslaveIsCrime) +
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

// Whose slave this is: the owner's faction, or the faction the game itself records
// (a player character enslaved by an NPC faction, e.g. the Rebirth start, has no player owner).
static Faction* SlaveOwnerFaction(Character* c)
{
    Character* owner = c->slaveOwner.getCharacter();
    if (owner != 0 && owner->getFaction() != 0)
        return owner->getFaction();
    StateBroadcastData* sb = c->getStateBroadcast();
    return sb != 0 ? sb->isSlaveOf : 0;
}

// A living, non-slave member. Being knocked out does not stop someone from owning slaves.
// Animals (pets, pack beasts) are no people: they never own, guard, supervise or enslave.
static bool IsAnimal(Character* c) { return c != 0 && c->isAnimal() != 0; }

static bool IsFreePlayerMember(Character* c)
{
    return c != 0 && !c->isDead() && IsPlayerFaction(c) && c->isSlave() != IS_SLAVE && !IsAnimal(c);
}

static int CountFreeMembers();
struct SlaveRecord;
static SlaveRecord* FindRecord(Character* c);

// Whether the player owns this slave (or runaway). A known owner decides. With none known
// (the owner is far away and not loaded, or died) only a slave the mod has a record of, while
// the player has someone free to own them: in the Rebirth start every starter is someone
// else's slave and the game does not always say whose, and those must be left to the game.
static bool OwnedByPlayer(Character* c)
{
    Faction* of = SlaveOwnerFaction(c);
    if (of != 0)
        return of == PlayerFaction();
    return FindRecord(c) != 0 && CountFreeMembers() > 0;
}

// A slave in the player's faction that the player owns. Player characters enslaved by
// someone else are left to the game.
static bool IsOurSlave(Character* c)
{
    if (!g_cfg.enabled || c == 0 || !IsPlayerFaction(c) || c->isSlave() != IS_SLAVE)
        return false;
    return OwnedByPlayer(c);
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
    if (IsAnimal(c) && !g_cfg.animalSlaves) { if (why) *why = T("animals cannot be slaves", "animais nao podem ser escravos"); return false; }
    if (IsPlayerFaction(c)) { if (why) *why = T("already in your faction", "ja e da sua faccao"); return false; }
    // someone else's slave is their property: no stealing (a runaway, ESCAPING_SLAVE, is fair game)
    if (c->isSlave() == IS_SLAVE && !g_cfg.allowStealingSlaves)
    {
        Character* owner = c->slaveOwner.getCharacter();
        if (why) *why = T("is a slave of ", "e escravo(a) de ") + (owner != 0 && owner->getFaction() != 0 ? owner->getFaction()->getName() : T("someone else", "outra pessoa"));
        return false;
    }

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
// Optional: unconscious skeletons (any robot race) get "Reprogram" instead of "Enslave".
static bool IsThrallTarget(Character* c)
{
    if (!g_cfg.skeletonThrall || c == 0 || c->isDead() || IsPlayerFaction(c) || !c->medical.isUnconcious())
        return false;
    RaceData* race = c->getRace();
    return race != 0 && race->robot;
}

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

// Slave squads are named SquadName, then "SquadName 2", "SquadName 3"... once one is full.
static bool IsSlaveSquadName(const std::string& n)
{
    const std::string& base = g_cfg.squadName;
    if (n == base)
        return true;
    if (n.size() <= base.size() + 1 || n.compare(0, base.size(), base) != 0 || n[base.size()] != ' ')
        return false;
    for (size_t i = base.size() + 1; i < n.size(); i++)
        if (n[i] < '0' || n[i] > '9')
            return false;
    return true;
}

static int MaxSquadSize() { return (con != 0 && con->MAX_SQUAD_SIZE > 0) ? con->MAX_SQUAD_SIZE : 1000000; }

// The last character put into a slave squad: right after a recruit the game's character list
// does not have them yet, so several enslaved in the same frame would each get a new squad.
static hand g_lastSlaveMember;

// Find a slave squad of the player with room in it, or create the next one.
static ActivePlatoon* GetSlaveSquad()
{
    Character* last = g_lastSlaveMember.getCharacter();
    if (last != 0 && !last->isDead() && IsPlayerFaction(last) && last->getPlatoon() != 0 &&
        IsSlaveSquadName(last->getPlatoon()->getName()) && last->getPlatoon()->getSquadSize() < MaxSquadSize())
        return last->getPlatoon();
    std::set<std::string> names;
    const lektor<Character*>& all = ou->player->getAllPlayerCharacters();
    for (uint32_t i = 0; i < all.size(); i++)
    {
        Character* c = all.stuff[i];
        if (c == 0)
            continue;
        ActivePlatoon* p = c->getPlatoon();
        if (p == 0 || !IsSlaveSquadName(p->getName()))
            continue;
        if (p->getSquadSize() < MaxSquadSize())
            return p;
        names.insert(p->getName());
    }
    std::string name = g_cfg.squadName;
    for (int n = 2; names.count(name) != 0; n++)
        name = g_cfg.squadName + " " + Num((float)n);
    ActivePlatoon* created = ou->player->createSquad();
    if (created != 0)
    {
        created->setName(name);
        Trace("created squad '" + name + "' (max squad size " + Num((float)MaxSquadSize()) + ")");
    }
    else
        Trace("createSquad failed (max squads " + Num((float)(con != 0 ? con->MAX_SQUADS : -1)) + ")");
    return created;
}

// Why the game refuses a recruit: a full faction, or no room for another squad.
static std::string RecruitRefusedReason()
{
    Faction* pf = PlayerFaction();
    if (con != 0 && pf != 0 && con->MAX_FACTION_SIZE > 0 && pf->getFactionSize() >= con->MAX_FACTION_SIZE)
        return T(" Your faction is full (", " Sua faccao esta cheia (") + Num((float)pf->getFactionSize()) + "/" +
               Num((float)con->MAX_FACTION_SIZE) + T("; see the game options).", "; veja as opcoes do jogo).");
    return "";
}

// ---------------------------------------------------------------------------
// Enslave / free
// ---------------------------------------------------------------------------

// Marks an already-recruited member of the player's faction as a slave.
// A bounty stays on a character after they are enslaved: town guards, bounty hunters and allied
// NPCs kept attacking and knocking down the slave (Bounties Galore characters all carry one).
// A slave is property, not a wanted person: their bounties and any crime in progress are cleared.
static void ClearBounties(Character* c)
{
    if (!g_cfg.clearSlaveBounties || c == 0)
        return;
    std::vector<Faction*> enforcers;
    for (ogre_unordered_map<Faction*, Bounty>::type::iterator it = c->crimes.bounties.begin(); it != c->crimes.bounties.end(); ++it)
        if (it->first != 0)
            enforcers.push_back(it->first);
    int total = c->crimes.getTotalBounty();
    for (size_t i = 0; i < enforcers.size(); i++)
        c->crimes.clearBounty(enforcers[i]);
    if (total > 0 || !enforcers.empty())
        Trace("bounty: '" + c->getName() + "' had " + Num((float)total) + " cats from " + Num((float)enforcers.size()) +
              " faction(s), now " + Num((float)c->crimes.getTotalBounty()));
}

static void MakeSlave(Character* target, Character* owner)
{
    hand ownerHand(owner);
    target->changeSlaveOwner(ownerHand);
    SetSlaveState(target, IS_SLAVE);
    ClearBounties(target);
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
    if (IsPlayerFaction(target))
        g_lastSlaveMember = hand(target);

    if (!IsPlayerFaction(target))
    {
        ShowNotice(T("The game refused to recruit ", "O jogo recusou o recrutamento de ") + name + "." + RecruitRefusedReason());
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

// Restraints on the legs or body (shackles): a collar or a mark elsewhere does not stop running.
static bool WearsLegRestraints(Character* c)
{
    if (c == 0 || c->inventory == 0)
        return false;
    lektor<Item*> armour;
    c->inventory->getEquippedArmour(armour);
    for (uint32_t i = 0; i < armour.size(); i++)
    {
        Item* it = armour.stuff[i];
        if (IsShackleItem(it) && (it->slotType == ATTACH_BOOTS || it->slotType == ATTACH_BODY || it->slotType == ATTACH_LEGS))
            return true;
    }
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
    if (!TryEquipShackles(c, g_cfg.shackleItem) && !TryEquipShackles(c, g_cfg.shackleItemAlt) &&
        (g_cfg.shackleItem == "SHACKLES" || !TryEquipShackles(c, "SHACKLES")))
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

// ---- Shackle supply (RequireShackles): enslaving uses up a shackle kit, or loose prisoner
// shackles, carried by the enslaver or by a free member close to them. Freeing gives a kit back.
static GameData* ShackleKitData()
{
    return g_cfg.shackleKitItem.empty() ? 0 : ou->gamedata.getData(g_cfg.shackleKitItem, ITEM);
}

static Item* SupplyIn(Character* c)
{
    if (c == 0 || c->inventory == 0)
        return 0;
    GameData* kit = ShackleKitData();
    Item* it = kit != 0 ? c->inventory->getItem(kit) : 0;
    if (it != 0)
        return it;
    const char* const loose[2] = { "SHACKLES", "SHACKLES-BODY" };
    lektor<Item*> worn;
    c->inventory->getEquippedArmour(worn);
    for (int k = 0; k < 2; k++)
    {
        GameData* d = ou->gamedata.getData(loose[k], ARMOUR);
        it = d != 0 ? c->inventory->getItem(d) : 0;
        bool equipped = false;
        for (uint32_t w = 0; w < worn.size(); w++)
            equipped = equipped || worn.stuff[w] == it;
        if (it != 0 && !equipped)
            return it;
    }
    return 0;
}

// Whoever carries a shackle supply: the enslaver first, then free members within supervision range.
static Character* SupplyHolder(Character* performer, Item** out)
{
    *out = SupplyIn(performer);
    if (*out != 0)
        return performer;
    const lektor<Character*>& all = ou->player->getAllPlayerCharacters();
    for (uint32_t i = 0; i < all.size(); i++)
    {
        Character* c = all.stuff[i];
        if (c == 0 || c == performer || !IsFreePlayerMember(c) || c->isDead() ||
            (c->getPosition() - performer->getPosition()).length() > g_cfg.supervisionRange)
            continue;
        *out = SupplyIn(c);
        if (*out != 0)
            return c;
    }
    return 0;
}

static void UseShackleSupply(Character* performer)
{
    if (!g_cfg.requireShackles || performer == 0)
        return;
    Item* it = 0;
    Character* holder = SupplyHolder(performer, &it);
    if (holder == 0 || it == 0)
    {
        Trace("shackle supply: none left to use for '" + performer->getName() + "'");
        return;
    }
    std::string name = it->getName();
    holder->inventory->removeItemAutoDestroy(it, 1);
    Trace("shackle supply: '" + holder->getName() + "' used one '" + name + "'");
}

static bool GiveShackleKit(Character* c, int count)
{
    GameData* kit = ShackleKitData();
    if (kit == 0 || c == 0 || c->inventory == 0)
    {
        Trace("shackle kit: item '" + g_cfg.shackleKitItem + "' not found in game data");
        return false;
    }
    for (int k = 0; k < count; k++)
    {
        Item* item = ou->theFactory->createItem(kit, hand(), 0, 0, 0, 0);
        if (item == 0)
            return false;
        c->inventory->addItem(item, 1, true, false);   // dropped at their feet if there is no room
    }
    return true;
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
static Hotkey g_carryKey = { 0, false, false, false, false };
static Hotkey g_knockoutKey = { 0, false, false, false, false };

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
    g_carryKey = ParseKey(g_cfg.carryTestKey);
    g_knockoutKey = ParseKey(g_cfg.knockoutTestKey);
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
// slave caravan visit of this save (see UpdateSlaveCaravan), saved with the records
static int g_caravanState = 0;            // CaravanState
static double g_caravanNextHour = -1.0;   // game hour of the next visit (-1 = not scheduled)
static double g_caravanLeaveHour = -1.0;  // game hour the caravan at the base leaves
static std::string g_caravanSquadID;      // Platoon::stringID of the caravan squad (kept across loads)
static std::string g_recordsSave;   // save the records in memory belong to
// Former rebels (RebelsDistrust): key -> name. They never join the player again through dialogue.
static std::map<std::string, std::string> g_distrust;
// Slaves CageAtNight put in a cage (keys): only those come out in the morning.
static std::set<std::string> g_nightCaged;
// The key a character had when last looked up: the game gives a character a new handle
// (index-serial) when they move to another squad, so the records follow them by pointer.
static std::map<Character*, std::string> g_lastKey;
static void MoveSlaveState(const std::string& from, const std::string& to);

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
    f << "!caravan=" << g_caravanState << "|" << g_caravanNextHour << "|" << g_caravanLeaveHour << "|" << g_caravanSquadID << "\n";
    for (std::map<std::string, std::string>::const_iterator it = g_distrust.begin(); it != g_distrust.end(); ++it)
        f << "!distrust=" << it->first << "|" << it->second << "\n";
    for (std::set<std::string>::const_iterator it = g_nightCaged.begin(); it != g_nightCaged.end(); ++it)
        f << "!nightcage=" << *it << "\n";
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
    g_caravanState = 0;
    g_caravanNextHour = -1.0;
    g_caravanLeaveHour = -1.0;
    g_caravanSquadID.clear();
    g_distrust.clear();
    g_nightCaged.clear();
    g_lastKey.clear();
    g_recordsSave = save;
    std::ifstream f(SaveDataFile(save).c_str());
    std::string line;
    while (std::getline(f, line))
    {
        if (line.empty() || line[0] == '#')
            continue;
        if (line.compare(0, 11, "!nightcage=") == 0)
        {
            g_nightCaged.insert(line.substr(11));
            continue;
        }
        if (line.compare(0, 10, "!distrust=") == 0)
        {
            size_t bar = line.find('|', 10);
            if (bar != std::string::npos)
                g_distrust[line.substr(10, bar - 10)] = line.substr(bar + 1);
            continue;
        }
        if (line.compare(0, 9, "!caravan=") == 0)
        {
            std::vector<std::string> parts;
            std::string rest = line.substr(9);
            size_t start = 0;
            for (int k = 0; k < 3; k++)
            {
                size_t bar = rest.find('|', start);
                parts.push_back(rest.substr(start, bar == std::string::npos ? std::string::npos : bar - start));
                start = bar == std::string::npos ? rest.size() : bar + 1;
            }
            parts.push_back(start < rest.size() ? rest.substr(start) : std::string());
            g_caravanState = atoi(parts[0].c_str());
            g_caravanNextHour = parts[1].empty() ? -1.0 : atof(parts[1].c_str());
            g_caravanLeaveHour = parts[2].empty() ? -1.0 : atof(parts[2].c_str());
            g_caravanSquadID = parts[3];
            continue;
        }
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
    std::string key = SlaveKey(c);
    std::map<std::string, SlaveRecord>::iterator it = g_records.find(key);
    if (it != g_records.end())
    {
        g_lastKey[c] = key;
        return &it->second;
    }
    // moved to another squad: same character, new key (the name guards against a reused pointer)
    std::map<Character*, std::string>::iterator last = g_lastKey.find(c);
    if (last == g_lastKey.end() || last->second == key)
        return 0;
    std::map<std::string, SlaveRecord>::iterator old = g_records.find(last->second);
    if (old == g_records.end() || old->second.name != c->getName())
        return 0;
    std::string from = last->second;
    g_records[key] = old->second;
    g_records.erase(from);
    MoveSlaveState(from, key);
    last->second = key;
    Trace("record of '" + c->getName() + "' moved with them: key " + from + " -> " + key + " (squad change)");
    return &g_records[key];
}

static float ClampSubmission(float s)
{
    return s < 0.0f ? 0.0f : (s > 100.0f ? 100.0f : s);
}

static SlaveRecord& CreateRecord(Character* c, const std::string& origin, float submission)
{
    SlaveRecord& r = g_records[SlaveKey(c)];
    g_lastKey[c] = SlaveKey(c);
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

// A squad template from the game data, used only for its AI packages (the squad stays empty).
static GameData* SquadTemplate(const std::string& sid)
{
    return sid.empty() ? 0 : ou->gamedata.getData(sid, SQUAD_TEMPLATE);
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
    float value;   // price multiplier in the slave trade
};

static std::map<std::string, RaceTraits> g_races;
static RaceTraits g_defaultRace;

static void AddRace(const char* sid, const char* name, float start, float cap, float gain, float loss, float escape, float punish)
{
    RaceTraits t;
    t.name = name;
    t.start = start; t.cap = cap; t.gain = gain; t.loss = loss; t.escape = escape; t.punish = punish;
    t.value = 1.0f;
    g_races[sid] = t;
}

static void LoadRaces()
{
    g_races.clear();
    g_defaultRace.name = "default";
    g_defaultRace.start = g_cfg.submissionStart;
    g_defaultRace.cap = -1.0f;
    g_defaultRace.gain = g_defaultRace.loss = g_defaultRace.escape = g_defaultRace.punish = g_defaultRace.value = 1.0f;

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
        const char* keys[7] = { "Start", "Cap", "Gain", "Loss", "Escape", "Punish", "Value" };
        float* values[7] = { &t.start, &t.cap, &t.gain, &t.loss, &t.escape, &t.punish, &t.value };
        for (int k = 0; k < 7; k++)
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

// ---------------------------------------------------------------------------
// Slave rules, checked every couple of game seconds
// ---------------------------------------------------------------------------

static float g_ruleTimer = 0.0f;

// The game's own rule (StateBroadcastData::periodicUpdate): a slave outside a town that is
// more than ~3500 units from its slaveOwner character, or whose owner died, turns into an
// escaped slave. Our slaves are watched by the whole faction, so the owner is handed to the
// nearest free member whenever the current one is away; escapes are decided by our own rules.
static const float OWNER_SWITCH_RANGE = 1000.0f;

static void KeepOwnerNearby(Character* c)
{
    float nearD = 0.0f;
    Character* nearest = NearestFreeMember(c->getPosition(), &nearD, c, false);
    if (nearest == 0)
        return;
    Character* owner = c->slaveOwner.getCharacter();
    float ownerD = owner != 0 && owner != c && IsFreePlayerMember(owner) ? (owner->getPosition() - c->getPosition()).length() : 1e30f;
    if (nearest == owner || ownerD <= OWNER_SWITCH_RANGE || nearD >= ownerD)
        return;
    c->changeSlaveOwner(hand(nearest));
    Trace("owner: '" + c->getName() + "' now owned by '" + nearest->getName() + "' (" + Num(nearD) + " away, previous " +
          (ownerD < 1e29f ? Num(ownerD) : std::string("gone")) + ")");
}

// Knocked out, caged, or tied up by the game. The mod's shackles also put the game in
// "chained" mode, but shackled slaves can still run (at EscapeShackledMult).
static bool IsHeld(Character* c)
{
    return c->medical.isUnconcious() || c->inSomething == IN_PRISON || (c->isChainedMode() && !WearsShackles(c));
}

// Distance at which the game gives up on the owner (sqrt of the 12,500,000 it compares).
static const float GAME_ESCAPE_RANGE = 3536.0f;
static std::map<std::string, bool> g_flipLogged;   // keeps the 2 s "stays" line from repeating

static void StartEscape(Character* c, const char* why);

// One of our slaves the game turned into an escaped slave while still in the faction.
// With no free member anywhere near, nothing holds them: they run (unless caged, chained,
// knocked out or at EscapeMaxSubmission and above). Otherwise they go back to being a slave.
static bool RestoreFlippedSlave(Character* c, bool mayRun)
{
    if (!g_cfg.enabled || c == 0 || c->isDead() || !IsPlayerFaction(c) || c->isSlave() != ESCAPING_SLAVE)
        return false;
    SlaveRecord* r = FindRecord(c);
    if (r == 0 || r->escaped || !OwnedByPlayer(c))
        return false;   // a runaway of ours, or one of the player's people escaping someone else
    float nearD = 0.0f;
    Character* nearest = NearestFreeMember(c->getPosition(), &nearD, c, false);
    bool alone = nearest == 0 || nearD > GAME_ESCAPE_RANGE;
    bool held = IsHeld(c);
    bool willing = g_cfg.escapeEnabled && g_cfg.escapeMaxSubmission > 0.0f && r->submission < g_cfg.escapeMaxSubmission;
    std::string key = SlaveKey(c);
    if (mayRun && alone && !held && willing)
    {
        Trace("owner: '" + c->getName() + "' left alone (nearest free member " + (nearest != 0 ? Num(nearD) : std::string("none")) +
              "), submission " + Num(r->submission) + ": runs away");
        g_flipLogged.erase(key);
        StartEscape(c, "left alone");
        return true;
    }
    SetSlaveState(c, IS_SLAVE);
    if (!alone || !g_flipLogged.count(key))
        Trace("owner: '" + c->getName() + "' was marked as escaped by the game, stays a slave (" +
              (!alone ? std::string("free member ") + Num(nearD) + " away" : held ? std::string("held") : !mayRun ? std::string("just loaded") : "submission " + Num(r->submission)) + ")");
    if (alone)
        g_flipLogged[key] = true;
    else
        g_flipLogged.erase(key);
    KeepOwnerNearby(c);
    return true;
}

static void ApplySlaveRules(float dt)
{
    g_ruleTimer += dt;
    if (g_ruleTimer < 2.0f)
        return;
    g_ruleTimer = 0.0f;

    const lektor<Character*>& list = ou->player->getAllPlayerCharacters();
    std::vector<Character*> all(list.stuff, list.stuff + list.size());   // copied: a runaway leaves the list
    for (size_t i = 0; i < all.size(); i++)
    {
        Character* c = all[i];
        if (RestoreFlippedSlave(c, true) && !IsPlayerFaction(c))
            continue;
        if (!IsOurSlave(c) || c->isDead())
            continue;
        KeepOwnerNearby(c);

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
        RestoreFlippedSlave(c, false);   // saved while the game had them as escaped
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
static std::vector<hand> g_leavers;   // freed slaves walking away (pushed like runaways)
static std::vector<hand> g_fleeingRebels;   // rebels with nobody left to fight, running off (RebelsFlee)
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

enum SlaveAction { ACT_ENSLAVE, ACT_FREE, ACT_PUNISH, ACT_REWARD, ACT_THRALL, ACT_CAGE, ACT_UNCAGE };

// ---------------------------------------------------------------------------
// Speech: short lines in speech bubbles (Character::say) when slaves are taken,
// punished, rewarded, freed, bought, sold, run away or rise up. Built-in lines (many
// from the game's own slavers and slaves), replaced by SlaverMod_lines.ini when it has
// the key in the [en] / [pt] section. Variants are separated by '|'.
// ---------------------------------------------------------------------------

struct Speech
{
    hand who;
    std::string text;
    DWORD at;
};

static std::vector<Speech> g_speech;
static std::map<std::string, std::vector<std::string> > g_lines;

static std::vector<std::string> SplitBar(const std::string& text)
{
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= text.size())
    {
        size_t bar = text.find('|', start);
        std::string part = text.substr(start, bar == std::string::npos ? std::string::npos : bar - start);
        if (!part.empty())
            out.push_back(part);
        if (bar == std::string::npos)
            break;
        start = bar + 1;
    }
    return out;
}

static void LoadLines()
{
    const char* table[][3] = {
        // key                 English                                                                                     Portuguese
        { "EnslaveMaster",   "You're mine now.|You'll make a good slave.|Now you work for ME.|You belong to me now.",
                             "Agora voce e meu.|Voce vai dar um bom escravo.|Agora voce trabalha pra MIM.|Voce me pertence agora." },
        { "EnslaveSlave",    "N- no! Leave me alone!|No, I won't go!|Not the shackles...|No... please...",
                             "N- nao! Me deixa em paz!|Nao, eu nao vou!|Os grilhoes nao...|Nao... por favor..." },
        { "RecaptureMaster", "Thought you could run, huh?|Back in the shackles.|You'll pay for running.",
                             "Achou que podia fugir, ne?|De volta aos grilhoes.|Voce vai pagar por ter fugido." },
        { "PunishMaster",    "Bad slave!|Back down, slave!|Disobedient slave!|You'll learn your place.|Stupid slave!",
                             "Escravo ruim!|Abaixa a cabeca, escravo!|Escravo desobediente!|Voce vai aprender o seu lugar.|Escravo estupido!" },
        { "PunishSlave",     "Ahh!|Please, stop!|I'll obey! I'll obey!|Argh!|Mercy...",
                             "Aah!|Por favor, para!|Eu obedeco! Eu obedeco!|Argh!|Piedade..." },
        { "RewardMaster",    "Here. Don't say I never gave you anything.|Good work. Take this.|Keep it up and there's more.",
                             "Toma. Nao diga que eu nunca te dei nada.|Bom trabalho. Pega isso.|Continua assim que tem mais." },
        { "RewardSlave",     "Thank you, boss...|You lead, I'll follow, boss.|...Thanks.",
                             "Obrigado, chefe...|Voce lidera, eu sigo, chefe.|...Valeu." },
        { "FreeMaster",      "Go. You're free.|Get out of here before I change my mind.|You've earned it.",
                             "Vai. Voce esta livre.|Some daqui antes que eu mude de ideia.|Voce mereceu." },
        { "FreeSlave",       "Freedom!|I never wanna see another cage again!|Guess I owe you.|I'm free...",
                             "Liberdade!|Nunca mais quero ver uma jaula!|Acho que te devo essa.|Estou livre..." },
        { "FreeSlaveStays",  "I'll stay. You lead, I'll follow, boss.|Where else would I go? I'm staying.|I'll stay with you.",
                             "Eu fico. Voce lidera, eu sigo, chefe.|Pra onde mais eu iria? Eu fico.|Vou ficar com voces." },
        { "BoughtSlave",     "Nice meetin' ya, 'boss'...|You buying my freedom? Or you buying ME?|New master, same shackles...",
                             "Prazer, 'chefe'...|Voce comprou minha liberdade? Ou comprou a MIM?|Dono novo, mesmos grilhoes..." },
        { "SoldSlave",       "No! Don't sell me!|You bastard...|Not the cages again...",
                             "Nao! Nao me vende!|Seu desgracado...|As jaulas de novo nao..." },
        { "EscapeSlave",     "I'm free!|Freedom!|Screw this! I serve no one!|Run for it!",
                             "Estou livre!|Liberdade!|Chega! Nao sirvo a ninguem!|Corre!" },
        { "EscapeMaster",    "Slave! Stop!|A slave is getting away! Stop them!|Slave! You'll be punished for this!",
                             "Escravo! Para!|Um escravo esta fugindo! Pega!|Escravo! Voce vai ser punido por isso!" },
        { "CaravanArrive",   "Slave caravan! Fresh stock, chained and ready!|Slavers here! Come see the merchandise!|Slaves for sale, and we're buying too!",
                             "Caravana de escravos! Mercadoria fresca, acorrentada e pronta!|Escravistas na area! Venham ver a mercadoria!|Escravos a venda, e a gente compra tambem!" },
        { "RebellionSlave",  "Screw this! I serve no one!|Rise up! Break the chains!|Kill the masters!|No more shackles!",
                             "Chega! Nao sirvo a ninguem!|Levantem! Quebrem as correntes!|Matem os donos!|Chega de grilhoes!" },
    };
    g_lines.clear();
    std::string file = g_modDir + "SlaverMod_lines.ini";
    // SlaverMod_lines.ini [<language>] first, then the lang file's [lines], then the built-in lines
    const char* section = g_langCode.c_str();
    int fromFile = 0;
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++)
    {
        char buf[2048] = {0};
        GetPrivateProfileStringA(section, table[i][0], "", buf, sizeof(buf), file.c_str());
        if (buf[0] != 0)
            fromFile++;
        std::map<std::string, std::string>::const_iterator lf = g_langLines.find(table[i][0]);
        std::string builtIn = lf != g_langLines.end() ? lf->second : std::string(table[i][g_pt ? 2 : 1]);
        g_lines[table[i][0]] = SplitBar(buf[0] != 0 ? std::string(buf) : builtIn);
    }
    Trace("lines: " + Num((float)g_lines.size()) + " keys, " + Num((float)fromFile) + " from SlaverMod_lines.ini [" + section + "]");
}

// Queues one line of `key` for `who`, said after delayMs (if they can still talk then).
static void Say(Character* who, const char* key, DWORD delayMs)
{
    if (!g_cfg.speech || who == 0)
        return;
    std::map<std::string, std::vector<std::string> >::const_iterator it = g_lines.find(key);
    if (it == g_lines.end() || it->second.empty())
        return;
    Speech sp;
    sp.who = hand(who);
    sp.text = it->second[rand() % it->second.size()];
    sp.at = GetTickCount() + delayMs;
    g_speech.push_back(sp);
}

static void UpdateSpeech()
{
    DWORD now = GetTickCount();
    for (size_t i = 0; i < g_speech.size(); )
    {
        if (now < g_speech[i].at)
        {
            i++;
            continue;
        }
        Character* c = g_speech[i].who.getCharacter();
        if (c != 0 && !c->isDead() && !c->medical.isUnconcious())
            c->say(g_speech[i].text);
        g_speech.erase(g_speech.begin() + i);
    }
}

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
    std::string anim;           // performer animation while working (chosen when they start)
    DWORD nextSwing;            // punishing: when to swing again (attacks do not loop)
    DWORD flinchUntil;          // punishing: the slave plays a hit reaction until then
    std::string flinch;         // that reaction
    DWORD offTaskSince;         // when the AI switched them to something of its own (0 = on task)
    int lastGoal;               // last AI goal seen, for the log
    bool playerOrdered;         // the player gave the performer another order: cancel
    bool crimeLogged;           // the kidnapping line is in the trace already
};

static std::vector<PendingAction> g_pending;
static bool g_loggedReach = false;

static std::string Lower(std::string s)
{
    for (size_t i = 0; i < s.size(); i++)
        s[i] = (char)tolower((unsigned char)s[i]);
    return s;
}

// Animations are run by the name of their ANIMATION record; the skeleton's clip is the
// record's "anim name". Both are indexed (lower case) -> record name, once.
static std::map<std::string, std::string> g_animNames;

static void IndexAnimations()
{
    if (!g_animNames.empty())
        return;
    lektor<GameData*> list;
    ou->gamedata.getDataOfType(list, ANIMATION);
    int logged = 0;
    for (uint32_t i = 0; i < list.size(); i++)
    {
        GameData* gd = list.stuff[i];
        if (gd == 0)
            continue;
        std::string clip;
        boost::unordered::unordered_map<std::string, std::string, boost::hash<std::string>, std::equal_to<std::string>,
            Ogre::STLAllocator<std::pair<std::string const, std::string>, Ogre::GeneralAllocPolicy> >::const_iterator it = gd->sdata.find("anim name");
        if (it != gd->sdata.end())
            clip = it->second;
        if (!g_animNames.count(Lower(gd->name)))
            g_animNames[Lower(gd->name)] = gd->name;
        if (!clip.empty() && !g_animNames.count(Lower(clip)))
            g_animNames[Lower(clip)] = gd->name;
        std::string both = Lower(gd->name + " " + clip);
        if (logged < 150 && (both.find("attack") != std::string::npos || both.find("chop") != std::string::npos || both.find("swing") != std::string::npos ||
                             both.find("punch") != std::string::npos || both.find("kick") != std::string::npos || both.find("blow") != std::string::npos))
        {
            Trace("animation record '" + gd->name + "' clip '" + clip + "'");
            logged++;
        }
    }
    Trace("animations indexed: " + Num((float)list.size()) + " record(s)");
}

// The first animation of a '|' list (record or clip names) that this character can play.
static std::string FirstAnimation(Character* c, const std::string& list)
{
    IndexAnimations();
    AnimationClass* anim = c != 0 ? c->getAnimationClass() : 0;
    std::vector<std::string> names = SplitBar(list);
    for (size_t i = 0; i < names.size(); i++)
    {
        std::map<std::string, std::string>::const_iterator it = g_animNames.find(Lower(names[i]));
        std::string record = it != g_animNames.end() ? it->second : names[i];
        if (anim != 0 && anim->getAnimationData(record) != 0)
            return record;
    }
    return "";
}

// Weapon type from the equipped weapon's data ("skill category"); unarmed without one.
static WeaponCategory EquippedWeaponCategory(Character* c, bool* armed)
{
    *armed = false;
    if (c->inventory == 0)
        return SKILL_UNARMED;
    lektor<Item*> weapons;
    c->inventory->getEquippedWeapons(weapons);
    for (uint32_t i = 0; i < weapons.size(); i++)
    {
        Item* w = weapons.stuff[i];
        if (w == 0 || w->data == 0)
            continue;
        *armed = true;
        boost::unordered::unordered_map<std::string, int, boost::hash<std::string>, std::equal_to<std::string>,
            Ogre::STLAllocator<std::pair<std::string const, int>, Ogre::GeneralAllocPolicy> >::const_iterator it = w->data->idata.find("skill category");
        if (it != w->data->idata.end())
            return (WeaponCategory)it->second;
    }
    return *armed ? SKILL_BLUNT : SKILL_UNARMED;
}

// Punishing swings the weapon in hand: blades, heavy/blunt weapons, or fists.
static std::string PunishAnimationFor(Character* performer)
{
    if (!g_cfg.punishAnimation.empty())
        return g_cfg.punishAnimation;   // fixed by the player
    bool armed = false;
    WeaponCategory cat = EquippedWeaponCategory(performer, &armed);
    const std::string& list = !armed || cat == SKILL_UNARMED ? g_cfg.punishAnimUnarmed
                            : (cat == SKILL_BLUNT || cat == SKILL_HEAVY || cat == ATTACK_POLEARMS) ? g_cfg.punishAnimHeavy
                            : g_cfg.punishAnimBlade;
    std::string chosen = FirstAnimation(performer, list);
    if (chosen.empty())
        chosen = FirstAnimation(performer, g_cfg.punishAnimUnarmed);
    Trace("punish animation for '" + performer->getName() + "': armed=" + Num(armed) + " weapon type=" + Num((float)cat) + " -> '" + chosen + "'");
    return chosen;
}

// Crouching first-aid animation (or a swing, when punishing) + floating progress bar while the performer works.
static std::string PerformerAnimation(SlaveAction action, Character* performer)
{
    if (action == ACT_PUNISH)
        return performer != 0 ? PunishAnimationFor(performer) : "";
    if (action == ACT_REWARD)
        return g_cfg.rewardAnimation;
    return g_cfg.actionAnimation;
}

static const char* MasterLineKey(SlaveAction action)
{
    switch (action)
    {
    case ACT_FREE: return "FreeMaster";
    case ACT_PUNISH: return "PunishMaster";
    case ACT_REWARD: return "RewardMaster";
    case ACT_THRALL: return "ThrallMaster";
    default: return "EnslaveMaster";
    }
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
    case ACT_THRALL: return T("Reprogramming", "Reprogramando");
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
    p.anim = PerformerAnimation(p.action, performer);
    p.nextSwing = 0;
    p.flinchUntil = 0;
    Say(performer, MasterLineKey(p.action), 0);
    if (p.action == ACT_ENSLAVE)
        Say(p.target.getCharacter(), "EnslaveSlave", 1600);
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
    if (p.action != ACT_PUNISH)
        RunAnimation(performer, p.anim);
    else if (GetTickCount() >= p.nextSwing)
    {
        StopAnimation(performer, p.anim);
        RunAnimation(performer, p.anim);   // one swing, then again a moment later
        p.nextSwing = GetTickCount() + 1300;
        // the slave flinches from the blow
        Character* slave = p.target.getCharacter();
        std::vector<std::string> hits = SplitBar(g_cfg.punishHitAnimation);
        if (slave != 0 && !hits.empty())
        {
            StopAnimation(slave, p.flinch);
            p.flinch = FirstAnimation(slave, hits[rand() % hits.size()]);
            if (!p.flinch.empty())
            {
                StopAnimation(slave, TargetAnimation(p.action));
                RunAnimation(slave, p.flinch);
                p.flinchUntil = GetTickCount() + 600;
            }
        }
    }
    if (GetTickCount() >= p.flinchUntil)
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
    StopAnimation(performer, p.anim);
    StopAnimation(p.target.getCharacter(), TargetAnimation(p.action));
    StopAnimation(p.target.getCharacter(), p.flinch);
    if (p.bar != 0 && gui != 0)
        gui->destroy(p.bar);
    p.bar = 0;
}

// Small relation hit with the faction of someone the player enslaves.
static void ApplyRelationLoss(Faction* victimFaction, const std::string& victimName, bool reprogrammed = false)
{
    Faction* pf = PlayerFaction();
    if (g_cfg.relationLoss <= 0.0f || victimFaction == 0 || pf == 0 || victimFaction == pf || victimFaction->relations == 0)
        return;
    float before = victimFaction->relations->getFactionRelation(pf);
    victimFaction->relations->affectRelations(pf, -g_cfg.relationLoss, 1.0f);
    float after = victimFaction->relations->getFactionRelation(pf);
    Trace("relation with '" + victimFaction->getName() + "': " + Num(before) + " -> " + Num(after));
    if (after < before)
        ShowNotice(victimFaction->getName() + T(" did not like seeing ", " nao gostou de ver ") + victimName + (reprogrammed ? T(" reprogrammed. Relation: ", " reprogramado(a). Relacao: ") : T(" enslaved. Relation: ", " escravizado(a). Relacao: ")) + Num((float)(int)after) + ".");
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

// While putting shackles on someone of another faction the performer is committing a kidnapping,
// like carrying an unconscious body away: whoever sees it reacts as the game decides (alarm,
// guards, the victim's friends attacking). Renewed every frame of the work, so it lasts as long.
static void CommitEnslaveCrime(PendingAction& p, Character* performer, Character* target)
{
    if (!g_cfg.enslaveIsCrime || (p.action != ACT_ENSLAVE && p.action != ACT_THRALL) || IsPlayerFaction(target))
        return;
    SlaveRecord* r = FindRecord(target);
    if (r != 0 && r->escaped)   // a runaway is still yours
        return;
    Faction* victim = target->getFaction();
    if (victim == 0 || victim == PlayerFaction())
        return;
    // the game refuses it for some factions (drifters...): nobody minds those being taken
    performer->crimes.setCrime(CRIME_KIDNAPPING, victim, hand(target));
    if (!p.crimeLogged)
        Trace("crime: '" + performer->getName() + "' kidnapping '" + target->getName() + "' of '" + victim->getName() +
              "' -> committing=" + Num((float)performer->crimes.committingCrime));
    p.crimeLogged = true;
}

static bool g_ownOrder = false;   // set while the mod itself gives an order (see Character_addOrder_hook)

static void OrderMove(Character* who, const Ogre::Vector3& where)
{
    g_ownOrder = true;
    who->addOrder(0, MOVE_CUS_ORDERED, 0, false, true, where);
    g_ownOrder = false;
}

// Someone outside the player's faction is carrying them (a guard taking them to prison...).
static bool CarriedByOthers(Character* c)
{
    if (!c->isBeingCarried())
        return false;
    const lektor<Character*>& all = ou->player->getAllPlayerCharacters();
    for (uint32_t i = 0; i < all.size(); i++)
    {
        Character* m = all.stuff[i];
        if (m != 0 && m->isCarryingSomething && m->getCarryingObject().getCharacter() == c)
            return false;
    }
    return true;
}

// The AI's current goal (task system at AI + 0x20).
static AITaskSytem* TaskSystemOf(Character* c)
{
    if (c == 0 || c->ai == 0)
        return 0;
    return *reinterpret_cast<AITaskSytem**>(reinterpret_cast<char*>(c->ai) + 0x20);
}

static int CurrentGoal(Character* c)
{
    AITaskSytem* ts = TaskSystemOf(c);
    if (ts == 0)
        return -1;
    const TaskMatch& g = ts->getCurrentGoal();
    return g ? (int)g.key() : (int)NULL_TASK;
}

// The order beats anything the AI picks on its own (first aid, jobs, looting, fighting...):
// when their goal is not ours for a moment, the order is given again. A new order from the
// player cancels the action instead. Returns false when cancelled.
static bool KeepOnTask(PendingAction& p, Character* performer, Character* target)
{
    AITaskSytem* ts = TaskSystemOf(performer);
    if (ts == 0)
        return true;
    int goal = CurrentGoal(performer);
    if (goal != p.lastGoal)
    {
        Trace("goal: '" + performer->getName() + "' " + Num((float)p.lastGoal) + " -> " + Num((float)goal) +
              (p.acting < 0.0f ? " (walking)" : " (working)"));
        p.lastGoal = goal;
    }
    if (p.playerOrdered || (ts->hasPlayerOrders() && !ts->hasPlayerOrder(MOVE_CUS_ORDERED)))
    {
        Trace("action cancelled: '" + performer->getName() + "' got another order from the player");
        ShowNotice(T("Action cancelled: ", "Acao cancelada: ") + performer->getName() + T(" got another order.", " recebeu outra ordem."));
        return false;
    }
    // under attack (the victim's friends, guards who saw the kidnapping): let them fight
    if (goal == SELF_PRESERVATION || goal == MELEE_ATTACK || goal == FOCUSED_MELEE_ATTACK || goal == UNPROVOKED_FOCUSED_MELEE_ATTACK ||
        goal == ATTACK_ENEMIES || goal == ATTACK_ENEMIES_AND_NEUTRALS || performer->isInCombatMode(true, true))
    {
        Trace("action cancelled: '" + performer->getName() + "' is fighting (goal " + Num((float)goal) + ")");
        ShowNotice(T("Action cancelled: ", "Acao cancelada: ") + performer->getName() + T(" is under attack.", " esta sendo atacado(a)."));
        return false;
    }
    bool walking = p.acting < 0.0f;
    bool ours = walking ? goal == MOVE_CUS_ORDERED
                        : (goal == NULL_TASK || goal == IDLE || goal == MOVE_CUS_ORDERED || goal == HOLD_POSITION || goal == STAND_STILL);
    if (ours)
    {
        p.offTaskSince = 0;
        return true;
    }
    DWORD now = GetTickCount();
    if (p.offTaskSince == 0)
        p.offTaskSince = now;
    else if (now - p.offTaskSince > 700)
    {
        Trace("keep: '" + performer->getName() + "' was on goal " + Num((float)goal) + ", order given again");
        p.offTaskSince = 0;
        if (walking)
        {
            p.orderedPos = target->getPosition();
            OrderMove(performer, p.orderedPos);
        }
        else
            OrderMove(performer, performer->getPosition());
    }
    return true;
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
    if (action == ACT_ENSLAVE && g_cfg.requireShackles)
    {
        Item* supply = 0;
        if (SupplyHolder(performer, &supply) == 0)
        {
            *why = T("no shackle kit: buy one from a slave trader", "sem kit de grilhoes: compre um de um traficante de escravos");
            return false;
        }
    }
    if (CarriedByOthers(target)) { *why = target->getName() + T(" is being carried away by someone else", " esta sendo carregado(a) por outra pessoa"); return false; }
    if (IsAnimal(target) && !g_cfg.animalSlaves) { *why = T("animals cannot be slaves", "animais nao podem ser escravos"); return false; }
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

// Per-slave memory keyed like the records: moved along when a slave changes squads.
static void MoveSlaveState(const std::string& from, const std::string& to)
{
    std::map<std::string, DisciplineState>::iterator d = g_discipline.find(from);
    if (d != g_discipline.end())
    {
        g_discipline[to] = d->second;
        g_discipline.erase(from);
    }
    std::map<std::string, float>::iterator u = g_unsupervisedDays.find(from);
    if (u != g_unsupervisedDays.end())
    {
        g_unsupervisedDays[to] = u->second;
        g_unsupervisedDays.erase(from);
    }
}

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

// A freed slave below FreeStayMinSubmission goes home: back to the faction they were
// taken from (or EscapeFaction when unknown), in a squad of their own heading to its
// nearest town. Returns that faction, or 0 when they stay.
static Faction* SendFreedHome(Character* c, const std::string& originSid)
{
    Faction* f = OriginFaction(originSid);
    if (f == 0)
        f = OriginFaction(g_cfg.escapeFaction);
    if (f == 0 || f == PlayerFaction())
        return 0;
    for (size_t i = 0; i < g_escorts.size(); )
    {
        if (g_escorts[i].slave.getCharacter() == c)
            g_escorts.erase(g_escorts.begin() + i);
        else
            i++;
    }
    // the squad template gives the new squad the game's own wandering AI
    GameData* ai = SquadTemplate(g_cfg.freedSquadTemplate);
    Platoon* platoon = f->createNewEmptyActivePlatoon(ai, false, c->getPosition());
    ActivePlatoon* squad = platoon != 0 ? platoon->getActivePlatoon() : 0;
    c->setFaction(f, squad);
    TownBase* home = f->warMgr != 0 ? f->warMgr->getNearestTown(c->getPosition()) : 0;
    if (home != 0 && platoon != 0)
        platoon->ownerships.setHomeTown(home, SQ_ROAMING);
    if (home != 0)   // TownBase derives from RootObject first, so the pointer is the same
        c->addGoal(TRAVEL_TO_TARGET_TOWN, reinterpret_cast<RootObjectBase*>(home));
    g_leavers.push_back(hand(c));
    Trace("freed '" + c->getName() + "' goes home to '" + f->getName() + "' (town " + (home != 0 ? "found" : "none") +
          ", AI template " + (ai != 0 ? g_cfg.freedSquadTemplate : std::string("none")) + ") player=" + Num(IsPlayerFaction(c)));
    return IsPlayerFaction(c) ? 0 : f;
}

// ---- Skeleton thrall (optional): an unconscious skeleton (any robot race) is not enslaved
// but reprogrammed. A roll on the performer's robotics: success = a free member of your
// faction, in the performer's squad; failure = the skeleton is destroyed.
static float StatOf(Character* c, StatsEnumerated s);

// setRace changes the race data only: the character keeps their appearance data (body mesh, head),
// so a new appearance is made from the race (a headless race has no head to give) and the body rebuilt.
static bool ApplyRace(Character* c, GameData* race)
{
    // only skeleton -> skeleton: giving a skeleton body to a human (or animal) crashes the game
    RaceData* from = c->getRace();
    if (from == 0 || !from->robot)
    {
        Trace("race change skipped: '" + c->getName() + "' (" + RaceID(c) + ") is not a skeleton");
        return false;
    }
    c->setRace(race);
    AppearanceManager* am = AppearanceManager::getInstance();
    GameDataCopyStandalone* look = am != 0 ? am->createAppearanceData(race) : 0;
    if (look != 0)
        c->setAppearanceData(look);
    AppearanceBase* body = c->getAppearance();
    if (body != 0)
        body->reload();
    // the health parts (head, chest...) are built from the race only when the save loads
    // (MedicalSystem::notifyRaceChange did not rebuild them): the player is told to save and load
    Trace("race of '" + c->getName() + "' -> " + RaceID(c) + " (new appearance=" + Num(look != 0) + ", reloaded=" + Num(body != 0) + ")");
    return true;
}

static float ThrallChance(Character* performer)
{
    float chance = g_cfg.thrallBaseChance + g_cfg.thrallChancePerLevel * StatOf(performer, STAT_ROBOTICS);
    return chance < 0.0f ? 0.0f : (chance > g_cfg.thrallMaxChance ? g_cfg.thrallMaxChance : chance);
}

// Every fatal part (head, chest) far below zero: the game kills them on its next medical update.
static void DestroySkeleton(Character* c)
{
    MedicalSystem& med = c->medical;
    int parts = med.getPartCount();
    for (int i = 0; i < parts; i++)
    {
        MedicalSystem::HealthPartStatus* part = med.getPart((unsigned __int64)i);
        if (part != 0 && part->fatal)
            part->flesh = -part->maxHealth() * 3.0f;
    }
}

static void Thrall(Character* performer, Character* target)
{
    std::string name = target->getName();
    Faction* victimFaction = target->getFaction();
    float chance = ThrallChance(performer);
    float roll = (float)(rand() % 10000) / 100.0f;
    Trace("thrall: '" + performer->getName() + "' robotics=" + Num(StatOf(performer, STAT_ROBOTICS)) + " chance=" + Num(chance) +
          "% roll=" + Num(roll) + " on '" + name + "' (" + RaceID(target) + ")");
    if (roll >= chance)
    {
        DestroySkeleton(target);
        Trace("thrall: failed, '" + name + "' destroyed");
        ShowNotice(performer->getName() + T(" failed to reprogram ", " falhou ao reprogramar ") + name + T(" and destroyed them (", " e o(a) destruiu (") +
                   Num((float)(int)(chance + 0.5f)) + T("% chance).", "% de chance)."));
        return;
    }
    ActivePlatoon* squad = performer->getPlatoon();
    Platoon* previous = ou->player->getCurrentPlatoon();
    if (squad != 0 && squad->me != 0)
        ou->player->setCurrentPlatoon(squad->me);
    if (target->isChainedMode())
        target->setChainedMode(false, hand(performer));
    Character* arr[1] = { target };
    lektor<Character*> list;
    list.stuff = arr;
    list.count = 1;
    list.maxSize = 1;
    ou->player->recruit(list, false);
    list.stuff = nullptr;
    list.count = 0;
    list.maxSize = 0;
    if (previous != 0)
        ou->player->setCurrentPlatoon(previous);
    if (!IsPlayerFaction(target))
    {
        Trace("thrall: recruit refused");
        ShowNotice(T("The game refused to recruit ", "O jogo recusou o recrutamento de ") + name + "." + RecruitRefusedReason());
        return;
    }
    Trace("thrall: '" + name + "' joined the squad of '" + performer->getName() + "'");
    GameData* race = g_cfg.thrallRace.empty() ? 0 : ou->gamedata.getData(g_cfg.thrallRace, RACE);
    if (race != 0)
    {
        ApplyRace(target, race);
    }
    else if (!g_cfg.thrallRace.empty())
        Trace("thrall: ThrallRace '" + g_cfg.thrallRace + "' not found, race kept");
    ShowNotice(name + T(" was reprogrammed and now serves your faction (", " foi reprogramado(a) e agora serve a sua faccao (") +
               Num((float)(int)(chance + 0.5f)) + T("% chance).", "% de chance).") +
               (race != 0 ? T(" Save and load for the new body to update.", " Salve e carregue para o corpo novo atualizar.") : std::string("")));
    ApplyRelationLoss(victimFaction, name, true);
}

static void CompleteAction(SlaveAction action, Character* performer, Character* target)
{
    if (action == ACT_THRALL)
    {
        Thrall(performer, target);
        return;
    }
    if (action == ACT_PUNISH)
    {
        Punish(performer, target);
        Say(target, "PunishSlave", 500);
        return;
    }
    if (action == ACT_REWARD)
    {
        Reward(performer, target);
        Say(target, "RewardSlave", 700);
        return;
    }
    if (action == ACT_FREE)
    {
        SlaveRecord* record = FindRecord(target);
        std::string originSid = record != 0 ? record->origin : LegacyOrigin(target);
        Faction* origin = OriginFaction(originSid);
        float submission = SubmissionOf(target);
        std::string name = target->getName();
        RemoveShackles(target);
        FreeSlave(target);
        if (!IsOurSlave(target))
        {
            if (g_cfg.requireShackles && GiveShackleKit(performer, 1))
                Trace("shackle kit: '" + performer->getName() + "' got the kit back from '" + name + "'");
            ForgetRecord(target);
            Faction* gone = submission < g_cfg.freeStayMinSubmission ? SendFreedHome(target, originSid) : 0;
            if (gone != 0)
                ShowNotice(name + T(" was freed and goes back to ", " foi libertado(a) e volta para ") + gone->getName() + ".");
            else
                ShowNotice(name + T(" was freed and stays in your faction (", " foi libertado(a) e fica na sua faccao (") + SubmissionLevel(submission) + ").");
            Say(target, gone != 0 ? "FreeSlave" : "FreeSlaveStays", 700);
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
            UseShackleSupply(performer);
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
        {
            ShowNotice(name + T(" was recaptured. ", " foi recapturado(a). ") + SubmissionText(r.submission) + ".");
            Say(performer, "RecaptureMaster", 300);
        }
        else
            ShowNotice(name + T(" is now a slave. ", " agora e escravo(a). ") + SubmissionText(r.submission) + ".");
        UseShackleSupply(performer);
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
    case ACT_THRALL: return T(" will try to reprogram ", " vai tentar reprogramar ");
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
    case ACT_THRALL: return T(" is reprogramming ", " esta reprogramando ");
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
        PendingAction now;
        now.action = action;
        now.crimeLogged = false;
        CommitEnslaveCrime(now, performer, target);
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
    p.offTaskSince = 0;
    p.lastGoal = -1;
    p.playerOrdered = false;
    p.crimeLogged = false;
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
    StartAction(target, IsOurSlave(target) ? ACT_FREE : (IsThrallTarget(target) ? ACT_THRALL : ACT_ENSLAVE));
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
            if (performer != 0 && !performer->isDead())
                OrderMove(performer, performer->getPosition());   // stop walking after the target
            g_pending.erase(g_pending.begin() + i);
            continue;
        }
        if (!KeepOnTask(p, performer, target))
        {
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
        CommitEnslaveCrime(p, performer, target);
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

// ---- Carry test (debug): someone else knocks out and carries away the target of a slave
// action (or the selected character), like a guard taking them to prison.
static hand g_carryTarget, g_carrier;
static DWORD g_carryPickupAt = 0;

static Character* NearestOutsider(Character* target)
{
    Character* best = 0;
    float bestDist = 3000.0f;
    const lektor<Faction*>& factions = ou->factionMgr->participants;
    for (uint32_t i = 0; i < factions.size(); i++)
    {
        Faction* f = factions.stuff[i];
        if (f == 0 || f == PlayerFaction())
            continue;
        lektor<RootObject*> found;
        f->getCharactersInArea(found, target->getPosition(), bestDist, false);
        for (uint32_t k = 0; k < found.size(); k++)
        {
            Character* c = found.stuff[k] != 0 ? hand(found.stuff[k]).getCharacter() : 0;
            if (c == 0 || c == target || c->isDead() || c->isUnconcious() || IsPlayerFaction(c) ||
                c->isSlave() == IS_SLAVE || c->isCarryingSomething)
                continue;
            float d = (c->getPosition() - target->getPosition()).length();
            if (d < bestDist)
            {
                bestDist = d;
                best = c;
            }
        }
    }
    return best;
}

static void StartCarryTest()
{
    Character* target = !g_pending.empty() ? g_pending[0].target.getCharacter() : 0;
    if (target == 0)
        target = SelectedOtherCharacter();
    if (target == 0 || IsPlayerFaction(target) || target->isDead())
    {
        ShowNotice(T("Carry test: start enslaving someone (or select an NPC) first.", "Teste de carregar: comece a escravizar alguem (ou selecione um NPC) antes."));
        return;
    }
    Character* carrier = NearestOutsider(target);
    if (carrier == 0)
    {
        ShowNotice(T("Carry test: nobody from another faction nearby.", "Teste de carregar: ninguem de outra faccao por perto."));
        return;
    }
    if (!target->isUnconcious())
        target->medical.knockoutForceTimer(120.0f);
    g_carryTarget = hand(target);
    g_carrier = hand(carrier);
    g_carryPickupAt = GetTickCount() + 1500;   // let them fall first
    Trace("carry test: '" + carrier->getName() + "' (" + (carrier->getFaction() != 0 ? carrier->getFaction()->getName() : std::string("?")) +
          ") will carry '" + target->getName() + "'");
    ShowNotice(T("Carry test: ", "Teste de carregar: ") + carrier->getName() + T(" will carry ", " vai carregar ") + target->getName() + ".");
}

static void UpdateCarryTest()
{
    if (g_carryPickupAt == 0 || GetTickCount() < g_carryPickupAt)
        return;
    g_carryPickupAt = 0;
    Character* target = g_carryTarget.getCharacter();
    Character* carrier = g_carrier.getCharacter();
    if (target == 0 || carrier == 0 || carrier->isDead())
        return;
    carrier->pickupObject(target);
    // walk away from whoever was coming for them
    Character* chaser = !g_pending.empty() ? g_pending[0].performer.getCharacter() : 0;
    if (chaser == 0)
        chaser = NearestFreeMember(target->getPosition(), 0, 0, false);
    Ogre::Vector3 from = chaser != 0 ? chaser->getPosition() : target->getPosition();
    Ogre::Vector3 away = carrier->getPosition() - from;
    away.y = 0.0f;
    if (away.length() < 1.0f)
        away = Ogre::Vector3(1.0f, 0.0f, 0.0f);
    away.normalise();
    carrier->addOrder(0, MOVE_CUS_ORDERED, 0, false, true, carrier->getPosition() + away * 1500.0f);
    Trace("carry test: picked up, carrying=" + Num(carrier->isCarryingSomething) + " beingCarried=" + Num(target->isBeingCarried()) +
          " byOthers=" + Num(CarriedByOthers(target)));
}

// ---- Test kit (DebugMode only), fixed hotkeys: CTRL+SHIFT+ I info, M money, N enslave nearby,
// L cut a limb off, H ThrallRace on the selected character, V slave caravan now.
static int MissingLimbs(Character* c);
static int SellPrice(Character* c);
static void GivePlayerMoney(int amount);
static Hotkey g_tkInfo = { 'I', true, true, false, false };
static Hotkey g_tkMoney = { 'M', true, true, false, false };
static Hotkey g_tkNearby = { 'N', true, true, false, false };
static Hotkey g_tkLimb = { 'L', true, true, false, false };
static Hotkey g_tkRace = { 'H', true, true, false, false };
static Hotkey g_tkCaravan = { 'V', true, true, false, false };
static void TestCaravanNow();

static void TestInfo()
{
    Faction* pf = PlayerFaction();
    std::string msg = "squad max " + Num((float)(con != 0 ? con->MAX_SQUAD_SIZE : -1)) +
                      ", squads max " + Num((float)(con != 0 ? con->MAX_SQUADS : -1)) +
                      ", faction " + Num((float)(pf != 0 ? pf->getFactionSize() : -1)) + "/" +
                      Num((float)(con != 0 ? con->MAX_FACTION_SIZE : -1));
    std::set<ActivePlatoon*> squads;
    const lektor<Character*>& all = ou->player->getAllPlayerCharacters();
    for (uint32_t i = 0; i < all.size(); i++)
        if (all.stuff[i] != 0 && all.stuff[i]->getPlatoon() != 0)
            squads.insert(all.stuff[i]->getPlatoon());
    for (std::set<ActivePlatoon*>::iterator it = squads.begin(); it != squads.end(); ++it)
        msg += " | " + (*it)->getName() + ": " + Num((float)(*it)->getSquadSize());
    Character* c = SelectedOtherCharacter();
    if (c != 0)
        msg += " || " + c->getName() + " (" + RaceID(c) + "): missing limbs " + Num((float)MissingLimbs(c)) +
               ", sell price " + Num((float)SellPrice(c)) + ", slave " + Num((float)c->isSlave());
    Trace("test info: " + msg);
    ShowNotice(msg);
}

// Knocks out and enslaves up to 10 NPCs near the selected character.
static void TestEnslaveNearby()
{
    Character* me = ou->player->selectedCharacter.getCharacter();
    if (me == 0 || !IsPlayerFaction(me))
    {
        ShowNotice(T("Test: select one of your characters first.", "Teste: selecione um personagem seu antes."));
        return;
    }
    std::vector<Character*> picked;
    const lektor<Faction*>& factions = ou->factionMgr->participants;
    for (uint32_t i = 0; i < factions.size() && picked.size() < 10; i++)
    {
        Faction* f = factions.stuff[i];
        if (f == 0 || f == PlayerFaction())
            continue;
        lektor<RootObject*> found;
        f->getCharactersInArea(found, me->getPosition(), 2000.0f, false);
        for (uint32_t k = 0; k < found.size() && picked.size() < 10; k++)
        {
            Character* c = found.stuff[k] != 0 ? hand(found.stuff[k]).getCharacter() : 0;
            if (c == 0 || c->isDead() || IsPlayerFaction(c) || (c->isAnimal() != 0 && !g_cfg.animalSlaves) || c->isSlave() == IS_SLAVE)
                continue;
            picked.push_back(c);
        }
    }
    // the game takes a moment to count a knocked out character as unconscious: those are
    // knocked out now and enslaved on the next press
    int done = 0, knocked = 0;
    for (size_t i = 0; i < picked.size(); i++)
    {
        Character* c = picked[i];
        if (IsEnslaveCandidate(c, 0))
        {
            CompleteAction(ACT_ENSLAVE, me, c);   // same as a finished enslave action (record, shackles, escort)
            if (IsPlayerFaction(c))
                done++;
        }
        else
        {
            c->medical.knockoutForceTimer(120.0f);
            knocked++;
        }
    }
    Trace("test: enslave nearby, " + Num((float)done) + " enslaved, " + Num((float)knocked) + " knocked out");
    ShowNotice(T("Test: enslaved ", "Teste: escravizou ") + Num((float)done) + T(", knocked out ", ", nocauteou ") + Num((float)knocked) +
               (knocked > 0 ? T(" (press again to enslave them).", " (aperte de novo para escraviza-los).") : std::string(".")));
}

static void TestCutLimb()
{
    Character* c = SelectedOtherCharacter();
    if (c == 0 || c->isDead())
    {
        ShowNotice(T("Test: select a character first.", "Teste: selecione um personagem antes."));
        return;
    }
    for (int l = RobotLimbs::LEFT_ARM; l <= RobotLimbs::RIGHT_LEG; l++)
    {
        LimbState st = c->medical.getLimbState((RobotLimbs::Limb)l);
        if (st == LIMB_STUMP || st == LIMB_CRUSHED)
            continue;
        int before = SellPrice(c);
        c->medical.amputate((RobotLimbs::Limb)l, false, Ogre::Vector3(0, 0, 0));
        Trace("test: limb " + Num((float)l) + " of '" + c->getName() + "' cut, price " + Num((float)before) + " -> " + Num((float)SellPrice(c)));
        ShowNotice(c->getName() + T(": limb cut, sell price ", ": membro cortado, preco de venda ") + Num((float)before) + " -> " +
                   Num((float)SellPrice(c)));
        return;
    }
    ShowNotice(c->getName() + T(" has no limb left to cut.", " nao tem mais membro para cortar."));
}

static void TestThrallRace()
{
    Character* c = SelectedOtherCharacter();
    GameData* race = g_cfg.thrallRace.empty() ? 0 : ou->gamedata.getData(g_cfg.thrallRace, RACE);
    if (c == 0 || race == 0)
    {
        Trace("test: race key, selected=" + Num(c != 0) + " ThrallRace='" + g_cfg.thrallRace + "' found=" + Num(race != 0));
        ShowNotice(T("Test: set ThrallRace and select a character first.", "Teste: configure ThrallRace e selecione um personagem antes."));
        return;
    }
    std::string before = RaceID(c);
    if (!ApplyRace(c, race))
    {
        ShowNotice(T("Test: only skeletons can change race.", "Teste: so esqueletos podem trocar de raca."));
        return;
    }
    Trace("test: race of '" + c->getName() + "' " + before + " -> " + RaceID(c));
    ShowNotice(c->getName() + ": " + before + " -> " + RaceID(c) + T(" (save and load for the body parts).", " (salve e carregue para as partes do corpo)."));
}

static void HandleTestKit()
{
    if (!g_cfg.debugMode)
        return;
    if (KeyPressed(g_tkInfo))
        TestInfo();
    if (KeyPressed(g_tkMoney))
    {
        GivePlayerMoney(20000);
        Trace("test: +20000 cats");
        ShowNotice(T("Test: +20000 cats.", "Teste: +20000 cats."));
    }
    if (KeyPressed(g_tkNearby))
        TestEnslaveNearby();
    if (KeyPressed(g_tkLimb))
        TestCutLimb();
    if (KeyPressed(g_tkRace))
        TestThrallRace();
    if (KeyPressed(g_tkCaravan))
        TestCaravanNow();
}

static void HandleHotkey()
{
    HandleTestKit();
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
    // test key: the selected character falls unconscious (2 minutes)
    if (KeyPressed(g_knockoutKey) && g_cfg.debugMode)
    {
        Character* c = SelectedOtherCharacter();
        if (c == 0 || c->isDead())
            ShowNotice(T("Knockout test: select a character first.", "Teste de nocaute: selecione um personagem antes."));
        else
        {
            c->medical.knockoutForceTimer(120.0f);
            Trace("knockout test: '" + c->getName() + "' (" + RaceID(c) + ") knocked out for 120s");
            ShowNotice(T("Knockout test: ", "Teste de nocaute: ") + c->getName() + T(" is unconscious.", " esta inconsciente."));
        }
    }
    // test key: someone else carries the target away
    if (KeyPressed(g_carryKey) && g_cfg.debugMode)
        StartCarryTest();
    UpdateCarryTest();
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
// ---------------------------------------------------------------------------
// Cages: "Lock in a cage" sends one of our slaves into the nearest free cage of the player's
// faction; "Let out of the cage" frees them from it. CageAtNight does it for every slave at
// CageNightHour and lets the same ones out at CageMorningHour.
// ---------------------------------------------------------------------------

// Only getOccupant is used (UseableStuff.h pulls Building.h, which clashes with Platoon.h).
class UseableStuff
{
public:
    const hand& getOccupant() const;// public RVA = 0x28C440
};

struct CageTrip
{
    hand slave, cage;
    float elapsed, sinceCloser, bestDist, reorder;
    bool night;
};
static std::vector<CageTrip> g_caging;
static float g_cagingTimer = 0.0f;

static bool IsCaged(Character* c)
{
    return c != 0 && c->inSomething == IN_PRISON;
}

static bool CageTaken(Building* b)
{
    for (size_t i = 0; i < g_caging.size(); i++)
        if (g_caging[i].cage.getBuilding() == b)
            return true;
    return false;
}

// Nearest cage of the player's faction with nobody in it (or on the way to it).
static Building* FindFreeCage(Character* c)
{
    Faction* pf = PlayerFaction();
    lektor<RootObject*> found;
    ou->getObjectsWithinSphere(found, c->getPosition(), g_cfg.cageSearchRange, BUILDING, 400, 0);
    Building* best = 0;
    float bestDist = 0.0f;
    int cages = 0;
    for (uint32_t i = 0; i < found.size(); i++)
    {
        RootObject* o = found.stuff[i];
        Building* b = o != 0 ? hand(o).getBuilding() : 0;
        if (b == 0)
            continue;
        int function = *reinterpret_cast<int*>(reinterpret_cast<char*>(b) + 0x158);   // Building::specialFunction
        if (function != BF_CAGE)
            continue;
        cages++;
        if (o->getFaction() != pf || reinterpret_cast<UseableStuff*>(b)->getOccupant().getCharacter() != 0 || CageTaken(b))
            continue;
        float d = (o->getPosition() - c->getPosition()).length();
        if (best == 0 || d < bestDist)
        {
            best = b;
            bestDist = d;
        }
    }
    Trace("cage: " + Num((float)found.size()) + " building(s), " + Num((float)cages) + " cage(s) near '" + c->getName() + "', free one " +
          (best != 0 ? "at " + Num(bestDist) : std::string("none")));
    return best;
}

static bool SendToCage(Character* c, bool night)
{
    if (c == 0 || IsCaged(c) || c->isUnconcious() || c->isBeingCarried())
        return false;
    for (size_t i = 0; i < g_caging.size(); i++)
        if (g_caging[i].slave.getCharacter() == c)
            return true;
    Building* cage = FindFreeCage(c);
    if (cage == 0)
    {
        if (!night)
            ShowNotice(T("No free cage of yours near ", "Nenhuma jaula livre sua perto de ") + c->getName() + ".");
        return false;
    }
    CageTrip t;
    t.slave = hand(c);
    t.cage = hand(reinterpret_cast<RootObject*>(cage));
    t.elapsed = 0.0f;
    t.sinceCloser = 0.0f;
    t.bestDist = (reinterpret_cast<RootObject*>(cage)->getPosition() - c->getPosition()).length();
    t.reorder = 0.0f;
    t.night = night;
    g_caging.push_back(t);
    OrderMove(c, reinterpret_cast<RootObject*>(cage)->getPosition());
    if (!night)
        ShowNotice(c->getName() + T(" goes to a cage.", " vai para uma jaula."));
    return true;
}

static void LetOutOfCage(Character* c)
{
    if (!IsCaged(c))
        return;
    UseableStuff* cage = reinterpret_cast<UseableStuff*>(c->inWhat.getBuilding());
    c->setPrisonMode(false, cage);
    g_nightCaged.erase(SlaveKey(c));
    Trace("cage: '" + c->getName() + "' let out (still in prison=" + Num(IsCaged(c)) + ")");
}

static void UpdateCaging(float dt)
{
    if (g_caging.empty())
        return;
    g_cagingTimer += dt;
    if (g_cagingTimer < 0.5f)
        return;
    float step = g_cagingTimer;
    g_cagingTimer = 0.0f;
    for (size_t i = 0; i < g_caging.size(); )
    {
        CageTrip& t = g_caging[i];
        Character* c = t.slave.getCharacter();
        Building* b = t.cage.getBuilding();
        RootObject* cageObj = reinterpret_cast<RootObject*>(b);
        const char* drop = 0;
        if (c == 0 || c->isDead() || !IsOurSlave(c))
            drop = "no longer a slave of yours";
        else if (b == 0)
            drop = "the cage is gone";
        else if (c->isUnconcious() || c->isBeingCarried())
            drop = "knocked out or carried";
        else if (IsCaged(c))
            drop = "already caged";
        else if (reinterpret_cast<UseableStuff*>(b)->getOccupant().getCharacter() != 0)
            drop = "someone else is in the cage";
        t.elapsed += step;
        if (drop == 0 && t.elapsed > 90.0f)
            drop = "took too long";
        if (drop != 0)
        {
            Trace(std::string("cage: trip of '") + (c != 0 ? c->getName() : std::string("?")) + "' dropped: " + drop);
            g_caging.erase(g_caging.begin() + i);
            continue;
        }
        float d = (cageObj->getPosition() - c->getPosition()).length();
        if (d < t.bestDist - 5.0f)
        {
            t.bestDist = d;
            t.sinceCloser = 0.0f;
        }
        else
            t.sinceCloser += step;
        // close enough, or as close as the cage walls let them get
        if (d < 40.0f || (d < 150.0f && t.sinceCloser > 6.0f))
        {
            c->setPrisonMode(true, reinterpret_cast<UseableStuff*>(b));
            bool in = IsCaged(c);
            if (in && t.night)
                g_nightCaged.insert(SlaveKey(c));
            Trace("cage: '" + c->getName() + "' enters the cage at " + Num(d) + " -> in prison=" + Num(in));
            g_caging.erase(g_caging.begin() + i);
            continue;
        }
        t.reorder += step;
        if (t.reorder > 5.0f)
        {
            t.reorder = 0.0f;
            OrderMove(c, cageObj->getPosition());
        }
        i++;
    }
}

static int g_wasNight = -1;   // -1 = not known yet (first frame / after a load)

static bool IsCageNight()
{
    double h = fmod(ou->getTimeStamp_inGameHours().getTotalHours(), 24.0);
    float n = g_cfg.cageNightHour, m = g_cfg.cageMorningHour;
    return n > m ? (h >= n || h < m) : (h >= n && h < m);
}

static void UpdateCageNight()
{
    if (!g_cfg.cageAtNight)
    {
        g_wasNight = -1;
        return;
    }
    int night = IsCageNight() ? 1 : 0;
    if (night == g_wasNight)
        return;
    bool first = g_wasNight < 0;
    g_wasNight = night;
    const lektor<Character*>& all = ou->player->getAllPlayerCharacters();
    int count = 0;
    for (uint32_t i = 0; i < all.size(); i++)
    {
        Character* c = all.stuff[i];
        if (!IsOurSlave(c))
            continue;
        if (night && !first)
        {
            bool busy = false;
            for (size_t k = 0; k < g_pending.size(); k++)
                busy = busy || g_pending[k].target.getCharacter() == c;
            if (!busy && SendToCage(c, true))
                count++;
        }
        else if (!night && IsCaged(c) && g_nightCaged.count(SlaveKey(c)))
        {
            LetOutOfCage(c);
            count++;
        }
    }
    if (!night)
        g_nightCaged.clear();
    Trace("cage night: " + std::string(night ? "night" : "morning") + (first ? " (first look)" : "") + ", " + Num((float)count) + " slave(s)");
    if (count > 0)
        ShowNotice(night ? Num((float)count) + T(" slave(s) go to their cages for the night.", " escravo(s) vao para as jaulas passar a noite.")
                         : Num((float)count) + T(" slave(s) let out of the cages for the day.", " escravo(s) soltos das jaulas para o dia."));
}

static std::vector<SlaveAction> MenuActionsFor(Character* target)
{
    std::vector<SlaveAction> out;
    if (target == 0 || target->isDead() || (IsAnimal(target) && !g_cfg.animalSlaves) || IsAnimal(SelectedPlayerCharacter()))
        return out;
    if (IsOurSlave(target))
    {
        out.push_back(ACT_FREE);
        if (g_cfg.punishEnabled)
            out.push_back(ACT_PUNISH);
        if (g_cfg.rewardEnabled)
            out.push_back(ACT_REWARD);
        out.push_back(IsCaged(target) ? ACT_UNCAGE : ACT_CAGE);
        return out;
    }
    if (IsPlayerFaction(target))
    {
        if (target != SelectedPlayerCharacter() && CountFreeMembers() > 1)
            out.push_back(ACT_ENSLAVE);
        return out;
    }
    if (IsEnslaveCandidate(target, 0))
        out.push_back(IsThrallTarget(target) ? ACT_THRALL : ACT_ENSLAVE);
    return out;
}

static void OnMenuOption(MyGUI::Widget* sender, int left, int top, MyGUI::MouseButton id)
{
    Character* target = g_menuTarget.getCharacter();
    if (target != 0 && sender != 0)
    {
        SlaveAction action = (SlaveAction)atoi(sender->getUserString("SlaverModAction").c_str());
        Trace("menu option " + Num((float)action) + " on '" + target->getName() + "'");
        if (action == ACT_CAGE)
            SendToCage(target, false);
        else if (action == ACT_UNCAGE)
        {
            LetOutOfCage(target);
            ShowNotice(target->getName() + T(" was let out of the cage.", " foi solto(a) da jaula."));
        }
        else
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
    case ACT_THRALL: return upper ? T("REPROGRAM", "REPROGRAMAR") : T("Reprogram", "Reprogramar");
    case ACT_CAGE: return upper ? T("LOCK IN A CAGE", "PRENDER NA JAULA") : T("Lock in a cage", "Prender na jaula");
    case ACT_UNCAGE: return upper ? T("LET OUT OF THE CAGE", "SOLTAR DA JAULA") : T("Let out of the cage", "Soltar da jaula");
    default: return upper ? T("ENSLAVE", "ESCRAVIZAR") : T("Enslave", "Escravizar");
    }
}

// Appends our rows to the menu the game just built, copying the geometry of its last row.
static void AddSlaveOption(ContextMenuGUI* menu)
{
    if (!g_cfg.enabled || !g_cfg.contextMenu || ou == 0 || ou->player == 0 || menu == 0)
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
        // not at war (RebellionHostile=0) or nobody left to fight: the uprising becomes a flight
        if (target == 0 || dist > g_cfg.rebelChaseRange || !g_cfg.rebellionHostile)
        {
            Trace("rebel '" + c->getName() + "' has nobody to fight (dist " + Num(dist) + ", hostile " + Num(g_cfg.rebellionHostile) + "), " +
                  (g_cfg.rebelsFlee ? "fleeing" : "leaving"));
            if (g_cfg.rebelsFlee)
                g_fleeingRebels.push_back(g_rebels[i]);
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

// Runaways and rebels get out of their shackles (ShacklesComeOff): always, never, or only the strong
// enough to break them or the skilled enough to pick the lock (skill).
static bool GetsOutOfShackles(Character* c)
{
    if (g_cfg.shacklesComeOff == "always")
        return true;
    if (g_cfg.shacklesComeOff == "never")
        return false;
    CharStats* stats = c->getStats();
    if (stats == 0)
        return false;
    float str = stats->getStat(STAT_STRENGTH, true);
    float lock = stats->getStat(STAT_LOCKPICKING, true);
    bool out = str >= g_cfg.shackleBreakStrength || lock >= g_cfg.shackleBreakLockpicking;
    Trace("shackles: '" + c->getName() + "' str=" + Num(str) + " lockpicking=" + Num(lock) + (out ? " gets out" : " stays shackled"));
    return out;
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
    // a home for the rebels' squad, so fleeing rebels have a town to head to
    TownBase* home = rebelFaction->warMgr != 0 ? rebelFaction->warMgr->getNearestTown(rebels[0]->getPosition()) : 0;
    if (home != 0 && platoon != 0)
        platoon->ownerships.setHomeTown(home, SQ_ROAMING);
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
        if (GetsOutOfShackles(c))
            RemoveShackles(c);
        if (c->isChainedMode())
            c->setChainedMode(false, hand());
        std::string keyBefore = SlaveKey(c);
        c->setFaction(rebelFaction, squad);
        ForgetRecord(c);
        g_records.erase(keyBefore);
        if (g_cfg.rebelsDistrust)
            g_distrust[SlaveKey(c)] = c->getName();   // never joins you again through dialogue
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
    for (size_t r = 0; r < rebels.size() && r < 3; r++)
        Say(rebels[r], "RebellionSlave", (DWORD)(r * 700));
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

static TaskType GoalFromName(const std::string& g)
{
    if (g == "run_away")
        return RUN_AWAY;
    if (g == "total")
        return TOTAL_ESCAPE;
    if (g == "hometown")
        return RUN_AWAY_HOMETOWN;
    return RUN_AWAY_FORCED;
}

static TaskType EscapeGoal()
{
    return GoalFromName(g_cfg.escapeGoal);
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

    GameData* ai = SquadTemplate(g_cfg.escapeSquadTemplate);   // the game's escaped-slave AI
    Platoon* platoon = f->createNewEmptyActivePlatoon(ai, false, c->getPosition());
    ActivePlatoon* squad = platoon != 0 ? platoon->getActivePlatoon() : 0;
    c->setSlaveAIJob(false);
    c->changeSlaveOwner(hand());
    c->setFaction(f, squad);
    SetSlaveState(c, g_cfg.escapeMarkEscaped ? ESCAPING_SLAVE : NOT_SLAVE);
    Trace("escape: AI template " + (ai != 0 ? g_cfg.escapeSquadTemplate : std::string("none")));
    if (GetsOutOfShackles(c))
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
    Say(c, "EscapeSlave", 0);
    float guardDist = 0.0f;
    Character* guard = NearestFreeMember(c->getPosition(), &guardDist, c, true);
    if (guard != 0 && guardDist <= g_cfg.supervisionRange * 2.0f)
        Say(guard, "EscapeMaster", 1200);
}

// Rolls for one slave; called about once an in-game hour with the time that passed.
static void TryEscape(Character* c, SlaveRecord& r, double days, bool supervised)
{
    if (IsHeld(c))
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
// Pushes everyone in `list` away from the nearest free member until they are out of
// reach, then sends them to their faction's nearest town.
static void PushAway(std::vector<hand>& list, TaskType goal, const char* label)
{
    for (size_t i = 0; i < list.size(); )
    {
        Character* c = list[i].getCharacter();
        if (c == 0 || c->isDead() || IsPlayerFaction(c))
        {
            list.erase(list.begin() + i);
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
            Trace(std::string(label) + " '" + c->getName() + "' got away (nearest member " + Num(dist) + "), heading " + (home != 0 ? "to a town" : "nowhere"));
            list.erase(list.begin() + i);
            continue;
        }
        c->addGoal(goal, chaser);
        Trace(std::string(label) + " '" + c->getName() + "' moves away from '" + chaser->getName() + "' dist=" + Num(dist));
        i++;
    }
}

static void UpdateEscapees(float dt)
{
    if (g_escapees.empty() && g_leavers.empty() && g_fleeingRebels.empty())
        return;
    g_escapeeTimer += dt;
    if (g_escapeeTimer < 2.0f)
        return;
    g_escapeeTimer = 0.0f;
    PushAway(g_escapees, EscapeGoal(), "runaway");
    PushAway(g_leavers, GoalFromName(g_cfg.freedGoal), "freed");
    PushAway(g_fleeingRebels, EscapeGoal(), "fleeing rebel");
}

// ---------------------------------------------------------------------------
// Hooks
// ---------------------------------------------------------------------------

static bool g_loggedSlaves = false;
static void WatchMembers(float dt, bool reset);
static void UpdateTradeWindow();
static void CloseTradeWindow();
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
// [mod] of the lang file: SlaverMod.mod texts (the game start's name and description, character
// names) are swapped in the loaded game data, "record id|field" -> text. Checked every 2 s, as the
// game can load its data again (new game, load).
static DWORD g_nextModTextCheck = 0;
static bool g_modTextsLogged = false;

static void ApplyModTexts(bool now = false)
{
    if (g_langMod.empty() || ou == 0 || (!now && GetTickCount() < g_nextModTextCheck))
        return;
    g_nextModTextCheck = GetTickCount() + 2000;
    int applied = 0, missing = 0;
    for (std::map<std::string, std::string>::const_iterator it = g_langMod.begin(); it != g_langMod.end(); ++it)
    {
        size_t bar = it->first.find('|');
        if (bar == std::string::npos)
            continue;
        GameData* gd = ou->gamedata.getData(it->first.substr(0, bar));
        if (gd == 0)
        {
            missing++;
            continue;
        }
        std::string field = it->first.substr(bar + 1);
        if (field == "name")
        {
            if (gd->name != it->second)
            {
                gd->name = it->second;
                applied++;
            }
            continue;
        }
        // only fields the record already has: the map's nodes stay the game's
        boost::unordered::unordered_map<std::string, std::string, boost::hash<std::string>, std::equal_to<std::string>,
            Ogre::STLAllocator<std::pair<std::string const, std::string>, Ogre::GeneralAllocPolicy> >::iterator f = gd->sdata.find(field);
        if (f != gd->sdata.end() && f->second != it->second)
        {
            f->second = it->second;
            applied++;
        }
    }
    if (applied > 0 || !g_modTextsLogged)
    {
        g_modTextsLogged = true;
        Trace("lang: game data texts, " + NumStr(applied) + " applied, " + NumStr(missing) + " record(s) not loaded");
    }
}

// The new game window reads the starts' names and descriptions when it opens, in the main menu,
// where the world update does not run yet: the texts are swapped right before.
void (*NewGameWindow_setVisible_orig)(NewGameWindow*, bool) = NULL;
void NewGameWindow_setVisible_hook(NewGameWindow* thisptr, bool visible)
{
    if (visible)
        ApplyModTexts(true);
    NewGameWindow_setVisible_orig(thisptr, visible);
}

// Other mods replace the new game window (Second Life of Kenshi's QuickStartSelect): the title
// screen's update runs every frame in the main menu, so the texts are in place before any of them.
void (*TitleScreen_update_orig)(TitleScreen*) = NULL;
void TitleScreen_update_hook(TitleScreen* thisptr)
{
    ApplyModTexts();
    TitleScreen_update_orig(thisptr);
}

// ---------------------------------------------------------------------------
// Slave caravan: every SlaveCaravanDays days the Slave Traders send a caravan to the player's
// base (the first town of the player's faction) to buy and sell slaves. SlaverMod runs it
// itself: it creates the squad (SlaverMod.mod 6-SlaverMod.mod) SlaveCaravanSpawnDistance from
// the base and walks it in; its AI (7-SlaverMod.mod) has no goal that walks, so it then waits
// there. After SlaveCaravanStayHours (default 6) it is walked off towards the Slave Traders'
// town and removed once far enough.
// Talking to the caravan boss ("I'm looking to buy slaves") opens SlaverMod's trade window.
// The visit state is saved with the slave records of each save.
// ---------------------------------------------------------------------------

enum CaravanState { CARAVAN_NONE, CARAVAN_COMING, CARAVAN_HERE, CARAVAN_LEAVING };
static DWORD g_nextCaravanCheck = 0;
static bool g_caravanNoBaseLogged = false;
static bool g_caravanSoonNoticed = false;
static hand g_caravanSquad;               // the caravan's platoon (found again after a load)
static Ogre::Vector3 g_caravanLastPos(0.0f, 0.0f, 0.0f);   // where its boss was last seen
static double g_caravanStateHour = -1.0;  // game hour the current state began

// TownBase derives from RootObject first; Town.h cannot be included (Building.h clash).
static RootObject* TownObject(TownBase* t) { return reinterpret_cast<RootObject*>(t); }
static double GameHours() { return ou->getTimeStamp_inGameHours().getTotalHours(); }

static Faction* SlaveTradersFaction()
{
    return ou->factionMgr != 0 ? ou->factionMgr->getFactionByStringID(g_cfg.caravanFaction) : 0;
}

// The player's base the caravan visits: the first town of the player's faction.
static TownBase* PlayerBase()
{
    Faction* pf = PlayerFaction();
    if (pf == 0 || pf->warMgr == 0 || pf->warMgr->myTowns.size() == 0)
        return 0;
    return pf->warMgr->myTowns.stuff[0];
}

static void ScheduleNextCaravan(const std::string& why)
{
    g_caravanState = CARAVAN_NONE;
    g_caravanNextHour = GameHours() + g_cfg.slaveCaravanDays * 24.0;
    g_caravanSoonNoticed = false;
    g_caravanSquad = hand();
    g_caravanSquadID.clear();
    Trace("caravan: next visit in " + Num(g_cfg.slaveCaravanDays) + " days (" + why + ")");
}

// The caravan squad: kept by handle, found again after a load by its string ID.
static Platoon* CaravanPlatoon(Faction* traders, TownBase* base)
{
    Platoon* p = g_caravanSquad.getPlatoon();
    if (p != 0)
        return p;
    if (g_caravanSquadID.empty() || base == 0)
        return 0;
    lektor<RootObject*> found;
    Ogre::Vector3 around = g_caravanLastPos.isZeroLength() ? TownObject(base)->getPosition() : g_caravanLastPos;
    traders->getCharactersInArea(found, around, g_cfg.caravanSpawnDistance + 8000.0f, false);
    for (uint32_t i = 0; i < found.size(); i++)
    {
        Character* c = found.stuff[i] != 0 ? hand(found.stuff[i]).getCharacter() : 0;
        Platoon* cp = c != 0 && c->getPlatoon() != 0 ? c->getPlatoon()->me : 0;
        if (cp != 0 && cp->stringID == g_caravanSquadID)
        {
            g_caravanSquad = hand(cp);
            return cp;
        }
    }
    return 0;
}

// The caravan boss: the squad leader (or its first standing member). 0 while the squad is
// not loaded (far from the player's characters).
static Character* CaravanBoss(Platoon* p)
{
    ActivePlatoon* ap = p != 0 ? p->getActivePlatoon() : 0;
    if (ap == 0)
        return 0;
    if (ap->squadleader != 0 && !ap->squadleader->isDead())
        return ap->squadleader;
    lektor<RootObject*>* things = ap->getThings();
    for (uint32_t i = 0; things != 0 && i < things->size(); i++)
    {
        Character* c = things->stuff[i] != 0 ? hand(things->stuff[i]).getCharacter() : 0;
        if (c != 0 && !c->isDead() && c->isSlave() != IS_SLAVE)
            return c;
    }
    return 0;
}

static bool g_caravanSlavesDone = false;   // once per visit
static void FindSlavesForSale(Character* trader, std::vector<Character*>& out);

// The caravan brings SlaveCaravanSlavesMin..Max shackled slaves, owned by the boss and walking
// in his squad (made here when the squad came without them).
static void EnsureCaravanSlaves(Character* boss)
{
    if (g_caravanSlavesDone || boss == 0)
        return;
    g_caravanSlavesDone = true;
    std::vector<Character*> have;
    FindSlavesForSale(boss, have);
    if ((int)have.size() >= g_cfg.caravanSlavesMin || g_cfg.caravanSlaveTemplates.empty())
        return;
    int span = g_cfg.caravanSlavesMax - g_cfg.caravanSlavesMin;
    int want = g_cfg.caravanSlavesMin + (span > 0 ? rand() % (span + 1) : 0) - (int)have.size();
    ActivePlatoon* squad = boss->getPlatoon();
    int made = 0;
    for (int i = 0; i < want; i++)
    {
        const std::string& sid = g_cfg.caravanSlaveTemplates[rand() % g_cfg.caravanSlaveTemplates.size()];
        GameData* tpl = ou->gamedata.getData(sid, CHARACTER);
        if (tpl == 0)
            continue;
        Ogre::Vector3 at = boss->getPosition() + Ogre::Vector3((float)(rand() % 300 - 150), 0.0f, (float)(rand() % 300 - 150));
        RootObject* obj = ou->theFactory->createRandomCharacter(boss->getFaction(), at, squad, tpl, 0, -1.0f);
        Character* c = obj != 0 ? hand(obj).getCharacter() : 0;
        if (c == 0)
            continue;
        SetSlaveState(c, IS_SLAVE);
        c->changeSlaveOwner(hand(boss));
        GiveShackles(c);
        made++;
    }
    Trace("caravan: " + Num((float)have.size()) + " slave(s) with the caravan, " + Num((float)made) + " added");
}

// Creates the caravan SlaveCaravanSpawnDistance from the base, on the Slave Traders' side.
static void SpawnCaravan(Faction* traders, TownBase* base)
{
    GameData* squadData = ou->gamedata.getData(g_cfg.caravanSquad, SQUAD_TEMPLATE);
    if (squadData == 0)
    {
        Trace("caravan: squad template '" + g_cfg.caravanSquad + "' not found (SlaverMod.mod)");
        ScheduleNextCaravan("no squad data");
        return;
    }
    Ogre::Vector3 at = TownObject(base)->getPosition();
    TownBase* home = traders->warMgr != 0 ? traders->warMgr->getNearestTown(at) : 0;
    Ogre::Vector3 dir = home != 0 ? TownObject(home)->getPosition() - at : Ogre::Vector3(1.0f, 0.0f, 0.0f);
    dir.y = 0.0f;
    if (dir.length() < 1.0f)
        dir = Ogre::Vector3(1.0f, 0.0f, 0.0f);
    dir.normalise();
    Ogre::Vector3 spawn = at + dir * g_cfg.caravanSpawnDistance;
    Platoon* p = ou->theFactory->createRandomSquad(traders, spawn, base, 30, 0, squadData, 0, 0, 0, false, hand(), base, 1.0f, SQ_ROAMING, false);
    if (p == 0)
    {
        Trace("caravan: the game did not create the squad");
        ScheduleNextCaravan("squad not created");
        return;
    }
    g_caravanSquad = hand(p);
    g_caravanSquadID = p->stringID;
    g_caravanState = CARAVAN_COMING;
    g_caravanStateHour = GameHours();
    g_caravanSlavesDone = false;
    Trace("caravan: created " + Num(g_cfg.caravanSpawnDistance) + " units from '" + TownObject(base)->getName() + "' (towards '" +
          (home != 0 ? TownObject(home)->getName() : std::string("?")) + "'), loaded=" + Num(p->getActivePlatoon() != 0));
    ShowNotice(T("A slave caravan of the Slave Traders is coming to ", "Uma caravana de escravos dos Traficantes esta vindo para ") +
               TownObject(base)->getName() + ".");
}

static void CaravanArrived(Character* boss, TownBase* base)
{
    g_caravanState = CARAVAN_HERE;
    g_caravanStateHour = GameHours();
    g_caravanLeaveHour = GameHours() + g_cfg.slaveCaravanStayHours;
    EnsureCaravanSlaves(boss);
    Trace("caravan: arrived at '" + TownObject(base)->getName() + "', stays " + Num(g_cfg.slaveCaravanStayHours) + " h");
    ShowNotice(T("The slave caravan arrived. Talk to ", "A caravana de escravos chegou. Fale com ") + boss->getName() +
               T(" to buy or sell slaves.", " para comprar ou vender escravos."));
    Say(boss, "CaravanArrive", 0);
    if (g_cfg.caravanCamera && ou->player->getCamera() != 0)
        ou->player->getCamera()->focusCameraOnObject(boss, Ogre::Vector3(0.0f, 0.0f, 0.0f), false);
}

static Ogre::Vector3 g_caravanAwayPos(0.0f, 0.0f, 0.0f);   // where it walks when leaving
static DWORD g_caravanLastLeaveOrder = 0;

// Walks every standing member of the caravan to `where` (its AI only follows the leader).
static void OrderCaravan(Platoon* squad, const Ogre::Vector3& where)
{
    ActivePlatoon* ap = squad != 0 ? squad->getActivePlatoon() : 0;
    lektor<RootObject*>* things = ap != 0 ? ap->getThings() : 0;
    for (uint32_t i = 0; things != 0 && i < things->size(); i++)
    {
        Character* c = things->stuff[i] != 0 ? hand(things->stuff[i]).getCharacter() : 0;
        if (c != 0 && !c->isDead() && !c->medical.isUnconcious())
            OrderMove(c, where);
    }
    g_caravanLastLeaveOrder = GetTickCount();
}

// The stay is over: the caravan walks off towards the Slave Traders' town and is removed
// once far enough (see UpdateSlaveCaravan).
static void CaravanLeaves(Faction* traders, TownBase* base)
{
    g_caravanState = CARAVAN_LEAVING;
    g_caravanStateHour = GameHours();
    Platoon* p = g_caravanSquad.getPlatoon();
    TownBase* home = traders->warMgr != 0 && base != 0 ? traders->warMgr->getNearestTown(TownObject(base)->getPosition()) : 0;
    if (base != 0)
    {
        Ogre::Vector3 at = TownObject(base)->getPosition();
        Ogre::Vector3 dir = home != 0 ? TownObject(home)->getPosition() - at : Ogre::Vector3(1.0f, 0.0f, 0.0f);
        dir.y = 0.0f;
        if (dir.length() < 1.0f)
            dir = Ogre::Vector3(1.0f, 0.0f, 0.0f);
        dir.normalise();
        g_caravanAwayPos = at + dir * (g_cfg.caravanSpawnDistance + 6000.0f);
        if (p != 0)
            OrderCaravan(p, g_caravanAwayPos);
    }
    Trace("caravan: leaving towards '" + (home != 0 ? TownObject(home)->getName() : std::string("?")) + "'");
    ShowNotice(T("The slave caravan is packing up and leaving.", "A caravana de escravos esta arrumando as coisas e indo embora."));
}

// Removes the caravan squad (an NPC squad: nothing else keeps it).
static void RemoveCaravan(Faction* traders, const std::string& why)
{
    Platoon* p = g_caravanSquad.getPlatoon();
    if (p != 0)
        traders->destroyPlatoon(p);
    Trace("caravan: removed (" + why + ")");
    ScheduleNextCaravan("visit over");
}


// While coming and staying, the caravan's AI may wander off: members too far from the base
// are walked back to it (a spot near the base, around the boss's side).
static void LeashCaravan(Platoon* squad, TownBase* base)
{
    ActivePlatoon* ap = squad->getActivePlatoon();
    lektor<RootObject*>* things = ap != 0 ? ap->getThings() : 0;
    if (things == 0)
        return;
    Ogre::Vector3 at = TownObject(base)->getPosition();
    int moved = 0;
    for (uint32_t i = 0; i < things->size(); i++)
    {
        Character* c = things->stuff[i] != 0 ? hand(things->stuff[i]).getCharacter() : 0;
        if (c == 0 || c->isDead() || c->medical.isUnconcious())
            continue;
        Ogre::Vector3 off = c->getPosition() - at;
        off.y = 0.0f;
        float d = off.length();
        if (d <= g_cfg.caravanLeash)
            continue;
        off *= (g_cfg.caravanArriveDistance * 0.5f) / d;
        OrderMove(c, at + off);
        moved++;
    }
    static DWORD lastLog = 0;
    if (moved > 0 && GetTickCount() - lastLog > 30000)
    {
        lastLog = GetTickCount();
        Trace("caravan: " + Num((float)moved) + " member(s) walked back to the base");
    }
}

static void UpdateSlaveCaravan()
{
    if (!g_cfg.enabled || ou->factionMgr == 0 || ou->theFactory == 0 || GetTickCount() < g_nextCaravanCheck)
        return;
    g_nextCaravanCheck = GetTickCount() + 2000;
    Faction* traders = SlaveTradersFaction();
    if (traders == 0)
        return;
    TownBase* base = PlayerBase();
    double now = GameHours();

    if (g_caravanState == CARAVAN_NONE)
    {
        if (!g_cfg.slaveCaravan)
            return;
        if (base == 0)
        {
            if (!g_caravanNoBaseLogged)
                Trace("caravan: the player has no base, no visit");
            g_caravanNoBaseLogged = true;
            return;
        }
        g_caravanNoBaseLogged = false;
        if (g_caravanNextHour < 0.0)
        {
            ScheduleNextCaravan("first schedule in this save");
            return;
        }
        if (traders->relations != 0 && traders->relations->isEnemy(PlayerFaction()))
            return;
        if (!g_caravanSoonNoticed && now >= g_caravanNextHour - 24.0)
        {
            g_caravanSoonNoticed = true;
            ShowNotice(T("A slave caravan of the Slave Traders will visit ", "Uma caravana de escravos dos Traficantes vai visitar ") +
                       TownObject(base)->getName() + T(" in about a day.", " em cerca de um dia."));
        }
        if (now >= g_caravanNextHour)
            SpawnCaravan(traders, base);
        return;
    }

    Platoon* squad = CaravanPlatoon(traders, base);
    if (squad == 0)
    {
        // destroyed, or not found again after a load
        if (now - g_caravanStateHour > 6.0)
        {
            Trace("caravan: squad not found");
            ScheduleNextCaravan("squad gone");
        }
        return;
    }
    Character* boss = CaravanBoss(squad);
    if (boss == 0)
    {
        // not loaded (far from your characters): its time still runs
        if (g_caravanState == CARAVAN_HERE && now >= g_caravanLeaveHour)
            CaravanLeaves(traders, base);
        else if (g_caravanState == CARAVAN_LEAVING && now - g_caravanStateHour > 12.0)
            RemoveCaravan(traders, "left while unloaded");
        return;
    }
    g_caravanLastPos = boss->getPosition();
    float dist = base != 0 ? (boss->getPosition() - TownObject(base)->getPosition()).length() : 1e9f;
    if (g_caravanState == CARAVAN_COMING)
    {
        if (base == 0)
            RemoveCaravan(traders, "base gone");
        else if (dist < g_cfg.caravanArriveDistance || now - g_caravanStateHour > 12.0)
            CaravanArrived(boss, base);
        else if (GetTickCount() - g_caravanLastLeaveOrder > 10000)
        {
            // its AI does not walk: it is walked to a spot by the base, on its side
            Ogre::Vector3 at = TownObject(base)->getPosition();
            Ogre::Vector3 off = boss->getPosition() - at;
            off.y = 0.0f;
            if (off.length() > 1.0f)
                off *= (g_cfg.caravanArriveDistance * 0.4f) / off.length();
            OrderCaravan(squad, at + off);
        }
    }
    else if (g_caravanState == CARAVAN_HERE)
    {
        if (now >= g_caravanLeaveHour || base == 0)
            CaravanLeaves(traders, base);
        else
            LeashCaravan(squad, base);
    }
    else if (g_caravanState == CARAVAN_LEAVING)
    {
        if (dist > g_cfg.caravanSpawnDistance + 4000.0f || now - g_caravanStateHour > 12.0)
            RemoveCaravan(traders, "left");
        else if (GetTickCount() - g_caravanLastLeaveOrder > 10000)
            OrderCaravan(squad, g_caravanAwayPos);   // keep them walking off
    }
}

// Test key: the caravan comes now, or leaves now if it is here.
static void TestCaravanNow()
{
    if (g_caravanState == CARAVAN_NONE)
    {
        g_caravanNextHour = GameHours();
        Trace("caravan test: visit now");
        ShowNotice(T("Test: the slave caravan comes now.", "Teste: a caravana de escravos vem agora."));
    }
    else if (g_caravanState == CARAVAN_HERE)
    {
        g_caravanLeaveHour = GameHours();
        Trace("caravan test: leave now");
        ShowNotice(T("Test: the slave caravan leaves now.", "Teste: a caravana de escravos vai embora agora."));
    }
    g_nextCaravanCheck = 0;
}

// ---------------------------------------------------------------------------
// Shackles forbid running (ShacklesNoRunning): the movement of your shackled slaves never asks
// for more than walking. The movers are listed once a second; the hooks only look them up.
// ---------------------------------------------------------------------------

static std::set<AbstractMovementBase*> g_walkOnly;
static DWORD g_nextWalkOnlyUpdate = 0;

static void UpdateWalkOnly()
{
    if (GetTickCount() < g_nextWalkOnlyUpdate)
        return;
    g_nextWalkOnlyUpdate = GetTickCount() + 1000;
    g_walkOnly.clear();
    if (!g_cfg.enabled || !g_cfg.shacklesNoRunning)
        return;
    // your slaves and every runaway slave (ESCAPING_SLAVE, yours or anyone's) still in leg shackles
    std::vector<Character*> movers;
    std::vector<Ogre::Vector3> spots;   // where your people are, 1500 apart: runaways nearby are checked
    const lektor<Character*>& all = ou->player->getAllPlayerCharacters();
    for (uint32_t i = 0; i < all.size(); i++)
    {
        Character* c = all.stuff[i];
        if (c == 0 || c->isDead())
            continue;
        movers.push_back(c);
        Ogre::Vector3 p = c->getPosition();
        bool seen = false;
        for (size_t k = 0; k < spots.size() && !seen; k++)
            seen = spots[k].squaredDistance(p) < 1500.0f * 1500.0f;
        if (!seen && spots.size() < 8)
            spots.push_back(p);
    }
    const lektor<Faction*>& factions = ou->factionMgr->participants;
    for (uint32_t i = 0; i < factions.size(); i++)
    {
        Faction* f = factions.stuff[i];
        if (f == 0 || f == PlayerFaction())
            continue;
        for (size_t k = 0; k < spots.size(); k++)
        {
            lektor<RootObject*> found;
            f->getCharactersInArea(found, spots[k], 3000.0f, false);
            for (uint32_t j = 0; j < found.size(); j++)
            {
                Character* c = found.stuff[j] != 0 ? hand(found.stuff[j]).getCharacter() : 0;
                if (c != 0 && !c->isDead() && c->isSlave() == ESCAPING_SLAVE)
                    movers.push_back(c);
            }
        }
    }
    std::set<Character*> rebels;   // rebels who could not get out of their shackles
    for (size_t i = 0; i < g_rebels.size(); i++)
    {
        Character* c = g_rebels[i].getCharacter();
        if (c != 0 && !c->isDead())
        {
            movers.push_back(c);
            rebels.insert(c);
        }
    }
    for (size_t i = 0; i < g_fleeingRebels.size(); i++)
    {
        Character* c = g_fleeingRebels[i].getCharacter();
        if (c != 0 && !c->isDead())
        {
            movers.push_back(c);
            rebels.insert(c);
        }
    }
    for (size_t i = 0; i < movers.size(); i++)
    {
        Character* c = movers[i];
        if (c->movement == 0 || !(c->isSlave() == ESCAPING_SLAVE || rebels.count(c) || IsOurSlave(c)) || !WearsLegRestraints(c))
            continue;
        AbstractMovementBase* m = c->movement;   // CharMovement derives from it first
        if (m->getSpeedOrders() > WALK)
            m->setDesiredSpeedOrders(WALK);
        g_walkOnly.insert(m);
    }
}

static MoveSpeed CapSpeed(AbstractMovementBase* m, MoveSpeed speed)
{
    return (speed == JOG || speed == RUN) && !g_walkOnly.empty() && g_walkOnly.count(m) ? WALK : speed;
}

static float CapSpeed(AbstractMovementBase* m, float speed)
{
    if (g_walkOnly.empty() || !g_walkOnly.count(m))
        return speed;
    float walk = m->getStandardWalkSpeed();
    return walk > 0.0f && speed > walk ? walk : speed;
}

void (*Move_setDesiredSpeed_orig)(AbstractMovementBase*, MoveSpeed) = NULL;
void Move_setDesiredSpeed_hook(AbstractMovementBase* thisptr, MoveSpeed speed)
{
    Move_setDesiredSpeed_orig(thisptr, CapSpeed(thisptr, speed));
}

void (*Move_setDesiredSpeedF_orig)(AbstractMovementBase*, float) = NULL;
void Move_setDesiredSpeedF_hook(AbstractMovementBase* thisptr, float speed)
{
    Move_setDesiredSpeedF_orig(thisptr, CapSpeed(thisptr, speed));
}

void (*Move_setDesiredSpeedOrders_orig)(AbstractMovementBase*, MoveSpeed) = NULL;
void Move_setDesiredSpeedOrders_hook(AbstractMovementBase* thisptr, MoveSpeed speed)
{
    Move_setDesiredSpeedOrders_orig(thisptr, CapSpeed(thisptr, speed));
}

void (*Move_setDesiredSpeedOrdersF_orig)(AbstractMovementBase*, float) = NULL;
void Move_setDesiredSpeedOrdersF_hook(AbstractMovementBase* thisptr, float speed)
{
    Move_setDesiredSpeedOrdersF_orig(thisptr, CapSpeed(thisptr, speed));
}

static void UpdateSoldToCage(float dt);

void GameWorld_update_hook(GameWorld* thisptr, float time)
{
    if (GameWorld_update_orig)
        GameWorld_update_orig(thisptr, time);
    ApplyModTexts();

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
            {
                GiveShackles(all.stuff[i]);
                ClearBounties(all.stuff[i]);   // slaves made before v1.18 kept them
            }
    }

    HandleHotkey();
    UpdateRebellion();
    UpdateInventoryButton();
    UpdateNotice();
    UpdateSubmissionBar();
    UpdateTradeWindow();
    UpdateSpeech();
    if (!ou->isPaused())
    {
        UpdateSubmission();
        ApplySlaveRules(time);
        UpdatePendingActions(time);
        UpdateRebels(time);
        UpdateEscapees(time);
        UpdateEscorts(time);
        UpdateSlaveCaravan();
        UpdateWalkOnly();
        UpdateSoldToCage(time);
        UpdateCaging(time);
        UpdateCageNight();
        WatchMembers(time, false);
        CalibrateTime(time);
    }
}

// ---------------------------------------------------------------------------
// Slave trade window: replaces the game's buy / sell slave windows at slave traders.
// "I'm looking to buy slaves" lists the trader's slaves nearby; "I want to sell some
// slaves" lists your slaves. Prices come from the slave's stats, race and submission.
// Bought slaves join your slave squad; sold ones go to the trader's faction.
// ---------------------------------------------------------------------------

struct TradeOffer
{
    hand who;
    int price;
    bool marked;    // ticked for the next BUY / SELL click
};

static MyGUI::Window* g_tradeWindow = NULL;
static MyGUI::MultiListBox* g_tradeList = NULL;
static MyGUI::TextBox* g_tradeInfo = NULL;
static MyGUI::Button* g_tradeDo = NULL;
static MyGUI::Button* g_tradeAll = NULL;
static bool g_tradeBuying = false;
static hand g_tradeTrader;
static hand g_tradeCustomer;
static std::vector<TradeOffer> g_tradeOffers;

static float StatOf(Character* c, StatsEnumerated s)
{
    CharStats* stats = c->getStats();
    return stats != 0 ? stats->getStat(s, true) : 0.0f;
}

// Worth of a slave: TradeBaseValue + TradeStatValue x (strength, toughness, dexterity,
// melee attack and defence, labouring) x race value x (0.75 + submission / 200).
// Arms and legs that are gone (a stump) or crushed; robotic replacements count as limbs.
static int MissingLimbs(Character* c)
{
    int n = 0;
    for (int l = RobotLimbs::LEFT_ARM; l <= RobotLimbs::RIGHT_LEG; l++)
    {
        LimbState st = c->medical.getLimbState((RobotLimbs::Limb)l);
        if (st == LIMB_STUMP || st == LIMB_CRUSHED)
            n++;
    }
    return n;
}

// ... x (1 - TradeMissingLimbPenalty per missing or crushed limb), at least 10%.
static float SlaveValue(Character* c, float submission)
{
    float stats = StatOf(c, STAT_STRENGTH) + StatOf(c, STAT_TOUGHNESS) + StatOf(c, STAT_DEXTERITY) +
                  StatOf(c, STAT_MELEE_ATTACK) + StatOf(c, STAT_MELEE_DEFENCE) + StatOf(c, STAT_LABOURING);
    float value = g_cfg.tradeBaseValue + g_cfg.tradeStatValue * stats;
    float limbs = 1.0f - g_cfg.tradeMissingLimbPenalty * MissingLimbs(c);
    if (limbs < 0.1f)
        limbs = 0.1f;
    return value * TraitsOf(c).value * (0.75f + ClampSubmission(submission) / 200.0f) * limbs;
}

static int BuyPrice(Character* c)
{
    return (int)(SlaveValue(c, TraitsOf(c).start + g_cfg.tradeBoughtBonus) * g_cfg.tradeBuyMult + 0.5f);
}

static int SellPrice(Character* c)
{
    return (int)(SlaveValue(c, SubmissionOf(c)) * g_cfg.tradeSellMult + 0.5f);
}

static Faction* SlavesFaction()
{
    return ou->factionMgr != 0 ? ou->factionMgr->getFactionByStringID("57740-rebirth.mod") : 0;
}

// The trader's slaves around them: slaves of the trader's faction, or owned by someone of it,
// or of the generic "Slaves" faction. Searched in every faction, since a captive keeps theirs.
static void FindSlavesForSale(Character* trader, std::vector<Character*>& out)
{
    Faction* tf = trader->getFaction();
    Faction* slaves = SlavesFaction();
    std::map<std::string, bool> seen;
    const lektor<Faction*>& factions = ou->factionMgr->participants;
    for (uint32_t i = 0; i < factions.size(); i++)
    {
        Faction* f = factions.stuff[i];
        if (f == 0 || f == PlayerFaction())
            continue;
        lektor<RootObject*> found;
        f->getCharactersInArea(found, trader->getPosition(), g_cfg.tradeRadius, false);
        for (uint32_t k = 0; k < found.size(); k++)
        {
            Character* c = found.stuff[k] != 0 ? hand(found.stuff[k]).getCharacter() : 0;
            if (c == 0 || c == trader || c->isDead() || IsPlayerFaction(c) || seen.count(SlaveKey(c)))
                continue;
            seen[SlaveKey(c)] = true;
            if (c->isSlave() != IS_SLAVE)
                continue;
            Character* owner = c->slaveOwner.getCharacter();
            Faction* of = owner != 0 ? owner->getFaction() : 0;
            bool forSale = c->getFaction() == tf || of == tf || (slaves != 0 && c->getFaction() == slaves);
            Trace("trade: slave '" + c->getName() + "' faction='" + (c->getFaction() != 0 ? c->getFaction()->getName() : std::string("?")) +
                  "' owner='" + (owner != 0 ? owner->getName() : std::string("-")) + "' forSale=" + Num(forSale));
            if (forSale)
                out.push_back(c);
        }
    }
}

static void CloseTradeWindow()
{
    if (g_tradeWindow != NULL)
        MyGUI::Gui::getInstance().destroyWidget(g_tradeWindow);
    g_tradeWindow = NULL;
    g_tradeList = NULL;
    g_tradeInfo = NULL;
    g_tradeDo = NULL;
    g_tradeAll = NULL;
    g_tradeOffers.clear();
}

static void UpdateTradeInfo()
{
    if (g_tradeInfo == NULL)
        return;
    Character* customer = g_tradeCustomer.getCharacter();
    std::string text = T("Your money: ", "Seu dinheiro: ") + Num((float)(customer != 0 ? customer->getMoney() : 0)) + " cats";
    int count = 0, total = 0;
    for (size_t i = 0; i < g_tradeOffers.size(); i++)
        if (g_tradeOffers[i].marked)
        {
            count++;
            total += g_tradeOffers[i].price;
        }
    if (count > 0)
        text += "   |   " + Num((float)count) + T(" selected, ", " marcado(s), ") +
                (g_tradeBuying ? T("price: ", "preco: ") : T("you get: ", "voce recebe: ")) + Num((float)total) + " cats";
    else if (!g_tradeOffers.empty())
        text += "   |   " + T("Click the slaves to select them.", "Clique nos escravos para marcar.");
    if (g_tradeAll != NULL)
        g_tradeAll->setCaption(count > 0 && count == (int)g_tradeOffers.size() ? T("CLEAR", "DESMARCAR") : T("SELECT ALL", "MARCAR TODOS"));
    if (g_tradeOffers.empty())
        text = (g_tradeBuying ? T("The trader has no slaves for sale here.", "O traficante nao tem escravos a venda aqui.")
                              : T("You have no slaves near the trader to sell.", "Voce nao tem escravos perto do traficante para vender.")) + "   " + text;
    g_tradeInfo->setCaption(text);
}

static std::string RowName(const TradeOffer& o)
{
    Character* c = o.who.getCharacter();
    return std::string(o.marked ? "[X] " : "[  ] ") + (c != 0 ? c->getName() : std::string("?"));
}

// " (woman)" / " (man)" after the race in the trade window; nothing for single-sex races.
static std::string SexMark(Character* c)
{
    RaceData* race = c->getRace();
    if (race != 0 && race->singleGender)
        return "";
    return c->isFemale() ? T(" (woman)", " (mulher)") : T(" (man)", " (homem)");
}

static void FillTradeList()
{
    g_tradeOffers.clear();
    g_tradeList->removeAllItems();
    Character* trader = g_tradeTrader.getCharacter();
    if (trader == 0)
        return;
    std::vector<Character*> people;
    if (g_tradeBuying)
        FindSlavesForSale(trader, people);
    else
    {
        // only the slaves around the trader, like the ones for sale (not those at a far base)
        const lektor<Character*>& all = ou->player->getAllPlayerCharacters();
        for (uint32_t i = 0; i < all.size(); i++)
        {
            Character* c = all.stuff[i];
            if (c != 0 && IsOurSlave(c) && !c->isDead() &&
                (c->getPosition() - trader->getPosition()).length() <= g_cfg.tradeRadius)
                people.push_back(c);
        }
    }
    for (size_t i = 0; i < people.size(); i++)
    {
        Character* c = people[i];
        TradeOffer o;
        o.who = hand(c);
        o.price = g_tradeBuying ? BuyPrice(c) : SellPrice(c);
        o.marked = false;
        g_tradeOffers.push_back(o);
        float sub = g_tradeBuying ? TraitsOf(c).start + g_cfg.tradeBoughtBonus : SubmissionOf(c);
        size_t row = g_tradeList->getItemCount();
        g_tradeList->addItem(RowName(o));
        g_tradeList->setSubItemNameAt(1, row, TraitsOf(c).name + SexMark(c));
        g_tradeList->setSubItemNameAt(2, row, Num((float)(int)StatOf(c, STAT_STRENGTH)));
        g_tradeList->setSubItemNameAt(3, row, Num((float)(int)StatOf(c, STAT_TOUGHNESS)));
        g_tradeList->setSubItemNameAt(4, row, Num((float)(int)StatOf(c, STAT_MELEE_ATTACK)));
        g_tradeList->setSubItemNameAt(5, row, Num((float)(int)(sub + 0.5f)) + " " + SubmissionLevel(sub));
        g_tradeList->setSubItemNameAt(6, row, Num((float)o.price));
    }
    Trace("trade: " + std::string(g_tradeBuying ? "buy" : "sell") + " list has " + Num((float)g_tradeOffers.size()) + " slave(s)");
    UpdateTradeInfo();
}

static void BuySlave(Character* customer, Character* trader, Character* c, int price)
{
    if (customer->getMoney() < price)
    {
        ShowNotice(T("Not enough money to buy ", "Dinheiro insuficiente para comprar ") + c->getName() + ".");
        return;
    }
    std::string name = c->getName();
    ActivePlatoon* squad = GetSlaveSquad();
    Platoon* previous = ou->player->getCurrentPlatoon();
    if (squad != 0 && squad->me != 0)
        ou->player->setCurrentPlatoon(squad->me);
    SetSlaveState(c, NOT_SLAVE);   // the game will not recruit someone else's slave
    c->changeSlaveOwner(hand());
    if (c->inSomething == IN_PRISON)
    {
        // cages are UseableStuff, which derives from Building first: same pointer
        UseableStuff* cage = reinterpret_cast<UseableStuff*>(c->inWhat.getBuilding());
        c->setPrisonMode(false, cage);
        Trace("trade: '" + name + "' let out of the cage (cage=" + Num(cage != 0) + ", still in prison=" + Num(c->inSomething == IN_PRISON) + ")");
    }
    if (c->isChainedMode())
        c->setChainedMode(false, hand(customer));
    Character* arr[1] = { c };
    lektor<Character*> list;
    list.stuff = arr;
    list.count = 1;
    list.maxSize = 1;
    ou->player->recruit(list, false);
    list.stuff = nullptr;
    list.count = 0;
    list.maxSize = 0;
    if (previous != 0)
        ou->player->setCurrentPlatoon(previous);
    if (!IsPlayerFaction(c))
    {
        SetSlaveState(c, IS_SLAVE);
        c->changeSlaveOwner(hand(trader));
        Trace("trade: recruit of '" + name + "' refused");
        ShowNotice(T("The game refused the purchase of ", "O jogo recusou a compra de ") + name + "." + RecruitRefusedReason());
        return;
    }
    customer->takeMoney(price);
    g_lastSlaveMember = hand(c);
    MakeSlave(c, customer);
    SlaveRecord& r = CreateRecord(c, "", TraitsOf(c).start + g_cfg.tradeBoughtBonus);
    GiveShackles(c);
    QueueEscort(c, customer);
    Say(c, "BoughtSlave", 800);
    Trace("trade: bought '" + name + "' for " + Num((float)price) + " cats, money now " + Num((float)customer->getMoney()));
    ShowNotice(customer->getName() + T(" bought ", " comprou ") + name + T(" for ", " por ") + Num((float)price) + " cats. " + SubmissionText(r.submission) + ".");
}

// The player's cats belong to the faction, not to a squad.
static void GivePlayerMoney(int amount)
{
    Faction* pf = PlayerFaction();
    if (pf != 0 && pf->factionOwnerships != 0)
        pf->factionOwnerships->addMoney(amount);
}

// Selling every slave of the slave squad leaves it empty, and an empty squad is no longer
// found by GetSlaveSquad (it looks through characters): a new one would be made next time.
// Squads of ours (named SquadName) that a sale emptied are deleted.
static void RemoveEmptySlaveSquads(const std::set<ActivePlatoon*>& squads)
{
    // SquadAutonomy (also inside Second Life of Kenshi) keeps every player squad and crashes the
    // game on the next save if one is destroyed under it: then the empty squad is left alone.
    if (GetModuleHandleA("SquadAutonomy.dll") != 0)
    {
        Trace("trade: SquadAutonomy loaded, empty slave squads are kept (deleting them crashes it on save)");
        return;
    }
    Faction* pf = PlayerFaction();
    for (std::set<ActivePlatoon*>::const_iterator it = squads.begin(); it != squads.end(); ++it)
    {
        ActivePlatoon* sq = *it;
        if (pf == 0 || sq == 0 || sq->me == 0 || !IsSlaveSquadName(sq->getName()) || sq->getSquadSize() > 0)
            continue;
        if (ou->player->getCurrentPlatoon() == sq->me)
        {
            // never leave the deleted squad selected
            const lektor<Character*>& all = ou->player->getAllPlayerCharacters();
            for (uint32_t i = 0; i < all.size(); i++)
                if (all.stuff[i] != 0 && all.stuff[i]->getPlatoon() != 0 && all.stuff[i]->getPlatoon() != sq)
                {
                    ou->player->setCurrentPlatoon(all.stuff[i]->getPlatoon()->me);
                    break;
                }
        }
        pf->destroyPlatoon(sq->me);
        Trace("trade: empty squad '" + sq->getName() + "' deleted");
    }
}

// A slave sold while unconscious (or carried in) was left lying on the floor: the slave's own
// "get in my master's cage" goal needs them awake. An awake member of the trader's faction near
// the shop (a guard rather than the trader if there is one) carries them to a cage instead.
static Character* ShopCarrier(Character* trader, Character* slave)
{
    Faction* f = trader->getFaction();
    if (f == 0)
        return 0;
    lektor<RootObject*> found;
    f->getCharactersInArea(found, slave->getPosition(), g_cfg.tradeRadius, false);
    Character* best = 0;
    float bestDist = 0.0f;
    for (uint32_t k = 0; k < found.size(); k++)
    {
        Character* c = found.stuff[k] != 0 ? hand(found.stuff[k]).getCharacter() : 0;
        if (c == 0 || c == slave || c->isDead() || c->isUnconcious() || c->isSlave() != NOT_SLAVE ||
            c->isCarryingSomething || c->isAnimal() != 0)
            continue;
        float d = (c->getPosition() - slave->getPosition()).length() + (c == trader ? 100000.0f : 0.0f);
        if (best == 0 || d < bestDist)
        {
            best = c;
            bestDist = d;
        }
    }
    return best;
}

// Sold slaves still lying down, waiting for a free carrier: in a batch sale the shop's guards
// are busy carrying the first ones, so the rest wait here and are picked up one by one.
struct SoldToCage
{
    hand trader, slave;
    float elapsed;
};
static std::vector<SoldToCage> g_soldToCage;
static float g_soldToCageTimer = 0.0f;

static bool TryCageSoldSlave(Character* trader, Character* c)
{
    Character* carrier = ShopCarrier(trader, c);
    if (carrier == 0)
        return false;
    carrier->pickupObject(c);
    carrier->addGoal(FIND_CAGE_AND_PUT_IN, c);
    Trace("trade: '" + carrier->getName() + "' carries the sold '" + c->getName() + "' to a cage (carrying=" + Num(carrier->isCarryingSomething) + ")");
    return true;
}

static void CageSoldSlave(Character* trader, Character* c)
{
    if (!c->isUnconcious() || c->isBeingCarried())
        return;
    if (TryCageSoldSlave(trader, c))
        return;
    SoldToCage s;
    s.trader = hand(trader);
    s.slave = hand(c);
    s.elapsed = 0.0f;
    g_soldToCage.push_back(s);
    Trace("trade: '" + c->getName() + "' waits for a free carrier of the trader's faction (" + Num((float)g_soldToCage.size()) + " waiting)");
}

static void UpdateSoldToCage(float dt)
{
    if (g_soldToCage.empty())
        return;
    g_soldToCageTimer += dt;
    if (g_soldToCageTimer < 1.0f)
        return;
    float step = g_soldToCageTimer;
    g_soldToCageTimer = 0.0f;
    for (size_t i = 0; i < g_soldToCage.size(); )
    {
        SoldToCage& s = g_soldToCage[i];
        Character* trader = s.trader.getCharacter();
        Character* c = s.slave.getCharacter();
        s.elapsed += step;
        bool done = trader == 0 || c == 0 || c->isDead() || !c->isUnconcious() || c->isBeingCarried() ||
                    c->inSomething == IN_PRISON || IsPlayerFaction(c) || s.elapsed > 120.0f;
        if (!done)
            done = TryCageSoldSlave(trader, c);
        else if (c != 0 && s.elapsed > 120.0f)
            Trace("trade: nobody came to carry '" + c->getName() + "' to a cage");
        if (done)
            g_soldToCage.erase(g_soldToCage.begin() + i);
        else
            i++;
    }
}

static void SellSlave(Character* customer, Character* trader, Character* c, int price)
{
    std::string name = c->getName();
    Faction* f = trader->getFaction();
    if (f == 0)
        return;
    for (size_t i = 0; i < g_pending.size(); )
    {
        if (g_pending[i].target.getCharacter() == c)
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

    // put them down first if one of ours is carrying them
    const lektor<Character*>& all = ou->player->getAllPlayerCharacters();
    for (uint32_t i = 0; i < all.size(); i++)
    {
        Character* carrier = all.stuff[i];
        if (carrier != 0 && carrier->isCarryingSomething && carrier->getCarryingObject().getCharacter() == c)
        {
            carrier->dropCarriedObject(false, false);
            Trace("trade: '" + carrier->getName() + "' put '" + name + "' down");
        }
    }

    // join the shop's own slaves (their faction and squad, which has the slave AI);
    // with none around, a new squad of the trader's faction
    std::vector<Character*> shop;
    FindSlavesForSale(trader, shop);
    Character* fellow = 0;
    for (size_t i = 0; i < shop.size() && fellow == 0; i++)
        if (shop[i] != c && shop[i]->getPlatoon() != 0 && shop[i]->slaveOwner.getCharacter() == trader)
            fellow = shop[i];
    for (size_t i = 0; i < shop.size() && fellow == 0; i++)
        if (shop[i] != c && shop[i]->getPlatoon() != 0)
            fellow = shop[i];
    Faction* toFaction = fellow != 0 ? fellow->getFaction() : f;
    ActivePlatoon* squad = fellow != 0 ? fellow->getPlatoon() : 0;
    if (squad == 0)
    {
        // a squad template gives the new squad the game's slave AI ("Slave only squad");
        // without one they would stand where they were sold forever
        GameData* ai = SquadTemplate(g_cfg.soldSquadTemplate);
        Platoon* platoon = f->createNewEmptyActivePlatoon(ai, false, c->getPosition());
        squad = platoon != 0 ? platoon->getActivePlatoon() : 0;
        Trace("trade: new squad for the sold, AI template " + (ai != 0 ? g_cfg.soldSquadTemplate : std::string("none")));
    }
    c->setFaction(toFaction, squad);
    SetSlaveState(c, IS_SLAVE);
    c->changeSlaveOwner(hand(trader));
    c->setSlaveAIJob(true);
    c->addGoal(SLAVE_GET_IN_MY_MASTERS_CAGE, trader);
    ForgetRecord(c);
    if (IsPlayerFaction(c))
    {
        Trace("trade: '" + name + "' did not leave the faction");
        ShowNotice(T("The game refused the sale of ", "O jogo recusou a venda de ") + name + ".");
        return;
    }
    Say(c, "SoldSlave", 300);
    CageSoldSlave(trader, c);
    int before = customer->getMoney();
    GivePlayerMoney(price);
    Trace("trade: sold '" + name + "' to '" + (toFaction != 0 ? toFaction->getName() : std::string("?")) + "' (squad of " +
          (fellow != 0 ? "'" + fellow->getName() + "'" : std::string("a new squad")) + ") for " + Num((float)price) +
          " cats, money " + Num((float)before) + " -> " + Num((float)customer->getMoney()));
    ShowNotice(customer->getName() + T(" sold ", " vendeu ") + name + T(" for ", " por ") + Num((float)price) + " cats.");
}

// Buys or sells every ticked slave at once.
static void OnTradeDo(MyGUI::Widget* sender)
{
    Character* customer = g_tradeCustomer.getCharacter();
    Character* trader = g_tradeTrader.getCharacter();
    if (customer == 0 || trader == 0)
        return;
    std::vector<TradeOffer> picked;
    for (size_t i = 0; i < g_tradeOffers.size(); i++)
        if (g_tradeOffers[i].marked)
            picked.push_back(g_tradeOffers[i]);
    if (picked.empty())
    {
        ShowNotice(T("Click the slaves you want first.", "Primeiro clique nos escravos que voce quer."));
        return;
    }
    int done = 0, total = 0;
    std::set<ActivePlatoon*> emptied;   // squads the slaves are sold out of
    for (size_t i = 0; i < picked.size(); i++)
    {
        Character* c = picked[i].who.getCharacter();
        if (c == 0 || c->isDead())
            continue;
        if (!g_tradeBuying && c->getPlatoon() != 0)
            emptied.insert(c->getPlatoon());
        bool before = g_tradeBuying ? IsPlayerFaction(c) : !IsOurSlave(c);
        if (g_tradeBuying)
            BuySlave(customer, trader, c, picked[i].price);
        else
            SellSlave(customer, trader, c, picked[i].price);
        bool after = g_tradeBuying ? IsPlayerFaction(c) : !IsPlayerFaction(c);
        if (!before && after)
        {
            done++;
            total += picked[i].price;
        }
    }
    Trace("trade: batch of " + Num((float)picked.size()) + ", " + Num((float)done) + " done for " + Num((float)total) + " cats");
    RemoveEmptySlaveSquads(emptied);
    if (done > 1)
        ShowNotice(customer->getName() + (g_tradeBuying ? T(" bought ", " comprou ") : T(" sold ", " vendeu ")) + Num((float)done) +
                   T(" slaves for ", " escravos por ") + Num((float)total) + " cats.");
    if (g_tradeList != NULL)
        FillTradeList();
}

// A click on a row ticks / unticks it (the highlight is cleared so the same row can be clicked again).
static void ToggleTradeRow(size_t index)
{
    if (g_tradeList == NULL || index == MyGUI::ITEM_NONE || index >= g_tradeOffers.size())
        return;
    g_tradeOffers[index].marked = !g_tradeOffers[index].marked;
    g_tradeList->setItemNameAt(index, RowName(g_tradeOffers[index]));
    g_tradeList->clearIndexSelected();
    UpdateTradeInfo();
}

static void OnTradeAll(MyGUI::Widget* sender)
{
    bool all = !g_tradeOffers.empty();
    for (size_t i = 0; i < g_tradeOffers.size(); i++)
        all = all && g_tradeOffers[i].marked;
    for (size_t i = 0; i < g_tradeOffers.size(); i++)
    {
        g_tradeOffers[i].marked = !all;
        if (g_tradeList != NULL)
            g_tradeList->setItemNameAt(i, RowName(g_tradeOffers[i]));
    }
    UpdateTradeInfo();
}

// RequireShackles: the slave traders also sell shackle kits, one per click.
static void OnTradeKit(MyGUI::Widget* sender)
{
    Character* customer = g_tradeCustomer.getCharacter();
    if (customer == 0)
        return;
    if (customer->getMoney() < g_cfg.shackleKitPrice)
    {
        ShowNotice(T("Not enough money for a shackle kit.", "Dinheiro insuficiente para um kit de grilhoes."));
        return;
    }
    if (!GiveShackleKit(customer, 1))
    {
        ShowNotice(T("The shackle kit item is missing from the game data.", "O item kit de grilhoes nao esta nos dados do jogo."));
        return;
    }
    customer->takeMoney(g_cfg.shackleKitPrice);
    Trace("trade: '" + customer->getName() + "' bought a shackle kit for " + Num((float)g_cfg.shackleKitPrice) + " cats");
    ShowNotice(customer->getName() + T(" bought a shackle kit.", " comprou um kit de grilhoes."));
    UpdateTradeInfo();
}

static void OnTradeClose(MyGUI::Widget* sender)
{
    CloseTradeWindow();
}

static void OnTradeWindowButton(MyGUI::Window* sender, const std::string& name)
{
    if (name == "close")
        CloseTradeWindow();
}

static void OnTradeSelect(MyGUI::MultiListBox* sender, size_t index)
{
    ToggleTradeRow(index);
}

static void OpenTradeWindow(Character* trader, Character* customer, bool buying)
{
    CloseTradeWindow();
    MyGUI::Gui* mg = MyGUI::Gui::getInstancePtr();
    if (mg == NULL || trader == 0 || customer == 0)
        return;
    g_tradeTrader = hand(trader);
    g_tradeCustomer = hand(customer);
    g_tradeBuying = buying;

    const int w = 760, h = 460;
    const MyGUI::IntSize& view = MyGUI::RenderManager::getInstance().getViewSize();
    g_tradeWindow = mg->createWidget<MyGUI::Window>("Kenshi_WindowCX", (view.width - w) / 2, (view.height - h) / 2, w, h,
                                                    MyGUI::Align::Default, "Window", "SlaverModTrade");
    if (g_tradeWindow == NULL)
        return;
    g_tradeWindow->setCaption(buying ? T("BUYING SLAVES - ", "COMPRANDO ESCRAVOS - ") + trader->getName()
                                     : T("SELLING SLAVES - ", "VENDENDO ESCRAVOS - ") + trader->getName());
    g_tradeWindow->eventWindowButtonPressed += MyGUI::newDelegate(OnTradeWindowButton);
    const MyGUI::IntCoord client = g_tradeWindow->getClientCoord();
    int cw = client.width, ch = client.height;

    g_tradeList = g_tradeWindow->createWidget<MyGUI::MultiListBox>("Kenshi_MultiListBox", 8, 8, cw - 16, ch - 90, MyGUI::Align::Stretch, "SlaverModTradeList");
    const char* en[7] = { "Name", "Race", "Str", "Tough", "Attack", "Submission", "Price" };
    const char* pt[7] = { "Nome", "Raca", "For", "Rob", "Ataque", "Submissao", "Preco" };
    const float widths[7] = { 0.22f, 0.22f, 0.07f, 0.08f, 0.09f, 0.18f, 0.14f };
    for (int i = 0; i < 7; i++)
        g_tradeList->addColumn(T(en[i], pt[i]), (int)((cw - 40) * widths[i]));
    g_tradeList->eventListChangePosition += MyGUI::newDelegate(OnTradeSelect);

    g_tradeInfo = g_tradeWindow->createWidget<MyGUI::TextBox>("Kenshi_GenericTextBoxFlat", 8, ch - 76, cw - 16, 28, MyGUI::Align::Default, "SlaverModTradeInfo");
    g_tradeAll = g_tradeWindow->createWidget<MyGUI::Button>("Kenshi_Button1", 8, ch - 42, 170, 34, MyGUI::Align::Default, "SlaverModTradeAll");
    g_tradeAll->setCaption(T("SELECT ALL", "MARCAR TODOS"));
    g_tradeAll->eventMouseButtonClick += MyGUI::newDelegate(OnTradeAll);
    if (buying && g_cfg.requireShackles && ShackleKitData() != 0)
    {
        MyGUI::Button* kit = g_tradeWindow->createWidget<MyGUI::Button>("Kenshi_Button1", 186, ch - 42, cw - 186 - 368, 34, MyGUI::Align::Default, "SlaverModTradeKit");
        kit->setCaption(T("SHACKLE KIT ", "KIT DE GRILHOES ") + Num((float)g_cfg.shackleKitPrice));
        kit->eventMouseButtonClick += MyGUI::newDelegate(OnTradeKit);
    }
    g_tradeDo = g_tradeWindow->createWidget<MyGUI::Button>("Kenshi_Button1", cw - 360, ch - 42, 170, 34, MyGUI::Align::Default, "SlaverModTradeDo");
    g_tradeDo->setCaption(buying ? T("BUY", "COMPRAR") : T("SELL", "VENDER"));
    g_tradeDo->eventMouseButtonClick += MyGUI::newDelegate(OnTradeDo);
    MyGUI::Button* close = g_tradeWindow->createWidget<MyGUI::Button>("Kenshi_Button1", cw - 182, ch - 42, 170, 34, MyGUI::Align::Default, "SlaverModTradeClose");
    close->setCaption(T("CLOSE", "FECHAR"));
    close->eventMouseButtonClick += MyGUI::newDelegate(OnTradeClose);

    FillTradeList();
    Trace("trade window: " + std::string(buying ? "buy" : "sell") + " with '" + trader->getName() + "' (" +
          (trader->getFaction() != 0 ? trader->getFaction()->getName() : std::string("?")) + ") for '" + customer->getName() + "'");
}

// Closes the window when the trader or the customer is gone or they walked away.
static void UpdateTradeWindow()
{
    if (g_tradeWindow == NULL)
        return;
    Character* trader = g_tradeTrader.getCharacter();
    Character* customer = g_tradeCustomer.getCharacter();
    if (trader == 0 || customer == 0 || trader->isDead() || customer->isDead() ||
        (trader->getPosition() - customer->getPosition()).length() > g_cfg.tradeRadius)
        CloseTradeWindow();
}

// ---------------------------------------------------------------------------
// Watch (log only): dialogue actions about money and slaves, and every character
// that joins or leaves the player's faction. Also where our trade window takes over.
// ---------------------------------------------------------------------------

static const char* SlaveDialogActionName(int key)
{
    switch (key)
    {
    case DA_TAKE_MONEY: return "TAKE_MONEY";
    case DA_GIVE_MONEY: return "GIVE_MONEY";
    case DA_GIVE_TARGET_MY_SLAVES: return "GIVE_TARGET_MY_SLAVES";
    case DA_TAG_ESCAPED_SLAVE: return "TAG_ESCAPED_SLAVE";
    case DA_FREE_TARGET_SLAVE: return "FREE_TARGET_SLAVE";
    case DA_ENSLAVE_TARGETS_CARRIED_PERSON: return "ENSLAVE_TARGETS_CARRIED_PERSON";
    case CHOOSE_SLAVES_SELLING: return "CHOOSE_SLAVES_SELLING";
    case CHOOSE_SLAVES_BUYING: return "CHOOSE_SLAVES_BUYING";
    case CHOOSE_PRISONER_BAIL: return "CHOOSE_PRISONER_BAIL";
    case DA_PLAYER_SELL_PRISONERS: return "PLAYER_SELL_PRISONERS";
    case DA_REMOVE_SLAVE_STATUS: return "REMOVE_SLAVE_STATUS";
    default: return 0;
    }
}

static int PlayerMoney()
{
    const lektor<Character*>& all = ou->player->getAllPlayerCharacters();
    return all.size() > 0 && all.stuff[0] != 0 ? all.stuff[0]->getMoney() : 0;
}

void (*Dialogue_doActions_orig)(Dialogue*, DialogLineData*) = NULL;
void Dialogue_doActions_hook(Dialogue* thisptr, DialogLineData* line)
{
    const lektor<DialogLineData::DialogAction*>* actions = line != 0 ? line->getActions() : 0;
    std::string found;
    int trade = -1;   // CHOOSE_SLAVES_BUYING / SELLING: our window instead of the game's
    if (actions != 0)
        for (uint32_t i = 0; i < actions->size(); i++)
        {
            DialogLineData::DialogAction* a = actions->stuff[i];
            if (a != 0 && (a->key == CHOOSE_SLAVES_BUYING || a->key == CHOOSE_SLAVES_SELLING))
                trade = a->key;
            const char* name = a != 0 ? SlaveDialogActionName(a->key) : 0;
            if (name != 0)
                found += std::string(" ") + name + "(" + Num((float)a->value) + ")";
        }
    if (trade >= 0 && g_cfg.enabled && g_cfg.tradeWindow && thisptr != 0&& ou != 0 && ou->player != 0 && actions->size() == 1)
    {
        Character* trader = thisptr->me;
        Character* customer = thisptr->conversationTarget.getCharacter();
        if (trader != 0 && customer != 0 && IsPlayerFaction(customer) && !IsPlayerFaction(trader))
        {
            OpenTradeWindow(trader, customer, trade == CHOOSE_SLAVES_BUYING);
            return;
        }
    }
    int before = found.empty() || ou == 0 || ou->player == 0 ? 0 : PlayerMoney();
    Dialogue_doActions_orig(thisptr, line);
    if (found.empty() || ou == 0 || ou->player == 0)
        return;
    Character* me = thisptr != 0 ? thisptr->me : 0;
    Character* target = thisptr != 0 ? thisptr->conversationTarget.getCharacter() : 0;
    Trace("watch: dialog of '" + (me != 0 ? me->getName() : std::string("?")) + "' (" +
          (me != 0 && me->getFaction() != 0 ? me->getFaction()->getName() : std::string("?")) + ") with '" +
          (target != 0 ? target->getName() : std::string("?")) + "':" + found + " money " + Num((float)before) + " -> " + Num((float)PlayerMoney()));
}

// The fixed-price offer for a carried person ("I'll give you 400 Cats"): with the trade
// window on, the lines that need DC_CARRYING_SOMEONE_TO_ENSLAVE are never chosen.
bool (*DialogLineData_checkConditions_orig)(DialogLineData*, Dialogue*, Character*, bool) = NULL;
// RebelsDistrust: a slave who rose up against you never joins you again through dialogue.
static bool IsDistrustful(Character* c)
{
    if (c == 0 || g_distrust.empty() || IsPlayerFaction(c))
        return false;
    std::map<std::string, std::string>::const_iterator it = g_distrust.find(SlaveKey(c));
    if (it != g_distrust.end())
        return true;
    // the game may have given them a new handle (another squad): the name still tells
    std::string name = c->getName();
    for (it = g_distrust.begin(); it != g_distrust.end(); ++it)
        if (it->second == name)
            return true;
    return false;
}

static bool JoinsPlayer(DialogLineData* line)
{
    const lektor<DialogLineData::DialogAction*>* actions = line != 0 ? line->getActions() : 0;
    if (actions != 0)
        for (uint32_t i = 0; i < actions->size(); i++)
        {
            DialogLineData::DialogAction* a = actions->stuff[i];
            if (a != 0 && (a->key == DA_JOIN_SQUAD_WITH_EDIT || a->key == DA_JOIN_SQUAD_FAST))
                return true;
        }
    return false;
}

static std::set<Character*> g_loggedDistrust;

bool DialogLineData_checkConditions_hook(DialogLineData* thisptr, Dialogue* dialog, Character* target, bool isWordswap)
{
    if (g_cfg.enabled && g_cfg.rebelsDistrust && !g_distrust.empty() && thisptr != 0 && JoinsPlayer(thisptr))
    {
        Character* me = dialog != 0 ? dialog->me : 0;
        Character* who = IsDistrustful(me) ? me : (IsDistrustful(target) ? target : 0);
        if (who != 0)
        {
            if (g_loggedDistrust.insert(who).second)
                Trace("distrust: '" + who->getName() + "' rose up against you and will not join again");
            return false;
        }
    }
    if (g_cfg.enabled && g_cfg.disableCarriedSale && thisptr != 0)
        for (uint32_t i = 0; i < thisptr->conditions.size(); i++)
        {
            DialogLineData::DialogCondition* c = thisptr->conditions.stuff[i];
            if (c != 0 && c->key == DC_CARRYING_SOMEONE_TO_ENSLAVE)
                return false;
        }
    return DialogLineData_checkConditions_orig(thisptr, dialog, target, isWordswap);
}

// ---------------------------------------------------------------------------
// Other factions' slavers treat any slave around as theirs: they walk up to "process"
// the player's slaves, look for them to ship, punish or cage. Their target finders are
// hooked so a slave of the player is never a valid target for anyone else's AI.
// ---------------------------------------------------------------------------

// Copied from kenshi/AI/AI.h, which redefines CharacterMessage and cannot be included here:
// only the target finders hooked below and the squad pointer (vtable at 0x0, platoon at 0x10).
class AI
{
public:
    float findSomeoneWhoWouldMakeAGoodSlave(const hand& in, hand& out, bool justAsking);// public RVA = 0x8CC550
    float findBadSlaves(const hand& in, hand& out, bool justAsking);// public RVA = 0x8CBE80
    float findNewSlaveOrPrisonerToProcess(const hand& in, hand& out, bool justAsking);// public RVA = 0x8CA960
    float findRandomOneOfMySlaves(const hand& in, hand& out, bool justAsking);// public RVA = 0x8CC190
    float findConsciousSlaveInTown(const hand& in, hand& out, bool justAsking);// public RVA = 0x8CAD00
    float findConsciousSlaveInTownForShipping(const hand& in, hand& out, bool justAsking);// public RVA = 0x8CAE50
    float findConsciousSlaveInTown_unholy(const hand& in, hand& out, bool justAsking);// public RVA = 0x8CFC60
    float findKOedSlaveOfMine(const hand& in, hand& out, bool justAsking);// public RVA = 0x8CC7A0
    float _findWoundedAllies(hand& out, bool robots);// public RVA = 0x99DEE0
    float _NV_scoreGetFood_inHomeTown(const hand& subject, const Ogre::Vector3& v);// public RVA = 0x59B450 (vtable 0xC0)
    void* vtable_;
    float meiDistance; // 0x8 Member
    float meiDistanceTolerance; // 0xC Member
    Platoon* platoon; // 0x10 Member
};

static std::map<std::string, bool> g_loggedBlocks;

static bool BlockSlaverTarget(AI* ai, hand& out, const char* finder)
{
    Character* t = out.getCharacter();
    if (t == 0 || !IsOurSlave(t))
        return false;
    Faction* f = ai != 0 && ai->platoon != 0 ? ai->platoon->getFaction() : 0;
    if (f == 0 || f == PlayerFaction())
        return false;
    std::string key = std::string(finder) + "|" + f->getName() + "|" + SlaveKey(t);
    if (!g_loggedBlocks.count(key))
    {
        g_loggedBlocks[key] = true;
        Trace("protect: '" + f->getName() + "' AI (" + finder + ") wanted '" + t->getName() + "', blocked");
    }
    out.setNull();
    return true;
}

#define SLAVER_FINDER_HOOK(fn) \
float (*AI_##fn##_orig)(AI*, const hand&, hand&, bool) = NULL; \
float AI_##fn##_hook(AI* ai, const hand& in, hand& out, bool justAsking) \
{ \
    float score = AI_##fn##_orig(ai, in, out, justAsking); \
    if (g_cfg.protectSlaves && BlockSlaverTarget(ai, out, #fn)) \
        return 0.0f; \
    return score; \
}

SLAVER_FINDER_HOOK(findNewSlaveOrPrisonerToProcess)
SLAVER_FINDER_HOOK(findBadSlaves)
SLAVER_FINDER_HOOK(findRandomOneOfMySlaves)
SLAVER_FINDER_HOOK(findConsciousSlaveInTown)
SLAVER_FINDER_HOOK(findConsciousSlaveInTownForShipping)
SLAVER_FINDER_HOOK(findConsciousSlaveInTown_unholy)
SLAVER_FINDER_HOOK(findKOedSlaveOfMine)
SLAVER_FINDER_HOOK(findSomeoneWhoWouldMakeAGoodSlave)

// Someone sent to enslave / free / punish / reward does that first: while the order lasts
// they do not go patch up wounded allies on their own (the AI's "me" is at 0x2F8).
static bool IsPerformer(Character* c)
{
    for (size_t i = 0; i < g_pending.size(); i++)
        if (g_pending[i].performer.getCharacter() == c)
            return true;
    return false;
}

static std::map<std::string, bool> g_loggedBusy;

// A slave of the player's faction is still a member of it: the game counts them as enemies of
// their own faction (turret guards shot them). Between two members of the player's faction, one
// of them our slave, nobody is an enemy (rebels leave the faction first, so they still fight).
// v1.18: the same goes for anyone whose faction is not at war with the player (SparedByNeutrals):
// allies' turrets and gunners, town guards and modded factions kept shooting our slaves when
// the slave's race or old faction made them look hostile (UWE mist ghouls, Bounties Galore).
static bool SlaveOfFaction(Character* c, Faction* pf)
{
    return c->getFaction() == pf && c->isSlave() == IS_SLAVE;
}

static bool OwnSlavePair(Character* a, Character* b)
{
    if (!g_cfg.enabled || a == 0 || b == 0 || a == b)
        return false;
    Faction* pf = PlayerFaction();
    if (pf == 0)
        return false;
    bool aSlave = SlaveOfFaction(a, pf), bSlave = SlaveOfFaction(b, pf);
    if (!aSlave && !bSlave)
        return false;
    Character* other = aSlave ? b : a;
    Faction* of = other->getFaction();
    if (of == pf)
        return true;
    if (!g_cfg.sparedByNeutrals || of == 0 || of->relations == 0)
        return false;
    return !of->relations->isEnemy(pf);
}

static std::set<Character*> g_loggedEnemy;
bool (*Character_isEnemy_orig)(Character*, Character*, bool) = NULL;
bool Character_isEnemy_hook(Character* thisptr, Character* who, bool factorInDisguises)
{
    bool enemy = Character_isEnemy_orig(thisptr, who, factorInDisguises);
    if (enemy && OwnSlavePair(thisptr, who))
    {
        Character* slave = SlaveOfFaction(who, PlayerFaction()) ? who : thisptr;
        if (g_loggedEnemy.insert(slave).second)
            Trace("isEnemy: '" + thisptr->getName() + "' vs '" + who->getName() + "' was enemy, now not (slave '" + slave->getName() + "')");
        return false;
    }
    return enemy;
}

// Turrets score their targets; logged once per slave of ours, to see what the game gives them.
static std::set<Character*> g_loggedTurret;
float (*GunClassTurret_scoreTarget_orig)(GunClassTurret*, Character*, Character*) = NULL;
float GunClassTurret_scoreTarget_hook(GunClassTurret* thisptr, Character* me, Character* who)
{
    float score = GunClassTurret_scoreTarget_orig(thisptr, me, who);
    if (OwnSlavePair(me, who))
    {
        // the turret gave our own slaves a positive score (0.1-1) and shot them: 0 = not a target
        if (g_loggedTurret.insert(who).second)
            Trace("turret: '" + (me != 0 ? me->getName() : std::string("?")) + "' scored own slave '" + who->getName() + "' " + Num(score) + " -> 0");
        return 0.0f;
    }
    if (me != 0 && who != 0 && score != 0.0f && g_loggedTurret.insert(who).second && g_loggedTurret.size() < 40)
        Trace("turret: '" + me->getName() + "' scores '" + who->getName() + "' (" + (who->getFaction() != 0 ? who->getFaction()->getName() : std::string("?")) +
              ", player=" + Num(IsPlayerFaction(who)) + ") = " + Num(score));
    return score;
}

// Any order the player gives a performer (move, stop, hold, a job...) cancels their slave action.
void (*Character_addOrder_orig)(Character*, Building*, TaskType, RootObject*, bool, bool, const Ogre::Vector3&) = NULL;
void Character_addOrder_hook(Character* thisptr, Building* dest, TaskType t, RootObject* subject, bool shift, bool clear, const Ogre::Vector3& location)
{
    if (!g_ownOrder)
        for (size_t i = 0; i < g_pending.size(); i++)
            if (g_pending[i].performer.getCharacter() == thisptr && !g_pending[i].playerOrdered)
            {
                g_pending[i].playerOrdered = true;
                Trace("order: '" + thisptr->getName() + "' got order " + Num((float)t) + " from the player");
            }
    Character_addOrder_orig(thisptr, dest, t, subject, shift, clear, location);
}

// SlaveFood: own = slaves only eat food they carry (feed them by hand); none = they never eat
// (the game keeps slaves from starving). Either way they stop raiding your food storage.
static bool SlaveMayEat(Character* c, Inventory* from)
{
    if (!g_cfg.enabled || g_cfg.slaveFood == "any" || c == 0 || !IsOurSlave(c))
        return true;
    if (g_cfg.slaveFood == "none")
        return false;
    return from == 0 || from == c->inventory;
}

static std::set<Character*> g_loggedFood;
bool (*Character_eatItem_orig)(Character*, Item*, Inventory*) = NULL;
bool Character_eatItem_hook(Character* thisptr, Item* food, Inventory* from)
{
    if (!SlaveMayEat(thisptr, from))
    {
        if (g_loggedFood.insert(thisptr).second)
            Trace("food: slave '" + thisptr->getName() + "' may not eat " + (food != 0 ? "'" + food->getName() + "'" : std::string("?")) +
                  " (SlaveFood=" + g_cfg.slaveFood + ", " + (from == thisptr->inventory ? "own" : "other") + " inventory)");
        return false;
    }
    return Character_eatItem_orig(thisptr, food, from);
}

float (*AI_scoreGetFood_orig)(AI*, const hand&, const Ogre::Vector3&) = NULL;
float AI_scoreGetFood_hook(AI* ai, const hand& subject, const Ogre::Vector3& v)
{
    Character* me = ai != 0 ? *reinterpret_cast<Character**>(reinterpret_cast<char*>(ai) + 0x2F8) : 0;
    if (me != 0 && g_cfg.enabled && g_cfg.slaveFood != "any" && IsOurSlave(me))
        return 0.0f;   // never walks to the food storage
    return AI_scoreGetFood_orig(ai, subject, v);
}

float (*AI__findWoundedAllies_orig)(AI*, hand&, bool) = NULL;
float AI__findWoundedAllies_hook(AI* ai, hand& out, bool robots)
{
    Character* me = ai != 0 ? *reinterpret_cast<Character**>(reinterpret_cast<char*>(ai) + 0x2F8) : 0;
    if (me != 0 && !g_pending.empty() && IsPerformer(me))
    {
        if (!g_loggedBusy.count(SlaveKey(me)))
        {
            g_loggedBusy[SlaveKey(me)] = true;
            Trace("busy: '" + me->getName() + "' skips first aid until the slave order is done");
        }
        out.setNull();
        return 0.0f;
    }
    return AI__findWoundedAllies_orig(ai, out, robots);
}

// Who is in the player's faction, and as what, compared every couple of seconds.
static std::map<std::string, std::string> g_members;   // key -> "name|slaveState"
static float g_memberTimer = 0.0f;

static void WatchMembers(float dt, bool reset)
{
    if (reset)
    {
        g_members.clear();
        g_memberTimer = 0.0f;
        return;
    }
    g_memberTimer += dt;
    if (g_memberTimer < 2.0f)
        return;
    g_memberTimer = 0.0f;
    std::map<std::string, std::string> now;
    const lektor<Character*>& all = ou->player->getAllPlayerCharacters();
    for (uint32_t i = 0; i < all.size(); i++)
    {
        Character* c = all.stuff[i];
        if (c == 0)
            continue;
        std::string state = c->getName() + "|" + Num((float)c->isSlave()) + "|" + RaceID(c);
        if (c->isSlave() == IS_SLAVE)
        {
            Faction* of = SlaveOwnerFaction(c);
            state += std::string("|slave of ") + (of != 0 ? of->getName() : std::string("-")) + (IsOurSlave(c) ? " (ours)" : " (not ours)");
        }
        now[SlaveKey(c)] = state;
    }
    bool first = g_members.empty();
    if (first)
    {
        for (std::map<std::string, std::string>::const_iterator it = now.begin(); it != now.end(); ++it)
            if (it->second.find("|slave of ") != std::string::npos)
                Trace("watch: slave [" + it->first + "] " + it->second);
    }
    else
    {
        for (std::map<std::string, std::string>::const_iterator it = now.begin(); it != now.end(); ++it)
        {
            std::map<std::string, std::string>::const_iterator old = g_members.find(it->first);
            if (old == g_members.end())
                Trace("watch: joined [" + it->first + "] " + it->second + " record=" + Num(g_records.count(it->first) > 0) + " money=" + Num((float)PlayerMoney()));
            else if (old->second != it->second)
                Trace("watch: changed [" + it->first + "] " + old->second + " -> " + it->second);
        }
        for (std::map<std::string, std::string>::const_iterator it = g_members.begin(); it != g_members.end(); ++it)
            if (now.find(it->first) == now.end())
                Trace("watch: left [" + it->first + "] " + it->second + " record=" + Num(g_records.count(it->first) > 0) + " money=" + Num((float)PlayerMoney()));
    }
    g_members.swap(now);
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
    g_leavers.clear();
    g_fleeingRebels.clear();
    g_caging.clear();
    g_soldToCage.clear();
    g_wasNight = -1;
    g_unsupervisedDays.clear();
    g_discipline.clear();
    g_speech.clear();
    WatchMembers(0.0f, true);
    CloseTradeWindow();
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
    g_caravanState = 0;
    g_caravanNextHour = -1.0;
    g_caravanLeaveHour = -1.0;
    g_caravanSquadID.clear();
    g_distrust.clear();
    g_nightCaged.clear();
    g_lastKey.clear();
    g_recordsSave = "";
    g_recordsReady = true;
    SaveManager_newGame_orig(thisptr, startId);
}

// ---------------------------------------------------------------------------
// ModConfigMenu (optional): an in-game settings page in Options > MCM.
// MCM writes SlaverMod_user.ini and calls ReloadSettings; without MCM nothing changes.
// ---------------------------------------------------------------------------

static void __cdecl ReloadSettings()
{
    LoadSettings();
    LoadRaces();
    LoadLines();
    ParseHotkey();
}

extern "C" __declspec(dllexport) void MCM_Describe(MCM_Api* api)
{
    static const std::string ini = UserIniPath();   // must outlive the call
    const char* const sec = "SlaverMod";
#define L(en, pt) T(en, pt).c_str()

    api->beginMod(api, "SlaverMod", "SlaverMod", ini.c_str(), &ReloadSettings);
    if (api->version >= 2)
        api->info(api, L("Your faction can own slaves: enslave, shackles, relations and rebellion. Translations: the lang folder.",
                         "Sua facção pode ter escravos: escravizar, grilhões, relações e rebelião. Traduzido para português do Brasil."));

    api->section(api, L("General", "Geral"));
    // auto, the built-in English and Portuguese, then every lang\*.txt (named by its [info] name)
    static std::vector<std::string> langCodes, langNames;
    static std::vector<const char*> langValues, langLabels;
    langCodes.clear();
    langNames.clear();
    langCodes.push_back("auto");
    langNames.push_back(T("Game's language", "Idioma do jogo"));
    langCodes.push_back("en");
    langNames.push_back("English");
    langCodes.push_back("pt");
    langNames.push_back("Portugu\xc3\xaas (Brasil)");
    std::vector<std::pair<std::string, std::string> > files = ListLangFiles();
    for (size_t i = 0; i < files.size() && langCodes.size() < 32; i++)
    {
        if (files[i].first == "en" || files[i].first == "pt")
            continue;
        langCodes.push_back(files[i].first);
        langNames.push_back(files[i].second);
    }
    langValues.clear();
    langLabels.clear();
    for (size_t i = 0; i < langCodes.size(); i++)
    {
        langValues.push_back(langCodes[i].c_str());
        langLabels.push_back(langNames[i].c_str());
    }
    api->choice(api, sec, "Language", L("Language", "Idioma"),
                L("Language of the mod's messages and of this page. Other languages are text files in the mod's lang folder.",
                  "Idioma das mensagens do mod e desta página. Outros idiomas são arquivos de texto na pasta lang do mod."),
                "auto", &langValues[0], &langLabels[0], (int)langValues.size(), MCM_RESTART);
    api->toggle(api, sec, "ContextMenu", L("Option in the right-click menu", "Opção no menu do botão direito"),
                L("Enslave/Free in the order menu (hold the right mouse button over the target).", "Escravizar/Libertar no menu de ordens (segure o botão direito sobre o alvo)."), 1, 0);
    api->toggle(api, sec, "ShowButton", L("Inventory button", "Botão no inventário"),
                L("Also shows the ENSLAVE/FREE button in the inventory window.", "Mostra também o botão ESCRAVIZAR/LIBERTAR na janela de inventário."), 0, 0);
    api->toggle(api, sec, "Enabled", L("Mod active", "Mod ativo"),
                L("Off: the mod leaves every character alone (no menu options, submission, escapes, rebellions or protection). Your own people enslaved by another faction are already left alone.",
                  "Desligado: o mod não mexe em ninguém (sem opções no menu, submissão, fugas, rebeliões ou proteção). Seus personagens escravizados por outra facção já são ignorados."), 1, 0);
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

    api->toggle(api, sec, "SkeletonThrall", L("Reprogram skeletons", "Reprogramar esqueletos"),
                L("Unconscious skeletons are not enslaved: they are reprogrammed into free members. Robotics decides the chance; a failure destroys them.",
                  "Esqueletos inconscientes não viram escravos: são reprogramados e viram membros livres. A robótica decide a chance; uma falha os destrói."), 0, 0);
    api->number(api, sec, "ThrallBaseChance", L("Reprogram: base chance (%)", "Reprogramar: chance base (%)"),
                L("Chance with robotics 0.", "Chance com robótica 0."), 20.0f, 0.0f, 100.0f, 0, 0);
    api->number(api, sec, "ThrallChancePerLevel", L("Reprogram: % per robotics level", "Reprogramar: % por nível de robótica"),
                NULL, 0.75f, 0.0f, 5.0f, 2, 0);
    api->number(api, sec, "ThrallMaxChance", L("Reprogram: maximum chance (%)", "Reprogramar: chance máxima (%)"),
                NULL, 95.0f, 0.0f, 100.0f, 0, 0);
    static const char* const thrallRaceValues[] = { "", "95870-Newwworld.mod" };
    std::string keepRace = T("Keep their race", "Manter a raça"), headless = T("Headless (Skeleton No-Head MkII)", "Sem cabeça (Skeleton No-Head MkII)");
    const char* thrallRaceLabels[] = { keepRace.c_str(), headless.c_str() };
    api->choice(api, sec, "ThrallRace", L("Reprogram: race", "Reprogramar: raça"),
                L("Race a reprogrammed skeleton becomes. The health panel updates after saving and loading.", "Raça que o esqueleto reprogramado passa a ter. O painel de saúde atualiza depois de salvar e carregar."), "", thrallRaceValues, thrallRaceLabels, 2, 0);

    api->section(api, L("Slaves", "Escravos"));
    api->toggle(api, sec, "GiveShackles", L("Shackles", "Grilhões"),
                L("Slaves get locked shackles; freeing removes them.", "O escravo ganha grilhões trancados; ao ser liberto eles somem."), 1, 0);
    api->toggle(api, sec, "ShacklesNoRunning", L("Shackles forbid running", "Grilhões impedem correr"),
                L("Slaves, runaways and rebels with shackles on the legs or body can only walk (a collar does not count).",
                  "Escravos, fugitivos e rebeldes com grilhões nas pernas ou no corpo só conseguem andar (coleira não conta)."), 1, 0);
    api->toggle(api, sec, "BanWeapons", L("Ban weapons", "Proibir armas"),
                L("Slaves drop any equipped weapon.", "Escravos largam qualquer arma equipada."), 1, 0);
    // SlaveEscort=0 in old ini files still turns it off; here it is the "off" choice
    static const char* const escortValues[] = { "bodyguard", "follow", "off" };
    std::string guardLabel = T("Follow and guard", "Seguir e proteger"), followLabel = T("Follow only", "Só seguir"),
                offLabel = T("Off (they stay with their squad)", "Desligado (ficam com o esquadrão)");
    const char* escortLabels[] = { guardLabel.c_str(), followLabel.c_str(), offLabel.c_str() };
    api->choice(api, sec, "EscortMode", L("Follow the enslaver", "Seguir quem escravizou"),
                L("What a new slave does: follow and guard whoever enslaved them, only follow, or nothing (they move with their squad, in formation).",
                  "O que o recém-escravizado faz: segue e protege quem o escravizou, só segue, ou nada (anda com o esquadrão, em formação)."),
                "bodyguard", escortValues, escortLabels, 3, 0);
    api->toggle(api, sec, "AnimalSlaves", L("Animals can be slaves", "Animais podem ser escravos"),
                L("Animals and creatures can be enslaved and sold. They never enslave, punish or free anyone themselves.",
                  "Animais e criaturas podem ser escravizados e vendidos. Eles mesmos nunca escravizam, punem nem libertam ninguém."), 0, 0);
    api->toggle(api, sec, "SlaveAIJob", L("Native slave work", "Trabalho escravo nativo"),
                L("Turns on the game's slave work behaviour. Experimental.", "Liga o comportamento de trabalho escravo do jogo. Experimental."), 0, 0);
    static const char* const foodValues[] = { "any", "own", "none" };
    std::string foodAny = T("Like anyone (from your storage too)", "Como qualquer um (também do seu estoque)"),
                foodOwn = T("Only food they carry", "Só a comida que carregam"),
                foodNone = T("Never (the game keeps them from starving)", "Nunca (o jogo não os deixa morrer de fome)");
    const char* foodLabels[] = { foodAny.c_str(), foodOwn.c_str(), foodNone.c_str() };
    api->choice(api, sec, "SlaveFood", L("Slaves eat", "Escravos comem"),
                L("Stops slaves raiding your food storage. With \"only food they carry\", put food in a slave's inventory to feed them.",
                  "Impede os escravos de saquear seu estoque de comida. Com \"só a comida que carregam\", ponha comida no inventário do escravo para alimentá-lo."),
                "any", foodValues, foodLabels, 3, 0);
    api->toggle(api, sec, "CageAtNight", L("Slaves sleep in cages", "Escravos dormem nas jaulas"),
                L("At night your slaves walk into free cages of your faction; in the morning they come out. A slave can also be sent with the order menu.",
                  "À noite seus escravos entram nas jaulas livres da sua facção; de manhã saem. Também dá para mandar um escravo pelo menu de ordens."), 0, 0);
    api->integer(api, sec, "CageNightHour", L("Into the cages at (hour)", "Entram nas jaulas às (hora)"), NULL, 21, 0, 23, 0);
    api->integer(api, sec, "CageMorningHour", L("Out of the cages at (hour)", "Saem das jaulas às (hora)"), NULL, 7, 0, 23, 0);
    api->toggle(api, sec, "RequireShackles", L("Enslaving needs a shackle kit", "Escravizar exige kit de grilhões"),
                L("Each enslavement uses up a shackle kit (or loose prisoner shackles) carried by the enslaver or a member nearby. Slave traders sell kits; freeing a slave gives one back.",
                  "Cada escravização gasta um kit de grilhões (ou grilhões de prisioneiro soltos) carregado por quem escraviza ou por um membro perto. Traficantes vendem kits; libertar um escravo devolve um."), 0, 0);
    api->integer(api, sec, "ShackleKitPrice", L("Shackle kit price", "Preço do kit de grilhões"), NULL, 150, 0, 5000, 0);
    static const char* const restraintValues[] = { "SHACKLES", "12-SlaverMod.mod" };
    std::string restraintShackles = T("Prisoner shackles", "Grilhões de prisioneiro");
    std::string restraintCollar = T("Slave collar on the neck (by Macropod)", "Coleira de escravo no pescoço (por Macropod)");
    const char* restraintLabels[] = { restraintShackles.c_str(), restraintCollar.c_str() };
    api->choice(api, sec, "ShackleItem", L("Restraint", "Restrição"),
                L("What new slaves get locked on. Any locked item counts as a restraint for SlaverMod; other item IDs can be set in SlaverMod.ini.",
                  "O que é trancado nos novos escravos. Qualquer item trancado conta como restrição para o SlaverMod; outros IDs de item podem ser postos no SlaverMod.ini."),
                "SHACKLES", restraintValues, restraintLabels, 2, 0);
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
    api->integer(api, sec, "FreeStayMinSubmission", L("Freed slaves stay from submission", "Libertado fica a partir da submissão"),
                 L("Freed slaves with this submission or more stay in your faction; the others go back to their own. 0 = everyone stays.",
                   "Libertados com esta submissão ou mais ficam na sua facção; os outros voltam para a deles. 0 = todos ficam."), 90, 0, 101, 0);
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
    static const char* offValues[] = { "skill", "always", "never" };
    static std::string offLabels[3];
    static const char* offLabelPtrs[3];
    offLabels[0] = T("Strong or skilled only", "Só os fortes ou habilidosos");
    offLabels[1] = T("Always", "Sempre");
    offLabels[2] = T("Never", "Nunca");
    for (int i = 0; i < 3; i++)
        offLabelPtrs[i] = offLabels[i].c_str();
    api->choice(api, sec, "ShacklesComeOff", L("Runaways and rebels get out of shackles", "Fugitivos e rebeldes se livram dos grilhões"),
                L("Strong or skilled only: those with the strength to break them or the lockpicking to open them. The others keep them, and leg shackles keep them walking.",
                  "Só os fortes ou habilidosos: quem tem força para quebrar ou arrombamento para abrir. Os outros ficam com eles, e grilhões nas pernas os mantêm andando."),
                "skill", offValues, offLabelPtrs, 3, 0);
    api->integer(api, sec, "ShackleBreakStrength", L("Strength to break shackles", "Força para quebrar grilhões"), NULL, 50, 1, 100, 0);
    api->integer(api, sec, "ShackleBreakLockpicking", L("Lockpicking to open shackles", "Arrombamento para abrir grilhões"), NULL, 40, 1, 100, 0);
    api->text(api, sec, "EscapeFaction", L("Runaway faction if origin unknown (ID)", "Facção do fugitivo sem origem (ID)"), NULL, "51707-rebirth.mod", 0);

    api->section(api, L("Punishment and reward", "Punição e recompensa"));
    api->toggle(api, sec, "PunishEnabled", L("Punish", "Punir"),
                L("Punish option on your slaves: submission, but it hurts.", "Opção Punir nos seus escravos: dá submissão, mas machuca."), 1, 0);
    api->integer(api, sec, "PunishSubmission", L("Submission per punishment", "Submissão por punição"),
                 L("Halved, then a third... for each punishment within the cooldown.", "Cai pela metade, depois um terço... a cada punição dentro do intervalo."), 15, 0, 100, 0);
    api->integer(api, sec, "PunishDamage", L("Punishment damage", "Dano da punição"),
                 L("Blunt damage on a limb; grows with repeated punishments.", "Dano de impacto num membro; cresce com punições repetidas."), 10, 0, 100, 0);
    api->number(api, sec, "PunishCooldownDays", L("Punishment interval (days)", "Intervalo entre punições (dias)"), NULL, 1.0f, 0.0f, 10.0f, 1, 0);
    api->toggle(api, sec, "Speech", L("Speech bubbles", "Falas"),
                L("Slavers and slaves say a line when enslaving, punishing, rewarding, freeing, trading, escaping and rebelling (SlaverMod_lines.ini).",
                  "Escravistas e escravos falam ao escravizar, punir, recompensar, libertar, negociar, fugir e se rebelar (SlaverMod_lines.ini)."), 1, 0);
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

    api->section(api, L("Slave trade", "Comércio de escravos"));
    api->toggle(api, sec, "TradeWindow", L("SlaverMod trade window", "Janela de comércio do SlaverMod"),
                L("Replaces the game's buy / sell slave windows at slave traders.", "Substitui as janelas de compra e venda de escravos dos traficantes."), 1, 0);
    api->toggle(api, sec, "SlaveCaravan", L("Slave caravan", "Caravana de escravos"),
                L("The Slave Traders send a caravan to your base to buy and sell slaves. A message warns you a day before.",
                  "Os Traficantes de Escravos mandam uma caravana para a sua base para comprar e vender escravos. Uma mensagem avisa um dia antes."), 1, 0);
    api->number(api, sec, "SlaveCaravanDays", L("Slave caravan: days between visits", "Caravana de escravos: dias entre visitas"),
                L("Counted from when the last caravan leaves. A new value counts from the next visit.",
                  "Contados a partir de quando a última caravana vai embora. Um valor novo vale a partir da próxima visita."), 7.0f, 0.5f, 60.0f, 1, 0);
    api->number(api, sec, "SlaveCaravanStayHours", L("Slave caravan: hours at your base", "Caravana de escravos: horas na sua base"),
                L("Game hours the caravan stays after it arrives, then it leaves.", "Horas de jogo que a caravana fica depois de chegar; depois ela vai embora."), 6.0f, 1.0f, 96.0f, 0, 0);
    api->toggle(api, sec, "ProtectSlaves", L("Other slavers leave your slaves alone", "Outros escravistas deixam seus escravos em paz"),
                L("Slavers of other factions no longer try to process, cage or ship your slaves.", "Escravistas de outras facções deixam de tentar processar, prender ou levar seus escravos."), 1, 0);
    api->toggle(api, sec, "SparedByNeutrals", L("Allies and neutrals never attack your slaves", "Aliados e neutros nunca atacam seus escravos"),
                L("People and turrets of factions not at war with you never take your slaves for enemies, whatever their race or old faction.",
                  "Pessoas e torretas de facções que não estão em guerra com você nunca tratam seus escravos como inimigos, seja qual for a raça ou a antiga facção deles."), 1, 0);
    api->toggle(api, sec, "ClearSlaveBounties", L("Slaves lose their bounties", "Escravos perdem as recompensas"),
                L("Bounties on a new slave are cleared, so guards and bounty hunters stop hunting them.",
                  "As recompensas pela cabeça de um novo escravo são apagadas, então guardas e caçadores de recompensa param de persegui-lo."), 1, 0);
    api->toggle(api, sec, "AllowStealingSlaves", L("Allow stealing slaves", "Permitir roubar escravos"),
                L("Lets you enslave other factions' slaves. Off: only runaways and free people.", "Deixa escravizar os escravos de outras facções. Desligado: só fugitivos e pessoas livres."), 0, 0);
    api->toggle(api, sec, "DisableCarriedSale", L("No fixed 400 cats offer", "Sem a oferta fixa de 400 cats"),
                L("Traders no longer offer 400 cats for someone you carry: enslave them and sell them in the window.",
                  "Os traficantes deixam de oferecer 400 cats por quem você carrega: escravize e venda pela janela."), 1, 0);
    api->integer(api, sec, "TradeBaseValue", L("Base value", "Valor base"), L("Worth of any slave before stats.", "Quanto vale qualquer escravo antes dos atributos."), 300, 0, 10000, 0);
    api->number(api, sec, "TradeStatValue", L("Value per stat point", "Valor por ponto de atributo"),
                L("Strength, toughness, dexterity, melee attack, melee defence and labouring.", "Força, robustez, destreza, ataque e defesa corpo a corpo e trabalho braçal."), 12.0f, 0.0f, 200.0f, 1, 0);
    api->number(api, sec, "TradeBuyMult", L("Buying price multiplier", "Multiplicador do preço de compra"), NULL, 2.0f, 0.1f, 10.0f, 2, 0);
    api->number(api, sec, "TradeSellMult", L("Selling price multiplier", "Multiplicador do preço de venda"), NULL, 1.0f, 0.1f, 10.0f, 2, 0);
    api->number(api, sec, "TradeMissingLimbPenalty", L("Price lost per missing limb", "Preço perdido por membro faltando"),
                L("Per arm or leg that is gone or crushed (0.25 = -25%). Robotic limbs count as limbs.",
                  "Por braço ou perna perdido ou esmagado (0.25 = -25%). Membros robóticos contam como membros."), 0.25f, 0.0f, 1.0f, 2, 0);
    api->integer(api, sec, "TradeBoughtBonus", L("Submission bonus when bought", "Bônus de submissão ao comprar"),
                 L("Bought slaves are already broken in: race start + this.", "Escravos comprados já vêm domados: inicial da raça + isto."), 15, 0, 100, 0);

    api->section(api, L("Relations", "Relações"));
    api->number(api, sec, "RelationLoss", L("Loss when enslaving", "Perda ao escravizar"),
                L("How much the victim's faction relation drops.", "Quanto a relação com a facção da vítima cai."), 5.0f, 0.0f, 50.0f, 1, 0);
    api->number(api, sec, "RelationGain", L("Gain when freeing", "Ganho ao libertar"),
                L("How much the original faction relation rises. Must be lower than the loss.", "Quanto a relação com a facção de origem sobe. Precisa ser menor que a perda."), 2.0f, 0.0f, 50.0f, 1, 0);
    api->toggle(api, sec, "EnslaveIsCrime", L("Enslaving is a crime", "Escravizar é crime"),
                L("Whoever sees you putting shackles on someone treats it as kidnapping, like carrying them away.",
                  "Quem vê você colocando grilhões em alguém trata como sequestro, igual a carregar a pessoa."), 1, 0);

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
    api->toggle(api, sec, "RebelsFlee", L("Rebels flee", "Rebeldes fogem"),
                L("Once nobody is left to fight (or without war), the rebels run away towards a town instead of staying around.",
                  "Quando não sobra ninguém para lutar (ou sem guerra), os rebeldes fogem para uma cidade em vez de ficar por perto."), 1, 0);
    api->toggle(api, sec, "RebelsDistrust", L("Rebels never trust you again", "Rebeldes nunca mais confiam em você"),
                L("Slaves who rose up against you refuse to join you through dialogue. Enslaving them again still works.",
                  "Escravos que se rebelaram contra você recusam entrar no seu grupo por diálogo. Escravizar de novo ainda funciona."), 1, 0);
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
    api->hotkey(api, sec, "KnockoutTestKey", L("Hotkey: knock out", "Atalho: nocautear"),
                L("Debug mode only. The selected character falls unconscious.", "Só no modo debug. O personagem selecionado fica inconsciente."), "CTRL+SHIFT+K", 0);
    api->hotkey(api, sec, "CarryTestKey", L("Hotkey: carry test", "Atalho: teste de carregar"),
                L("Debug mode only. The nearest NPC of another faction knocks out and carries away the target being enslaved (or the selected NPC).",
                  "Só no modo debug. O NPC de outra facção mais próximo nocauteia e leva embora o alvo sendo escravizado (ou o NPC selecionado)."), "CTRL+SHIFT+C", 0);
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
    LoadLines();
    ParseHotkey();
    srand(GetTickCount());
    Trace("startPlugin v1.18");

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&GameWorld::_NV_mainLoop_GPUSensitiveStuff), GameWorld_update_hook, (void**)&GameWorld_update_orig))
        ErrorLog("SlaverMod: could not add GameWorld update hook!");

    FindMenuFlags();
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&ContextMenuGUI::show), ContextMenuGUI_show_hook, (void**)&ContextMenuGUI_show_orig))
        ErrorLog("SlaverMod: could not add context menu hook!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&SaveManager::saveGame), SaveManager_saveGame_hook, (void**)&SaveManager_saveGame_orig))
        ErrorLog("SlaverMod: could not add save hook!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&SaveManager::loadGame), SaveManager_loadGame_hook, (void**)&SaveManager_loadGame_orig))
        ErrorLog("SlaverMod: could not add load hook!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&Dialogue::_doActions), Dialogue_doActions_hook, (void**)&Dialogue_doActions_orig))
        ErrorLog("SlaverMod: could not add dialogue hook!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&AI::findNewSlaveOrPrisonerToProcess), AI_findNewSlaveOrPrisonerToProcess_hook, (void**)&AI_findNewSlaveOrPrisonerToProcess_orig))
        ErrorLog("SlaverMod: could not add AI hook findNewSlaveOrPrisonerToProcess!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&AI::findBadSlaves), AI_findBadSlaves_hook, (void**)&AI_findBadSlaves_orig))
        ErrorLog("SlaverMod: could not add AI hook findBadSlaves!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&AI::findRandomOneOfMySlaves), AI_findRandomOneOfMySlaves_hook, (void**)&AI_findRandomOneOfMySlaves_orig))
        ErrorLog("SlaverMod: could not add AI hook findRandomOneOfMySlaves!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&AI::findConsciousSlaveInTown), AI_findConsciousSlaveInTown_hook, (void**)&AI_findConsciousSlaveInTown_orig))
        ErrorLog("SlaverMod: could not add AI hook findConsciousSlaveInTown!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&AI::findConsciousSlaveInTownForShipping), AI_findConsciousSlaveInTownForShipping_hook, (void**)&AI_findConsciousSlaveInTownForShipping_orig))
        ErrorLog("SlaverMod: could not add AI hook findConsciousSlaveInTownForShipping!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&AI::findConsciousSlaveInTown_unholy), AI_findConsciousSlaveInTown_unholy_hook, (void**)&AI_findConsciousSlaveInTown_unholy_orig))
        ErrorLog("SlaverMod: could not add AI hook findConsciousSlaveInTown_unholy!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&AI::findKOedSlaveOfMine), AI_findKOedSlaveOfMine_hook, (void**)&AI_findKOedSlaveOfMine_orig))
        ErrorLog("SlaverMod: could not add AI hook findKOedSlaveOfMine!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&AI::findSomeoneWhoWouldMakeAGoodSlave), AI_findSomeoneWhoWouldMakeAGoodSlave_hook, (void**)&AI_findSomeoneWhoWouldMakeAGoodSlave_orig))
        ErrorLog("SlaverMod: could not add AI hook findSomeoneWhoWouldMakeAGoodSlave!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&Character::addOrder), Character_addOrder_hook, (void**)&Character_addOrder_orig))
        ErrorLog("SlaverMod: could not add hook Character::addOrder!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&AI::_findWoundedAllies), AI__findWoundedAllies_hook, (void**)&AI__findWoundedAllies_orig))
        ErrorLog("SlaverMod: could not add AI hook _findWoundedAllies!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&DialogLineData::checkConditions), DialogLineData_checkConditions_hook, (void**)&DialogLineData_checkConditions_orig))
        ErrorLog("SlaverMod: could not add dialogue condition hook!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&SaveManager::newGame), SaveManager_newGame_hook, (void**)&SaveManager_newGame_orig))
        ErrorLog("SlaverMod: could not add new game hook!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&AbstractMovementBase::_NV_setDesiredSpeed), Move_setDesiredSpeed_hook, (void**)&Move_setDesiredSpeed_orig))
        ErrorLog("SlaverMod: could not add speed hook 1!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(static_cast<void (AbstractMovementBase::*)(float)>(&AbstractMovementBase::setDesiredSpeed)), Move_setDesiredSpeedF_hook, (void**)&Move_setDesiredSpeedF_orig))
        ErrorLog("SlaverMod: could not add speed hook 2!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(static_cast<void (AbstractMovementBase::*)(MoveSpeed)>(&AbstractMovementBase::setDesiredSpeedOrders)), Move_setDesiredSpeedOrders_hook, (void**)&Move_setDesiredSpeedOrders_orig))
        ErrorLog("SlaverMod: could not add speed hook 3!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(static_cast<void (AbstractMovementBase::*)(float)>(&AbstractMovementBase::setDesiredSpeedOrders)), Move_setDesiredSpeedOrdersF_hook, (void**)&Move_setDesiredSpeedOrdersF_orig))
        ErrorLog("SlaverMod: could not add speed hook 4!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&TitleScreen::_NV_update), TitleScreen_update_hook, (void**)&TitleScreen_update_orig))
        ErrorLog("SlaverMod: could not add title screen hook!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&NewGameWindow::setVisible), NewGameWindow_setVisible_hook, (void**)&NewGameWindow_setVisible_orig))
        ErrorLog("SlaverMod: could not add new game window hook!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&Character::_NV_isEnemy), Character_isEnemy_hook, (void**)&Character_isEnemy_orig))
        ErrorLog("SlaverMod: could not add isEnemy hook!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&GunClassTurret::_NV_scoreTarget), GunClassTurret_scoreTarget_hook, (void**)&GunClassTurret_scoreTarget_orig))
        ErrorLog("SlaverMod: could not add turret score hook!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&Character::eatItem), Character_eatItem_hook, (void**)&Character_eatItem_orig))
        ErrorLog("SlaverMod: could not add eatItem hook!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&AI::_NV_scoreGetFood_inHomeTown), AI_scoreGetFood_hook, (void**)&AI_scoreGetFood_orig))
        ErrorLog("SlaverMod: could not add get-food hook!");
}
