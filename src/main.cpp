// Demon Roulette by g0shain
#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/LevelBrowserLayer.hpp>
#include <Geode/modify/EndLevelLayer.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/TextInput.hpp>

#include <algorithm>
#include <ctime>
#include <map>
#include <random>
#include <set>

using namespace geode::prelude;

// ======================= утилиты =======================

template <class T>
static int idOf(T& v) {
    if constexpr (requires { v.value(); }) return static_cast<int>(v.value());
    else return static_cast<int>(v);
}

static std::mt19937& rng() {
    static std::mt19937 r{std::random_device{}()};
    return r;
}
static int randInt(int a, int b) { return std::uniform_int_distribution<int>(a, b)(rng()); }

static std::string joinStr(std::vector<std::string> const& v, char sep) {
    std::string s;
    for (size_t i = 0; i < v.size(); i++) { if (i) s += sep; s += v[i]; }
    return s;
}
static std::vector<std::string> splitKeep(std::string const& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) { out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    out.push_back(cur);
    return out;
}
static std::string clean(std::string s) {
    for (auto& c : s) if (c == '|' || c == '\n' || c == '\r') c = ' ';
    return s;
}
static int toInt(std::string const& s) { return std::atoi(s.c_str()); }

// ======================= категории уровней =======================

struct Cat {
    const char* key;    // ключ настройки (значения по умолчанию)
    const char* name;
    const char* diff;   // значение diff для сервера
    int demon;          // фильтр демонов (0 = не демон)
};
static const Cat CATS[] = {
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
static constexpr int NCATS = sizeof(CATS) / sizeof(CATS[0]);

// ======================= данные забегов =======================

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

static std::vector<RunData> g_runs;
static int g_activeUid = 0;
static int g_nextNum = 1;
static int g_nextUid = 1;
static int g_pendingUid = 0;          // игрок вышел из уровня, нужно проверить результат
static bool g_leaveThenNext = false;  // «Next level» нажат внутри уровня
static bool g_nextInfoByUs = false;   // следующая страница уровня открыта нашим модом
static bool g_openingRunBrowser = false;  // открываем список уровней забега
static int g_openingRunUid = 0;

enum class PassKind { None, Next, Complete };
static PassKind g_endKind = PassKind::None;   // окно, которое нужно показать на экране прохождения
static int g_endUid = 0;

static RunData* findRun(int uid) {
    for (auto& r : g_runs) if (r.uid == uid) return &r;
    return nullptr;
}
static RunData* activeRun() { return g_activeUid ? findRun(g_activeUid) : nullptr; }

// Забег, к которому относится уровень (активный в приоритете)
static RunData* runForLevel(int levelID) {
    if (auto a = activeRun()) {
        if (a->levelID == levelID) return a;
    }
    for (auto& r : g_runs) {
        if (r.levelID == levelID) return &r;
        for (auto& e : r.levels) if (e.id == levelID) return &r;
    }
    return nullptr;
}

static void saveAll() {
    std::string out;
    out += joinStr({"M", std::to_string(g_nextNum), std::to_string(g_nextUid), std::to_string(g_activeUid)}, '|') + "\n";
    for (auto& r : g_runs) {
        std::string seen;
        for (int id : r.seen) { if (!seen.empty()) seen += ','; seen += std::to_string(id); }
        out += joinStr({
            "R", std::to_string(r.uid), clean(r.name), std::to_string(r.created),
            r.finished ? "1" : "0", clean(r.result), std::to_string(r.cats),
            r.fixedStep ? "1" : "0", r.skipAdds ? "1" : "0", r.infSkips ? "1" : "0",
            std::to_string(r.skipLimit), std::to_string(r.skipsUsed),
            std::to_string(r.required), std::to_string(r.best), std::to_string(r.passed),
            std::to_string(r.levelID), clean(r.levelName), clean(r.diffName),
            r.awaiting ? "1" : "0", seen
        }, '|') + "\n";
        for (auto& e : r.levels) {
            out += joinStr({
                "L", std::to_string(e.id), clean(e.name), clean(e.diff),
                std::to_string(e.required), std::to_string(e.got), clean(e.state)
            }, '|') + "\n";
        }
    }
    Mod::get()->setSavedValue<std::string>("runs-data", out);
}

static void loadAll() {
    g_runs.clear();
    auto text = Mod::get()->getSavedValue<std::string>("runs-data", "");
    for (auto& line : splitKeep(text, '\n')) {
        if (line.empty()) continue;
        auto f = splitKeep(line, '|');
        if (f[0] == "M" && f.size() >= 4) {
            g_nextNum = std::max(1, toInt(f[1]));
            g_nextUid = std::max(1, toInt(f[2]));
            g_activeUid = toInt(f[3]);
        } else if (f[0] == "R" && f.size() >= 20) {
            RunData r;
            r.uid = toInt(f[1]);
            r.name = f[2];
            r.created = std::strtoll(f[3].c_str(), nullptr, 10);
            r.finished = f[4] == "1";
            r.result = f[5];
            r.cats = static_cast<unsigned>(std::strtoul(f[6].c_str(), nullptr, 10));
            r.fixedStep = f[7] == "1";
            r.skipAdds = f[8] == "1";
            r.infSkips = f[9] == "1";
            r.skipLimit = toInt(f[10]);
            r.skipsUsed = toInt(f[11]);
            r.required = toInt(f[12]);
            r.best = toInt(f[13]);
            r.passed = toInt(f[14]);
            r.levelID = toInt(f[15]);
            r.levelName = f[16];
            r.diffName = f[17];
            r.awaiting = f[18] == "1";
            for (auto& s : splitKeep(f[19], ',')) if (!s.empty()) r.seen.insert(toInt(s));
            g_runs.push_back(std::move(r));
        } else if (f[0] == "L" && f.size() >= 7 && !g_runs.empty()) {
            Entry e;
            e.id = toInt(f[1]);
            e.name = f[2];
            e.diff = f[3];
            e.required = toInt(f[4]);
            e.got = toInt(f[5]);
            e.state = f[6];
            g_runs.back().levels.push_back(std::move(e));
        }
    }
    if (!findRun(g_activeUid)) g_activeUid = 0;
}

$on_mod(Loaded) { loadAll(); }

static bool canSkip(RunData const& r) {
    return !r.finished && !r.awaiting && r.levelID != 0 && (r.infSkips || r.skipsUsed < r.skipLimit);
}

// ======================= открытие уровня =======================

template <class T>
static int sceneDepth(T* d) {
    if constexpr (requires { d->m_pobScenesStack->count(); }) return static_cast<int>(d->m_pobScenesStack->count());
    else return 0;
}

// Показывает сцену так, чтобы в стеке остались только главное меню и она сама.
// Тогда стек не растёт, а Esc идёт по цепочке:
// страница уровня -> список уровней забега -> список забегов -> главное меню
static void showScene(CCScene* scene) {
    auto dir = CCDirector::get();
    int depth = sceneDepth(dir);
    if (depth >= 2) {
        for (int i = depth; i > 2; --i) dir->popScene();
        dir->replaceScene(scene);
    } else if (depth == 1) {
        dir->pushScene(scene);
    } else {
        dir->replaceScene(scene);
    }
}

static void openLevelScene(GJGameLevel* level) {
    g_nextInfoByUs = true;   // страница будет «нашей»: Esc с неё ведёт в список уровней забега
    auto scene = LevelInfoLayer::scene(level, false);
    g_nextInfoByUs = false;
    showScene(CCTransitionFade::create(0.5f, scene));
}

// Открывает уровень, дождавшись окончания текущего перехода между сценами
// (иначе replaceScene во время перехода теряется). Сам освобождает level.
static void openLevelSafe(GJGameLevel* level, int tries = 0) {
    auto scene = CCDirector::get()->getRunningScene();
    if (scene && typeinfo_cast<CCTransitionScene*>(scene) && tries < 180) {
        Loader::get()->queueInMainThread([level, tries] { openLevelSafe(level, tries + 1); });
        return;
    }
    openLevelScene(level);
    level->release();
}

// Значение фильтра демонов у GJSearchObject: Easy 7, Medium 8, Hard 6 (базовый Demon), Insane 9, Extreme 10
static int demonFilterValue(int demon) {
    switch (demon) {
        case 1: return 7;
        case 2: return 8;
        case 3: return 6;
        case 4: return 9;
        default: return 10;
    }
}

// Звёзды рейтингового уровня
template <class T>
static int starsOf(T* l) {
    if constexpr (requires { idOf(l->m_stars); }) return idOf(l->m_stars);
    else return -1;
}
// Категория -> звёзды: Auto 1, Easy 2, Normal 3, Hard 4-5, Harder 6-7, Insane 8-9, Demon 10
// (сервер смешивает Easy+Auto и Insane+Demon, поэтому проверяем по звёздам)
static bool starsMatchCat(int cat, int stars) {
    switch (cat) {
        case 0: return stars == 1;
        case 1: return stars == 2;
        case 2: return stars == 3;
        case 3: return stars == 4 || stars == 5;
        case 4: return stars == 6 || stars == 7;
        case 5: return stars == 8 || stars == 9;
        default: return stars == 10;
    }
}

// Код сложности демона в данных уровня: 3 easy, 4 medium, 0 hard, 5 insane, 6 extreme
static int demonCodeFor(int demon) {
    switch (demon) {
        case 1: return 3;
        case 2: return 4;
        case 3: return 0;
        case 4: return 5;
        default: return 6;
    }
}
template <class T>
static int demonDiffOf(T* l) {
    if constexpr (requires { static_cast<int>(l->m_demonDifficulty); }) return static_cast<int>(l->m_demonDifficulty);
    else return -2;   // поля нет — проверку пропускаем
}

// ======================= получение случайного уровня =======================

class Fetcher : public CCObject, public LevelManagerDelegate {
public:
    static Fetcher* get() {
        static Fetcher* inst = new Fetcher();
        return inst;
    }

    void beginRoll(int uid) {
        auto r = findRun(uid);
        if (!r) return;
        m_kind = Kind::Roll;
        m_uid = uid;
        r->levelID = 0;
        r->levelName.clear();
        r->best = 0;
        r->awaiting = false;
        g_activeUid = uid;
        saveAll();
        m_attempts = 0;
        m_waiting = false;
        Notification::create("Rolling a level...", NotificationIcon::Loading, 1.5f)->show();
        requestRoll();
    }

    void beginOpen(int levelId) {
        m_kind = Kind::Open;
        m_openId = levelId;
        m_waiting = true;
        Notification::create("Opening level...", NotificationIcon::Loading, 1.5f)->show();
        auto obj = GJSearchObject::create(SearchType::Search, std::to_string(levelId));
        auto glm = GameLevelManager::sharedState();
        glm->m_levelManagerDelegate = this;
        glm->getOnlineLevels(obj);
    }

private:
    enum class Kind { Roll, Open };
    Kind m_kind = Kind::Roll;
    int m_uid = 0;
    int m_openId = 0;
    bool m_waiting = false;
    int m_attempts = 0;
    int m_cat = 0;
    int m_page = 0;
    std::map<int, int> m_hiPage;

    void requestRoll() {
        auto r = findRun(m_uid);
        if (!r) return;
        std::vector<int> en;
        for (int i = 0; i < NCATS; i++) if (r->cats & (1u << i)) en.push_back(i);
        if (en.empty()) {
            Notification::create("This run has no difficulties enabled", NotificationIcon::Error)->show();
            return;
        }
        m_cat = en[randInt(0, static_cast<int>(en.size()) - 1)];
        auto const& c = CATS[m_cat];
        int& hi = m_hiPage.try_emplace(m_cat, 1200).first->second;
        m_page = randInt(0, hi);

        auto obj = GJSearchObject::create(SearchType::MostLiked);
        obj->m_difficulty = c.diff;
        if (c.demon) obj->m_demonFilter = static_cast<GJDifficulty>(demonFilterValue(c.demon));
        obj->m_starFilter = true;
        obj->m_page = m_page;

        m_waiting = true;
        auto glm = GameLevelManager::sharedState();
        glm->m_levelManagerDelegate = this;
        glm->getOnlineLevels(obj);
    }

    void retryOrFail() {
        m_waiting = false;
        if (++m_attempts > 25) {
            GameLevelManager::sharedState()->m_levelManagerDelegate = nullptr;
            m_hiPage.clear();
            Notification::create("Could not load a level. Press Continue in the run to retry.", NotificationIcon::Error)->show();
            return;
        }
        Loader::get()->queueInMainThread([this] { requestRoll(); });
    }

    void shrinkRange() {
        int& hi = m_hiPage[m_cat];
        hi = m_page > 0 ? m_page - 1 : 0;
    }

    void loadLevelsFinished(CCArray* levels, char const*) override {
        if (!m_waiting) return;

        if (m_kind == Kind::Open) {
            m_waiting = false;
            GameLevelManager::sharedState()->m_levelManagerDelegate = nullptr;
            GJGameLevel* found = nullptr;
            if (levels) {
                for (auto lvl : CCArrayExt<GJGameLevel*>(levels)) {
                    if (!found) found = lvl;
                    if (idOf(lvl->m_levelID) == m_openId) { found = lvl; break; }
                }
            }
            if (!found) {
                Notification::create("Level not found", NotificationIcon::Error)->show();
                return;
            }
            found->retain();
            Loader::get()->queueInMainThread([found] { openLevelSafe(found); });
            return;
        }

        auto r = findRun(m_uid);
        if (!r) { m_waiting = false; GameLevelManager::sharedState()->m_levelManagerDelegate = nullptr; return; }
        if (!levels || levels->count() == 0) {
            shrinkRange();
            retryOrFail();
            return;
        }
        std::vector<GJGameLevel*> candidates, exact;
        int wanted = CATS[m_cat].demon ? demonCodeFor(CATS[m_cat].demon) : -1;
        for (auto lvl : CCArrayExt<GJGameLevel*>(levels)) {
            if (lvl->m_levelLength == 5) continue;                 // платформеры не подходят
            if (r->seen.count(idOf(lvl->m_levelID))) continue;     // повторы внутри забега
            int stars = starsOf(lvl);
            if (stars >= 0 && !starsMatchCat(m_cat, stars)) continue;   // не та сложность
            candidates.push_back(lvl);
            if (wanted >= 0 && demonDiffOf(lvl) == wanted) exact.push_back(lvl);
        }
        if (candidates.empty()) {
            retryOrFail();
            return;
        }
        // подстраховка: проверяем сложность демона по данным уровня
        bool verify = wanted >= 0 && demonDiffOf(candidates[0]) != -2;
        if (verify && exact.empty() && m_attempts < 10) {
            retryOrFail();
            return;
        }
        auto& pool = (verify && !exact.empty()) ? exact : candidates;
        auto lvl = pool[randInt(0, static_cast<int>(pool.size()) - 1)];
        m_waiting = false;
        GameLevelManager::sharedState()->m_levelManagerDelegate = nullptr;
        int uid = m_uid;
        std::string diffName = CATS[m_cat].name;
        lvl->retain();
        Loader::get()->queueInMainThread([lvl, uid, diffName] {
            if (auto rr = findRun(uid)) {
                rr->levelID = idOf(lvl->m_levelID);
                rr->levelName = std::string(lvl->m_levelName);
                rr->diffName = diffName;
                rr->best = 0;
                rr->awaiting = false;
                rr->seen.insert(rr->levelID);
                g_activeUid = uid;
                saveAll();
                openLevelSafe(lvl);
            } else {
                lvl->release();
            }
        });
    }

    // Пустая страница сервер обычно отдаёт как «ошибку» — считаем это за конец списка
    void loadLevelsFailed(char const*) override {
        if (!m_waiting) return;
        if (m_kind == Kind::Open) {
            m_waiting = false;
            GameLevelManager::sharedState()->m_levelManagerDelegate = nullptr;
            Notification::create("Could not open the level", NotificationIcon::Error)->show();
            return;
        }
        shrinkRange();
        retryOrFail();
    }
};

// ======================= логика забега =======================

static void goNext(int uid) {
    auto r = findRun(uid);
    if (!r || r->finished) return;
    if (auto pl = PlayLayer::get()) {
        // сначала нормально выходим из уровня, дальше продолжит LevelInfoLayer::init
        g_activeUid = uid;
        g_leaveThenNext = true;
        pl->onQuit();
    } else {
        Fetcher::get()->beginRoll(uid);
    }
}

// Меняет состояние забега сразу в момент прохождения (даже если игрок выйдет до появления окна)
static PassKind markPassed(int uid) {
    auto r = findRun(uid);
    if (!r || r->finished || r->awaiting || r->levelID == 0) return PassKind::None;

    r->passed++;
    r->levels.push_back({r->levelID, r->levelName, r->diffName, r->required, r->best, "passed"});
    int next = r->fixedStep ? r->required + 1 : r->best + 1;

    if (next > 100) {
        r->finished = true;
        r->result = "completed";
        if (g_activeUid == uid) g_activeUid = 0;
        saveAll();
        return PassKind::Complete;
    }
    r->required = next;
    r->awaiting = true;
    saveAll();
    return PassKind::Next;
}

static void showPassPopup(int uid, PassKind kind) {
    auto r = findRun(uid);
    if (!r || kind == PassKind::None) return;
    if (kind == PassKind::Complete) {
        createQuickPopup("Demon Roulette",
            fmt::format("<cg>Run complete!</c> Levels passed: {}", r->passed),
            "OK", nullptr, [](FLAlertLayer*, bool) {});
        return;
    }
    int got = r->levels.empty() ? 0 : r->levels.back().got;
    createQuickPopup("Level passed!",
        fmt::format("<cg>{}%</c> reached.\nNext level needs at least <cy>{}%</c>.", got, r->required),
        "Later", "Next level",
        [uid](FLAlertLayer*, bool nx) {
            // «Later» и закрытие окна ничего не ломают — прогресс уже сохранён
            if (nx) goNext(uid);
        });
}

static void processPass(int uid) { showPassPopup(uid, markPassed(uid)); }

static void handleResult(int uid) {
    auto r = findRun(uid);
    if (!r || r->finished || r->awaiting || r->levelID == 0) return;
    if (r->best >= r->required) processPass(uid);
    else Notification::create(fmt::format("Best {}%, need {}%", r->best, r->required), NotificationIcon::Info)->show();
}

static void doSkip(int uid) {
    auto r = findRun(uid);
    if (!r || !canSkip(*r)) return;
    r->levels.push_back({r->levelID, r->levelName, r->diffName, r->required, r->best, "skipped"});
    r->skipsUsed++;
    if (r->skipAdds) r->required = std::min(100, r->required + 1);
    saveAll();
    Fetcher::get()->beginRoll(uid);
}

static void confirmSkip(int uid) {
    auto r = findRun(uid);
    if (!r || !canSkip(*r)) return;
    int newReq = r->skipAdds ? std::min(100, r->required + 1) : r->required;
    createQuickPopup("Skip level?",
        fmt::format("Skip <cy>{}</c>?\nThe next level will need <cg>{}%</c>.", r->levelName, newReq),
        "No", "Yes",
        [uid](FLAlertLayer*, bool yes) { if (yes) doSkip(uid); });
}

// Открывает НАСТОЯЩИЙ список уровней GD со всеми уровнями забега:
// сверху текущий, ниже пройденные и пропущенные (внизу уровень на 1%)
static void openRunLevels(int uid) {
    auto r = findRun(uid);
    if (!r) return;
    if (!r->finished && r->levelID == 0) {
        Fetcher::get()->beginRoll(uid);   // уровень ещё не выбран — выбираем
        return;
    }
    std::vector<std::string> ids;
    std::set<int> used;
    auto add = [&](int id) {
        if (id != 0 && used.insert(id).second) ids.push_back(std::to_string(id));
    };
    if (!r->finished && !r->awaiting) add(r->levelID);
    for (auto it = r->levels.rbegin(); it != r->levels.rend(); ++it) add(it->id);
    if (ids.empty()) {
        Notification::create("This run has no levels yet", NotificationIcon::Info)->show();
        return;
    }
    if (!r->finished) { g_activeUid = uid; saveAll(); }
    auto obj = GJSearchObject::create(SearchType::MapPackOnClick, joinStr(ids, ','));
    g_openingRunBrowser = true;
    g_openingRunUid = uid;
    auto browser = LevelBrowserLayer::scene(obj);
    g_openingRunBrowser = false;
    showScene(CCTransitionFade::create(0.5f, browser));
}

static void giveUpRun(int uid) {
    auto r = findRun(uid);
    if (!r || r->finished) return;
    if (!r->awaiting && r->levelID != 0)
        r->levels.push_back({r->levelID, r->levelName, r->diffName, r->required, r->best, "failed"});
    r->finished = true;
    r->result = "gave up";
    if (g_activeUid == uid) g_activeUid = 0;
    saveAll();
}

// Подтверждение; после сдачи показываем список уровней забега
static void confirmGiveUp(int uid) {
    auto r = findRun(uid);
    if (!r || r->finished) return;
    createQuickPopup("Give up?", "End this run? It stays in the list as finished.", "No", "Yes",
        [uid](FLAlertLayer*, bool yes) {
            if (!yes) return;
            giveUpRun(uid);
            openRunLevels(uid);
        });
}

// ======================= полоска «нужный процент» =======================

// Полоска в стиле стандартных полосок GD (та же текстура, что у Normal/Practice mode)
class NeedBar : public CCNode {
protected:
    CCSprite* m_fill = nullptr;
    CCLabelBMFont* m_label = nullptr;
    float m_bw = 0.f, m_bh = 0.f;

    bool init(float width) {
        if (!CCNode::init()) return false;
        auto bg = CCSprite::create("GJ_progressBar_001.png");
        m_fill = CCSprite::create("GJ_progressBar_001.png");
        if (!bg || !m_fill) return false;
        m_bw = bg->getContentWidth();
        m_bh = bg->getContentHeight();
        bg->setColor({0, 0, 0});
        bg->setOpacity(125);
        this->addChild(bg);
        // пропорции как у стандартной полоски GD
        m_fill->setAnchorPoint({0.f, 0.5f});
        m_fill->setScaleX(0.992f);
        m_fill->setScaleY(0.86f);
        m_fill->setPosition({-m_bw / 2 + 1.36f, 0.f});
        this->addChild(m_fill, 1);
        m_label = CCLabelBMFont::create("", "bigFont.fnt");
        m_label->setScale(0.5f);
        this->addChild(m_label, 2);
        this->setScale(width / m_bw);
        return true;
    }

public:
    static NeedBar* create(float width) {
        auto ret = new NeedBar();
        if (ret && ret->init(width)) { ret->autorelease(); return ret; }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    void set(float ratio, std::string const& text) {
        ratio = std::clamp(ratio, 0.f, 1.f);
        m_fill->setTextureRect(CCRect(0.f, 0.f, std::max(0.1f, m_bw * ratio), m_bh));
        m_fill->setColor(ccColor3B{0, 255, 0});
        m_label->setString(text.c_str());
    }
};

static void setBarFromRun(NeedBar* bar, RunData const& r) {
    if (r.awaiting) bar->set(1.f, "Passed!");
    else bar->set(r.required > 0 ? static_cast<float>(r.best) / r.required : 0.f,
                  fmt::format("{}% / {}%", r.best, r.required));
}

// Прячет полоску процентов практики (ищем по ID узлов) и возвращает её узел
static CCNode* hidePracticeBar(CCNode* root) {
    CCNode* first = nullptr;
    auto children = root->getChildren();
    if (!children) return nullptr;
    for (auto child : CCArrayExt<CCNode*>(children)) {
        std::string id = child->getID();
        if (id.find("practice") == std::string::npos) continue;
        if (id.find("button") != std::string::npos || id.find("btn") != std::string::npos ||
            id.find("toggle") != std::string::npos || id.find("menu") != std::string::npos) continue;
        bool barLike = id.find("bar") != std::string::npos;
        if (!(barLike || id.find("percent") != std::string::npos ||
              id.find("label") != std::string::npos || id.find("progress") != std::string::npos)) continue;
        child->setVisible(false);
        if (!first && barLike) first = child;
    }
    return first;
}

// Подпись над полоской, как «Normal Mode» у стандартной
static void addBarTitle(CCNode* parent, CCPoint barPos) {
    auto title = CCLabelBMFont::create("Demon Roulette", "bigFont.fnt");
    title->setScale(0.32f);
    title->setPosition({barPos.x, barPos.y + 14.f});
    parent->addChild(title, 50);
}

// Ставит полоску на место прятанной полоски практики (или в запасную позицию)
static NeedBar* attachBar(CCNode* host, RunData const& r, CCPoint fallback) {
    auto first = hidePracticeBar(host);
    float w = 180.f;
    if (first) {
        float fw = first->getContentWidth() * first->getScaleX();
        if (fw > 60.f) w = fw;
    }
    auto bar = NeedBar::create(w);
    if (!bar) return nullptr;
    setBarFromRun(bar, r);
    if (first && first->getParent()) {
        bar->setPosition(first->getPosition());
        first->getParent()->addChild(bar, 50);
        addBarTitle(first->getParent(), bar->getPosition());
    } else {
        bar->setPosition(fallback);
        host->addChild(bar, 50);
        addBarTitle(host, fallback);
    }
    return bar;
}

// У CCMenuItemSpriteExtra анимация нажатия возвращает кнопку к m_baseScale,
// поэтому после setScale его нужно выставить тоже
template <class T>
static void setBtnScale(T* btn, float scale) {
    btn->setScale(scale);
    if constexpr (requires { btn->m_baseScale = scale; }) btn->m_baseScale = scale;
}

// Прячет стандартную полоску прогресса уровня, если она есть в биндингах
template <class T>
static CCNode* hideVanillaBar(T* pl) {
    if constexpr (requires { static_cast<CCNode*>(pl->m_progressBar)->setVisible(false); }) {
        auto n = static_cast<CCNode*>(pl->m_progressBar);
        n->setVisible(false);
        return n;
    } else {
        return nullptr;
    }
}

// ======================= хуки уровня =======================

class $modify(RoulettePlay, PlayLayer) {
    struct Fields {
        NeedBar* bar = nullptr;
        bool prompting = false;
        bool pauseOnPrompt = false;
        PassKind pendingKind = PassKind::None;
        int pendingUid = 0;
    };

    RunData* tracked() {
        auto r = activeRun();
        if (!r || r->finished || r->awaiting || r->levelID == 0) return nullptr;
        if (idOf(m_level->m_levelID) != r->levelID) return nullptr;
        if (m_isPracticeMode || m_isTestMode) return nullptr;
        return r;
    }

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        auto r = activeRun();
        if (!r || r->finished || r->levelID == 0) return;
        if (idOf(m_level->m_levelID) != r->levelID) return;
        auto win = CCDirector::get()->getWinSize();
        CCNode* vanilla = hideVanillaBar(this);
        auto bar = NeedBar::create(220.f);
        if (!bar) return;
        setBarFromRun(bar, *r);
        if (vanilla && vanilla->getParent()) {
            // позиция и слой как у стандартной полоски, а размер свой (220), иначе выходит слишком большая
            bar->setPosition(vanilla->getPosition());
            vanilla->getParent()->addChild(bar, vanilla->getZOrder() + 1);
        } else {
            bar->setPosition({win.width / 2, win.height - 32.f});
            m_uiLayer->addChild(bar, 100);
        }
        m_fields->bar = bar;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (!m_fields->bar) return;
        auto r = activeRun();
        if (!r) return;
        if (r->awaiting) { m_fields->bar->set(1.f, "Passed!"); return; }
        int cur = this->getCurrentPercentInt();
        m_fields->bar->set(r->required > 0 ? static_cast<float>(cur) / r->required : 0.f,
                           fmt::format("{}% / need {}%", cur, r->required));
    }

    void record(RunData* r, int percent) {
        if (percent > r->best) { r->best = percent; saveAll(); }
    }

    void schedulePrompt(float delay, bool pause) {
        if (m_fields->prompting) return;
        m_fields->prompting = true;
        m_fields->pauseOnPrompt = pause;
        this->scheduleOnce(schedule_selector(RoulettePlay::prompt), delay);
    }

    // Окно «уровень пройден» прямо внутри уровня (состояние забега уже обновлено)
    void prompt(float) {
        m_fields->prompting = false;
        auto kind = m_fields->pendingKind;
        m_fields->pendingKind = PassKind::None;
        if (kind == PassKind::None) return;
        // пауза и курсор, чтобы можно было нажать кнопку в окне
        if (m_fields->pauseOnPrompt) this->pauseGame(false);
        PlatformToolbox::showCursor();
        showPassPopup(m_fields->pendingUid, kind);
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        auto r = tracked();
        if (r) record(r, this->getCurrentPercentInt());
        PlayLayer::destroyPlayer(player, obj);
        if (r && r->best >= r->required) {
            m_fields->pendingUid = r->uid;
            m_fields->pendingKind = markPassed(r->uid);
            schedulePrompt(0.8f, true);
        }
    }

    void levelComplete() {
        auto r = tracked();
        if (r) record(r, 100);
        PlayLayer::levelComplete();
        if (r && r->best >= r->required) {
            // состояние обновляем сразу, а окно покажет экран прохождения, когда появится
            g_endUid = r->uid;
            g_endKind = markPassed(r->uid);
        }
    }

    void onQuit() {
        // процент засчитывается только после смерти (или прохождения), а не при выходе
        if (auto r = tracked()) g_pendingUid = r->uid;
        PlayLayer::onQuit();
    }
};

class $modify(RoulettePause, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        auto pl = PlayLayer::get();
        auto r = activeRun();
        if (!pl || !r || r->finished || r->levelID == 0) return;
        if (idOf(pl->m_level->m_levelID) != r->levelID) return;
        auto win = CCDirector::get()->getWinSize();
        attachBar(this, *r, {win.width / 2, win.height - 22.f});
    }
};

