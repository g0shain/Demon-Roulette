// Хуки уровня: учёт процента, окно прохождения, полоска в игре и в паузе

#include <Geode/modify/EndLevelLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include "core/Data.hpp"
#include "core/RunLogic.hpp"
#include "ui/NeedBar.hpp"

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
