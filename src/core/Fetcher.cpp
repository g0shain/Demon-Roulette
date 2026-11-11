#include "core/Fetcher.hpp"
#include "core/Data.hpp"
#include "core/Navigation.hpp"

#include <Geode/ui/Notification.hpp>

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


Fetcher* Fetcher::get() {
    static Fetcher* inst = new Fetcher();
    return inst;
}

void Fetcher::beginRoll(int uid) {
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

void Fetcher::beginOpen(int levelId) {
    m_kind = Kind::Open;
    m_openId = levelId;
    m_waiting = true;
    Notification::create("Opening level...", NotificationIcon::Loading, 1.5f)->show();
    auto obj = GJSearchObject::create(SearchType::Search, std::to_string(levelId));
    auto glm = GameLevelManager::sharedState();
    glm->m_levelManagerDelegate = this;
    glm->getOnlineLevels(obj);
}

void Fetcher::requestRoll() {
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

void Fetcher::retryOrFail() {
    m_waiting = false;
    if (++m_attempts > 25) {
        GameLevelManager::sharedState()->m_levelManagerDelegate = nullptr;
        m_hiPage.clear();
        Notification::create("Could not load a level. Press Continue in the run to retry.", NotificationIcon::Error)->show();
        return;
    }
    Loader::get()->queueInMainThread([this] { requestRoll(); });
}

void Fetcher::shrinkRange() {
    int& hi = m_hiPage[m_cat];
    hi = m_page > 0 ? m_page - 1 : 0;
}

void Fetcher::loadLevelsFinished(CCArray* levels, char const*) {
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
void Fetcher::loadLevelsFailed(char const*) {
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