// Окно «Level passed» над экраном прохождения: показываем после того, как он
// зарегистрировал нажатия, иначе его кнопки перехватывают клики
class $modify(RouletteEnd, EndLevelLayer) {
    void customSetup() {
        EndLevelLayer::customSetup();
        if (g_endKind != PassKind::None)
            this->scheduleOnce(schedule_selector(RouletteEnd::showPass), 1.2f);
    }

    void showPass(float) {
        auto kind = g_endKind;
        g_endKind = PassKind::None;
        PlatformToolbox::showCursor();
        showPassPopup(g_endUid, kind);
    }
};

class $modify(RouletteInfo, LevelInfoLayer) {
    struct Fields {
        bool byUs = false;
    };

    // Esc / стрелка назад: страница уровня -> список уровней забега
    bool leaveToRunLevels() {
        if (!m_fields->byUs) return false;
        auto r = runForLevel(idOf(m_level->m_levelID));
        if (!r) return false;
        openRunLevels(r->uid);
        return true;
    }

    void onBack(CCObject* sender) {
        if (leaveToRunLevels()) return;
        LevelInfoLayer::onBack(sender);
    }

    void keyBackClicked() {
        if (leaveToRunLevels()) return;
        LevelInfoLayer::keyBackClicked();
    }

    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) return false;
        m_fields->byUs = g_nextInfoByUs;
        g_nextInfoByUs = false;

        // «Next level» из уровня: продолжаем уже со страницы уровня
        if (g_leaveThenNext) {
            g_leaveThenNext = false;
            // даём закончиться переходу из уровня, потом бросаем следующий
            this->scheduleOnce(schedule_selector(RouletteInfo::autoNext), 0.9f);
            return true;
        }

        auto r = activeRun();
        if (!r || r->finished || r->levelID == 0) return true;
        if (idOf(level->m_levelID) != r->levelID) return true;

        auto win = CCDirector::get()->getWinSize();
        attachBar(this, *r, {win.width / 2, 20.f});

        // кнопка Skip / Next level прямо на странице уровня
        std::string label;
        int tag = 0;
        if (r->awaiting) { label = "Next level"; tag = 2; }
        else if (canSkip(*r)) {
            label = r->infSkips ? "Skip" : fmt::format("Skip ({})", r->skipLimit - r->skipsUsed);
            tag = 1;
        }
        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        float by = win.height - 62.f;
        auto addBtn = [&](std::string const& text, int t) {
            auto btn = CCMenuItemSpriteExtra::create(ButtonSprite::create(text.c_str()), this,
                menu_selector(RouletteInfo::onRouletteBtn));
            btn->setTag(t);
            setBtnScale(btn, 0.6f);
            float bw = btn->getScaledContentWidth();
            btn->setPosition({8.f + bw / 2, by});   // левый край прижат к краю экрана
            menu->addChild(btn);
            by -= 30.f;
        };
        if (tag) addBtn(label, tag);
        addBtn("Give up", 3);
        this->addChild(menu, 60);

        if (g_pendingUid == r->uid)
            this->scheduleOnce(schedule_selector(RouletteInfo::showResult), 0.6f);
        return true;
    }

    void autoNext(float) {
        if (auto r = activeRun()) {
            if (!r->finished) Fetcher::get()->beginRoll(r->uid);
        }
    }

    void showResult(float) {
        int uid = g_pendingUid;
        g_pendingUid = 0;
        handleResult(uid);
    }

    void onRouletteBtn(CCObject* sender) {
        int tag = static_cast<CCNode*>(sender)->getTag();
        if (tag == 1) confirmSkip(g_activeUid);
        else if (tag == 2) goNext(g_activeUid);
        else if (tag == 3) confirmGiveUp(g_activeUid);
    }
};

