#include "core/RunLogic.hpp"
#include "core/Fetcher.hpp"
#include "core/Navigation.hpp"

#include <Geode/ui/Notification.hpp>

void goNext(int uid) {
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
PassKind markPassed(int uid) {
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

void showPassPopup(int uid, PassKind kind) {
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

void processPass(int uid) { showPassPopup(uid, markPassed(uid)); }

void handleResult(int uid) {
    auto r = findRun(uid);
    if (!r || r->finished || r->awaiting || r->levelID == 0) return;
    if (r->best >= r->required) processPass(uid);
    else Notification::create(fmt::format("Best {}%, need {}%", r->best, r->required), NotificationIcon::Info)->show();
}

void doSkip(int uid) {
    auto r = findRun(uid);
    if (!r || !canSkip(*r)) return;
    r->levels.push_back({r->levelID, r->levelName, r->diffName, r->required, r->best, "skipped"});
    r->skipsUsed++;
    if (r->skipAdds) r->required = std::min(100, r->required + 1);
    saveAll();
    Fetcher::get()->beginRoll(uid);
}

void confirmSkip(int uid) {
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
void openRunLevels(int uid) {
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

void giveUpRun(int uid) {
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
void confirmGiveUp(int uid) {
    auto r = findRun(uid);
    if (!r || r->finished) return;
    createQuickPopup("Give up?", "End this run? It stays in the list as finished.", "No", "Yes",
        [uid](FLAlertLayer*, bool yes) {
            if (!yes) return;
            giveUpRun(uid);
            openRunLevels(uid);
        });
}
