// Страница уровня: полоска, кнопки Skip / Next level / Give up, Esc по цепочке

#include <Geode/modify/LevelInfoLayer.hpp>

#include "core/Data.hpp"
#include "core/Fetcher.hpp"
#include "core/RunLogic.hpp"
#include "ui/NeedBar.hpp"

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