// ======================= окно мода =======================

class RouletteLayer : public CCLayer {
protected:
    enum class View { List, Create };
    View m_view = View::List;

    // состояние экрана создания забега
    unsigned m_cats = 0;
    bool m_fixed = false, m_skipAdds = true, m_inf = false;
    int m_skips = 0;
    TextInput* m_nameInput = nullptr;
    CCLabelBMFont* m_skipLabel = nullptr;

    bool init() override {
        if (!CCLayer::init()) return false;
        this->setKeypadEnabled(true);
        loadDefaults();
        build();
        return true;
    }

    void loadDefaults() {
        auto m = Mod::get();
        m_cats = 0;
        for (int i = 0; i < NCATS; i++)
            if (m->getSettingValue<bool>(CATS[i].key)) m_cats |= (1u << i);
        m_fixed = m->getSettingValue<bool>("step-from-required");
        m_skipAdds = m->getSettingValue<bool>("skip-adds-percent");
        m_inf = m->getSettingValue<bool>("skip-infinite");
        m_skips = static_cast<int>(m->getSettingValue<int64_t>("skip-count"));
    }

    void go(View v) {
        m_view = v;
        Loader::get()->queueInMainThread([self = Ref<RouletteLayer>(this)] { self->build(); });
    }

    // ---------- общий каркас ----------
    void build() {
        this->removeAllChildren();
        m_nameInput = nullptr;
        m_skipLabel = nullptr;
        auto win = CCDirector::get()->getWinSize();

        if (auto bg = CCSprite::create("GJ_gradientBG.png")) {
            bg->setAnchorPoint({0, 0});
            bg->setScaleX(win.width / bg->getContentWidth());
            bg->setScaleY(win.height / bg->getContentHeight());
            bg->setColor({0, 102, 255});
            this->addChild(bg, -10);
        }

        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        this->addChild(menu, 20);

        auto back = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png"), this,
            menu_selector(RouletteLayer::onBack));
        back->setPosition({25.f, win.height - 25.f});
        menu->addChild(back);

