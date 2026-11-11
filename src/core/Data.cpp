#include "core/Data.hpp"

std::vector<RunData> g_runs;
int g_activeUid = 0;
int g_nextNum = 1;
int g_nextUid = 1;
int g_pendingUid = 0;
bool g_leaveThenNext = false;
bool g_nextInfoByUs = false;
bool g_openingRunBrowser = false;
int g_openingRunUid = 0;
PassKind g_endKind = PassKind::None;
int g_endUid = 0;

RunData* findRun(int uid) {
    for (auto& r : g_runs) if (r.uid == uid) return &r;
    return nullptr;
}
RunData* activeRun() { return g_activeUid ? findRun(g_activeUid) : nullptr; }

// Забег, к которому относится уровень (активный в приоритете)
RunData* runForLevel(int levelID) {
    if (auto a = activeRun()) {
        if (a->levelID == levelID) return a;
    }
    for (auto& r : g_runs) {
        if (r.levelID == levelID) return &r;
        for (auto& e : r.levels) if (e.id == levelID) return &r;
    }
    return nullptr;
}

void saveAll() {
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

void loadAll() {
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

bool canSkip(RunData const& r) {
    return !r.finished && !r.awaiting && r.levelID != 0 && (r.infSkips || r.skipsUsed < r.skipLimit);
}
