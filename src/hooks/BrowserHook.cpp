// Список уровней забега: Esc и стрелка назад ведут в список забегов, кнопка Give up

#include <Geode/modify/LevelBrowserLayer.hpp>

#include "core/Data.hpp"
#include "core/Navigation.hpp"
#include "core/RunLogic.hpp"
#include "ui/RouletteLayer.hpp"

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
        showScene(CCTransitionFade::create(0.5f, rouletteScene()));
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