        if (m_view == View::List) buildList(menu, win);
        else buildCreate(menu, win);
    }

    CCMenuItemSpriteExtra* addButton(CCMenu* menu, char const* text, SEL_MenuHandler cb,
                                     CCPoint pos, float scale = 1.f, int tag = 0) {
        auto spr = ButtonSprite::create(text);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, cb);
        btn->setPosition(pos);
        setBtnScale(btn, scale);
        btn->setTag(tag);
        menu->addChild(btn);
        return btn;
    }

    static ccColor4B rowColor(int i) {
        return (i % 2 == 0) ? ccColor4B{161, 88, 44, 255} : ccColor4B{194, 114, 62, 255};
    }

    // ---------- список забегов ----------
    // Рамка, заголовок и фон — настоящий GJListLayer игры (как в Saved Levels),
    // строки с родными цветами ячеек лежат внутри него
    void buildList(CCMenu* menu, CCSize win) {
        float W = 356.f, H = 220.f;
        float lx = (win.width - W) / 2, ly = (win.height - H) / 2 - 8.f;

        auto emptyView = CustomListView::create(CCArray::create(), static_cast<BoomListType>(0), H, W);
        auto list = GJListLayer::create(emptyView, "Demon Roulette", {194, 114, 62, 255}, W, H, 0);
        list->setPosition({lx, ly});
        this->addChild(list, 1);

        addButton(menu, "Create run", menu_selector(RouletteLayer::onCreate), {win.width - 75.f, win.height - 28.f}, 0.8f);
        addButton(menu, "Delete all", menu_selector(RouletteLayer::onDeleteAll), {win.width - 75.f, win.height - 62.f}, 0.7f);

        CCSize sz{W, H};
        auto scroll = ScrollLayer::create(sz);
        scroll->setPosition({lx, ly});
        this->addChild(scroll, 5);

        float rowH = 56.f;
        size_t n = g_runs.size();
        float total = std::max(H, rowH * static_cast<float>(n));
        scroll->m_contentLayer->setContentSize({W, total});

        if (n == 0) {
            auto empty = CCLabelBMFont::create("No runs yet. Press Create run.", "bigFont.fnt");
            empty->setScale(0.4f);
            empty->setPosition({W / 2, total - H / 2});
            scroll->m_contentLayer->addChild(empty);
        }

        auto rowMenu = CCMenu::create();
        rowMenu->setPosition({0, 0});
        // размер меню = весь список, иначе прокрутка прячет его как «вышедшее за экран»
        rowMenu->setContentSize({W, total});
        scroll->m_contentLayer->addChild(rowMenu, 5);

        int row = 0;
        for (int k = static_cast<int>(n) - 1; k >= 0; --k, ++row) {   // сверху новые
            auto& r = g_runs[k];
            float y = total - (row + 1) * rowH;

            auto bg = CCLayerColor::create(rowColor(row), W, rowH);
            bg->setPosition({0.f, y});
            scroll->m_contentLayer->addChild(bg);

            std::string title = r.name + (r.uid == g_activeUid ? "  (active)" : "");
            auto name = CCLabelBMFont::create(title.c_str(), "bigFont.fnt");
            name->setAnchorPoint({0, 0.5f});
            name->setPosition({12.f, y + rowH - 18.f});
            name->limitLabelWidth(W - 150.f, 0.6f, 0.2f);
            scroll->m_contentLayer->addChild(name);

            std::string st;
            if (r.finished) st = fmt::format("Finished ({}) | passed {}", r.result, r.passed);
            else if (r.awaiting) st = fmt::format("Passed {} | next needs {}%", r.passed, r.required);
            else st = fmt::format("Level {} | needs {}% | passed {}", r.passed + 1, r.required, r.passed);
            auto status = CCLabelBMFont::create(st.c_str(), "chatFont.fnt");
            status->setAnchorPoint({0, 0.5f});
            status->setScale(0.75f);
            status->setColor({255, 225, 120});
            status->setPosition({12.f, y + 15.f});
            scroll->m_contentLayer->addChild(status);

            auto enter = CCMenuItemSpriteExtra::create(ButtonSprite::create("Enter"), this,
                menu_selector(RouletteLayer::onEnterRun));
            enter->setTag(r.uid);
            setBtnScale(enter, 0.65f);
            enter->setPosition({W - 95.f, y + rowH / 2});
            rowMenu->addChild(enter);

            CCNode* trash = CCSprite::createWithSpriteFrameName("GJ_trashBtn_001.png");
            if (!trash) trash = ButtonSprite::create("Del");
            auto del = CCMenuItemSpriteExtra::create(trash, this, menu_selector(RouletteLayer::onDeleteRun));
            del->setTag(r.uid);
            setBtnScale(del, 0.85f);
            del->setPosition({W - 32.f, y + rowH / 2});
            rowMenu->addChild(del);
        }
        scroll->moveToTop();
    }

    // ---------- создание забега ----------
    // info >= 0 добавляет рядом кнопку «?», по нажатию объясняющую функцию
    void addToggle(CCMenu* menu, float x, float y, char const* text, bool on, int tag, SEL_MenuHandler cb, int info = -1) {
        auto t = CCMenuItemToggler::createWithStandardSprites(this, cb, 0.55f);
        t->setPosition({x, y});
        t->setTag(tag);
        t->toggle(on);
        menu->addChild(t);
        auto label = CCLabelBMFont::create(text, "bigFont.fnt");
        label->setAnchorPoint({0, 0.5f});
        label->setScale(0.4f);
        label->setPosition({x + 16.f, y});
        this->addChild(label, 6);

        if (info >= 0) {
            // обычная кнопка игры с буквой «?» (отдельной иконки вопроса в игре нет)
            auto b = CCMenuItemSpriteExtra::create(ButtonSprite::create("?"), this, menu_selector(RouletteLayer::onInfo));
            b->setTag(info);
            setBtnScale(b, 0.5f);
            b->setPosition({x + 16.f + label->getScaledContentWidth() + 16.f, y});
            menu->addChild(b);
        }
    }

    void onInfo(CCObject* sender) {
        int i = static_cast<CCNode*>(sender)->getTag();
        char const* title = "";
        std::string text;
        if (i == 1) {
            title = "Skip adds +1%";
            text = "ON: skipping a level raises the requirement by 1%\n(skip at 10% -> the next level needs 11%).\n"
                   "OFF: the requirement stays the same (10%).";
        } else if (i == 2) {
            title = "Fixed +1 step";
            text = "OFF: the next level needs the percent you reached + 1\n(needed 1%, got 2% -> next needs 3%).\n"
                   "ON: the next level needs the previous requirement + 1,\nno matter how much you got\n(needed 1%, got 2% -> next needs 2%).";
        } else {
            return;
        }
        createQuickPopup(title, text, "OK", nullptr, [](FLAlertLayer*, bool) {});
    }

    std::string skipText() const {
        return m_inf ? std::string("Skips: infinite") : fmt::format("Skips: {}", m_skips);
    }

    void buildCreate(CCMenu* menu, CCSize win) {
        auto head = CCLabelBMFont::create("New run", "goldFont.fnt");
        head->setScale(0.75f);
        head->setPosition({win.width / 2, win.height - 20.f});
        this->addChild(head, 6);

        auto nameLbl = CCLabelBMFont::create("Run name:", "bigFont.fnt");
        nameLbl->setScale(0.45f);
        nameLbl->setPosition({win.width / 2 - 120.f, win.height - 52.f});
        this->addChild(nameLbl, 6);
        m_nameInput = TextInput::create(180.f, "Run name", "bigFont.fnt");
        m_nameInput->setPosition({win.width / 2 + 40.f, win.height - 52.f});
        m_nameInput->setString(fmt::format("Run {}", g_nextNum));
        m_nameInput->setMaxCharCount(20);
        this->addChild(m_nameInput, 6);

        float top = win.height - 90.f;
        auto h1 = CCLabelBMFont::create("Regular levels", "goldFont.fnt");
        h1->setScale(0.5f);
        h1->setPosition({win.width * 0.17f, top});
        this->addChild(h1, 6);
        auto h2 = CCLabelBMFont::create("Demons", "goldFont.fnt");
        h2->setScale(0.5f);
        h2->setPosition({win.width * 0.45f, top});
        this->addChild(h2, 6);
        auto h3 = CCLabelBMFont::create("Options", "goldFont.fnt");
        h3->setScale(0.5f);
        h3->setPosition({win.width * 0.78f, top});
        this->addChild(h3, 6);

        for (int i = 0; i < 6; i++)
            addToggle(menu, win.width * 0.06f, top - 28.f - i * 27.f, CATS[i].name,
                      (m_cats >> i) & 1u, i, menu_selector(RouletteLayer::onCat));
        for (int i = 6; i < NCATS; i++)
            addToggle(menu, win.width * 0.34f, top - 28.f - (i - 6) * 27.f, CATS[i].name,
                      (m_cats >> i) & 1u, i, menu_selector(RouletteLayer::onCat));

        float ox = win.width * 0.63f;
        m_skipLabel = CCLabelBMFont::create(skipText().c_str(), "bigFont.fnt");
        m_skipLabel->setScale(0.4f);
        m_skipLabel->setAnchorPoint({0, 0.5f});
        m_skipLabel->setPosition({ox, top - 28.f});
        this->addChild(m_skipLabel, 6);
        addButton(menu, "-", menu_selector(RouletteLayer::onSkipDelta), {ox + 135.f, top - 28.f}, 0.5f, -1);
        addButton(menu, "+", menu_selector(RouletteLayer::onSkipDelta), {ox + 160.f, top - 28.f}, 0.5f, 1);
        addToggle(menu, ox + 8.f, top - 58.f, "Infinite skips", m_inf, 0, menu_selector(RouletteLayer::onOpt));
        addToggle(menu, ox + 8.f, top - 85.f, "Skip adds +1%", m_skipAdds, 1, menu_selector(RouletteLayer::onOpt), 1);
        addToggle(menu, ox + 8.f, top - 112.f, "Fixed +1 step", m_fixed, 2, menu_selector(RouletteLayer::onOpt), 2);

        addButton(menu, "Create", menu_selector(RouletteLayer::onConfirmCreate), {win.width / 2 - 60.f, 24.f}, 0.9f);
        addButton(menu, "Cancel", menu_selector(RouletteLayer::onCancel), {win.width / 2 + 60.f, 24.f}, 0.9f);
    }

    // ---------- обработчики ----------
    void onBack(CCObject*) { keyBackClicked(); }

    void onCancel(CCObject*) { go(View::List); }

    void onCreate(CCObject*) {
        loadDefaults();
        go(View::Create);
    }

    void onCat(CCObject* sender) {
        int i = static_cast<CCNode*>(sender)->getTag();
        m_cats ^= (1u << i);
    }

    void onOpt(CCObject* sender) {
        int t = static_cast<CCNode*>(sender)->getTag();
        if (t == 0) m_inf = !m_inf;
        else if (t == 1) m_skipAdds = !m_skipAdds;
        else if (t == 2) m_fixed = !m_fixed;
        if (m_skipLabel) m_skipLabel->setString(skipText().c_str());
    }

    void onSkipDelta(CCObject* sender) {
        int d = static_cast<CCNode*>(sender)->getTag();
        m_skips = std::clamp(m_skips + d, 0, 99);
        if (m_skipLabel) m_skipLabel->setString(skipText().c_str());
    }

    void onConfirmCreate(CCObject*) {
        if (!m_cats) {
            Notification::create("Select at least one difficulty", NotificationIcon::Error)->show();
            return;
        }
        std::string nm = m_nameInput ? m_nameInput->getString() : std::string();
        while (!nm.empty() && nm.front() == ' ') nm.erase(nm.begin());
        while (!nm.empty() && nm.back() == ' ') nm.pop_back();
        if (nm.empty()) nm = fmt::format("Run {}", g_nextNum);

        RunData r;
        r.uid = g_nextUid++;
        r.name = clean(nm);
        r.created = static_cast<long long>(std::time(nullptr));
        r.cats = m_cats;
        r.fixedStep = m_fixed;
        r.skipAdds = m_skipAdds;
        r.infSkips = m_inf;
        r.skipLimit = m_skips;
        r.required = 1;
        g_nextNum++;
        int uid = r.uid;
        g_runs.push_back(std::move(r));
        g_activeUid = uid;
        saveAll();

        Fetcher::get()->beginRoll(uid);   // первый уровень открывается сам
        go(View::List);
    }

    void onEnterRun(CCObject* sender) {
        openRunLevels(static_cast<CCNode*>(sender)->getTag());
    }

    void onDeleteRun(CCObject* sender) {
        int uid = static_cast<CCNode*>(sender)->getTag();
        createQuickPopup("Delete run?", "This run will be removed from the list.", "No", "Yes",
            [self = Ref<RouletteLayer>(this), uid](FLAlertLayer*, bool yes) {
                if (!yes) return;
                g_runs.erase(std::remove_if(g_runs.begin(), g_runs.end(),
                    [uid](RunData const& r) { return r.uid == uid; }), g_runs.end());
                if (g_activeUid == uid) g_activeUid = 0;
                saveAll();
                self->go(View::List);
            });
    }

    void onDeleteAll(CCObject*) {
        if (g_runs.empty()) return;
        createQuickPopup("Delete all runs?", "Every run will be removed. This cannot be undone.", "No", "Yes",
            [self = Ref<RouletteLayer>(this)](FLAlertLayer*, bool yes) {
                if (!yes) return;
                g_runs.clear();
                g_activeUid = 0;
                saveAll();
                self->go(View::List);
            });
    }

