#include "ui/Widgets.hpp"

#include <algorithm>
#include <cmath>

CCScale9Sprite* panel(CCNode* parent, float x, float y, float w, float h, ccColor3B col, int opacity, int z) {
    // Скруглённая заготовка ломается (углы наезжают друг на друга), если она меньше ~55 единиц.
    // Поэтому маленькие плашки строим крупнее и сжимаем обратно.
    float m = std::min(w, h);
    float k = m < 60.f ? std::ceil(60.f / m) : 1.f;
    auto sp = CCScale9Sprite::create("square02b_001.png", {0, 0, 80, 80});
    sp->setContentSize({w * k, h * k});
    sp->setAnchorPoint({0, 0});
    sp->setScale(1.f / k);
    sp->setPosition({x, y});
    sp->setColor(col);
    sp->setOpacity(static_cast<GLubyte>(opacity));
    parent->addChild(sp, z);
    return sp;
}

ccColor3B shade(ccColor3B c, float f) {
    return {static_cast<GLubyte>(c.r * f), static_cast<GLubyte>(c.g * f), static_cast<GLubyte>(c.b * f)};
}

// Осветляет цвет (f = доля белого)
ccColor3B tint(ccColor3B c, float f) {
    return {static_cast<GLubyte>(c.r + (255 - c.r) * f), static_cast<GLubyte>(c.g + (255 - c.g) * f),
            static_cast<GLubyte>(c.b + (255 - c.b) * f)};
}

// Объёмная скруглённая заготовка: тень, тёмный контур, мягкая окантовка в тон, тёмный низ, основа, блик
void bevel(CCNode* node, float w, float h, ccColor3B col) {
    auto add = [node](ccColor3B c, int op, float x, float y, float ww, float hh, int z) {
        panel(node, x, y, ww, hh, c, op, z);
    };
    add({0, 0, 0}, 110, 0.f, -2.f, w, h, 0);                           // тень
    add(shade(col, 0.4f), 255, 0.f, 0.f, w, h, 1);                     // тёмный контур
    add(tint(col, 0.45f), 255, 1.5f, 1.5f, w - 3.f, h - 3.f, 2);       // мягкая окантовка в тон цвета
    add(shade(col, 0.6f), 255, 3.f, 3.f, w - 6.f, h - 6.f, 3);         // тёмная основа (низ)
    add(col, 255, 3.f, 6.f, w - 6.f, h - 9.f, 4);                      // основной цвет
    add({255, 255, 255}, 50, 5.f, h / 2.f, w - 10.f, h / 2.f - 4.f, 5);   // блик сверху
}

// Кнопка нужного цвета с окантовкой и объёмом
CCNode* makeBtn(std::string const& text, ccColor3B col, float w, float h, float fontScale) {
    auto node = CCNode::create();
    node->setContentSize({w, h});
    bevel(node, w, h, col);
    auto lbl = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
    lbl->limitLabelWidth(w - 10.f, fontScale, 0.15f);
    lbl->setPosition({w / 2, h / 2 + 1.f});
    node->addChild(lbl, 10);
    return node;
}

// Плашка статуса забега: крупный процент и подпись под ним
CCNode* makeChip(std::string const& bigText, std::string const& caption, ccColor3B col, float w, float h) {
    auto node = CCNode::create();
    node->setAnchorPoint({0, 0});
    node->setContentSize({w, h});
    bevel(node, w, h, col);
    auto b = CCLabelBMFont::create(bigText.c_str(), "bigFont.fnt");
    b->limitLabelWidth(w - 10.f, 0.55f, 0.2f);
    b->setPosition({w / 2, h / 2 + 6.f});
    node->addChild(b, 10);
    auto sm = CCLabelBMFont::create(caption.c_str(), "chatFont.fnt");
    sm->setScale(0.6f);
    sm->setPosition({w / 2, h / 2 - 11.f});
    node->addChild(sm, 10);
    return node;
}

// Большая панель: тень, светлая окантовка, тёмная линия, заливка
void bigPanel(CCNode* parent, float x, float y, float w, float h, int z) {
    panel(parent, x - 3.f, y - 5.f, w + 6.f, h + 6.f, {0, 0, 0}, 110, z);
    panel(parent, x - 3.f, y - 3.f, w + 6.f, h + 6.f, {165, 140, 245}, 255, z);
    panel(parent, x - 1.f, y - 1.f, w + 2.f, h + 2.f, {70, 52, 135}, 255, z);
    panel(parent, x, y, w, h, {20, 14, 44}, 245, z);
}
