// Кнопка мода в главном меню

#include <Geode/modify/MenuLayer.hpp>

#include "core/Data.hpp"
#include "ui/RouletteLayer.hpp"

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
        CCDirector::get()->pushScene(CCTransitionFade::create(0.5f, rouletteScene()));
    }
};