public:
    static RouletteLayer* create() {
        auto ret = new RouletteLayer();
        if (ret && ret->init()) { ret->autorelease(); return ret; }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    static CCScene* scene() {
        auto sc = CCScene::create();
        sc->addChild(RouletteLayer::create());
        return sc;
    }

    void keyBackClicked() override {
        if (m_view == View::List) CCDirector::get()->popScene();
        else go(View::List);
    }
};

// Список уровней забега: Esc / стрелка назад ведут в список забегов
class $modify(RouletteBrowser, LevelBrowserLayer) {
    struct Fields {
        bool ours = false;
        int uid = 0;
    };

    bool init(GJSearchObject* obj) {
        if (!LevelBrowserLayer::init(obj)) return false;
        if (g_openingRunBrowser) {
            m_fields->ours = true;
            m_fields->uid = g_openingRunUid;
            g_openingRunBrowser = false;

            auto r = findRun(m_fields->uid);
            if (r && !r->finished) {
                auto btn = CCMenuItemSpriteExtra::create(ButtonSprite::create("Give up"), this,
                    menu_selector(RouletteBrowser::onRouletteGiveUp));
                setBtnScale(btn, 0.6f);
                float bw = btn->getScaledContentWidth();
                btn->setPosition({8.f + bw / 2, 170.f});
                auto menu = CCMenu::create();
                menu->setPosition({0, 0});
                menu->addChild(btn);
                this->addChild(menu, 60);
            }
        }
        return true;
    }

