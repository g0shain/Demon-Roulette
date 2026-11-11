#pragma once
// Данные забегов: структуры, глобальное состояние, сохранение и загрузка

#include "core/Utils.hpp"

#include <set>
#include <string>
#include <vector>

// ---------- категории уровней ----------
struct Cat {
    const char* key;    // ключ настройки (значения по умолчанию)
    const char* name;
    const char* diff;   // значение diff для сервера
    int demon;          // фильтр демонов (0 = не демон)
};
inline constexpr Cat CATS[] = {
    {"lvl-auto",   "Auto",          "-3", 0},
    {"lvl-easy",   "Easy",          "1",  0},
    {"lvl-normal", "Normal",        "2",  0},
    {"lvl-hard",   "Hard",          "3",  0},
    {"lvl-harder", "Harder",        "4",  0},
    {"lvl-insane", "Insane",        "5",  0},
    {"demon-easy",    "Easy Demon",    "-2", 1},
    {"demon-medium",  "Medium Demon",  "-2", 2},
    {"demon-hard",    "Hard Demon",    "-2", 3},
    {"demon-insane",  "Insane Demon",  "-2", 4},
    {"demon-extreme", "Extreme Demon", "-2", 5},
};
inline constexpr int NCATS = sizeof(CATS) / sizeof(CATS[0]);


// ---------- данные забегов ----------
struct Entry {
    int id = 0;
    std::string name, diff;
    int required = 0, got = 0;
    std::string state;   // passed / skipped / failed
};

struct RunData {
    int uid = 0;
    std::string name;
    long long created = 0;
    bool finished = false;
    std::string result;
    unsigned cats = 0;
    bool fixedStep = false;
    bool skipAdds = true;
    bool infSkips = false;
    int skipLimit = 0, skipsUsed = 0;
    int required = 1, best = 0, passed = 0;
    int levelID = 0;
    std::string levelName, diffName;
    bool awaiting = false;
    std::set<int> seen;
    std::vector<Entry> levels;
};

enum class PassKind { None, Next, Complete };

// Все забеги (от старых к новым) и состояние, общее для разных частей мода
extern std::vector<RunData> g_runs;
extern int g_activeUid;
extern int g_nextNum;
extern int g_nextUid;
extern int g_pendingUid;           // игрок вышел из уровня, нужно проверить результат
extern bool g_leaveThenNext;       // «Next level» нажат внутри уровня
extern bool g_nextInfoByUs;        // следующая страница уровня открыта нашим модом
extern bool g_openingRunBrowser;   // открываем список уровней забега
extern int g_openingRunUid;
extern PassKind g_endKind;         // окно, которое нужно показать на экране прохождения
extern int g_endUid;

RunData* findRun(int uid);
RunData* activeRun();
RunData* runForLevel(int levelID);   // забег, к которому относится уровень (активный в приоритете)
void saveAll();
void loadAll();
bool canSkip(RunData const& r);