    void onRouletteGiveUp(CCObject*) { confirmGiveUp(m_fields->uid); }

    bool leaveToRuns() {
        if (!m_fields->ours) return false;
        showScene(CCTransitionFade::create(0.5f, RouletteLayer::scene()));
        return true;
    }

    void onBack(CCObject* sender) {
        if (leaveToRuns()) return;
        LevelBrowserLayer::onBack(sender);
    }

    void keyBackClicked() {
        if (leaveToRuns()) return;
        LevelBrowserLayer::keyBackClicked();
    }
};

// ======================= кнопка в главном меню =======================

class $modify(RouletteMenu, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        g_leaveThenNext = false;
        g_endKind = PassKind::None;
        auto menu = this->getChildByID("bottom-menu");
        auto spr = CircleButtonSprite::createWithSpriteFrameName(
            "diffIcon_06_btn_001.png", 1.0f, CircleBaseColor::Green, CircleBaseSize::Medium);
        if (menu && spr) {
            auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(RouletteMenu::onRoulette));
            btn->setID("demon-roulette-button"_spr);
            menu->addChild(btn);
            menu->updateLayout();
        }
        return true;
    }

    void onRoulette(CCObject*) {
        CCDirector::get()->pushScene(CCTransitionFade::create(0.5f, RouletteLayer::scene()));
    }
};
