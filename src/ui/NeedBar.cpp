#include "ui/NeedBar.hpp"

void setBarFromRun(NeedBar* bar, RunData const& r) {
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

NeedBar* attachBar(CCNode* host, RunData const& r, CCPoint fallback) {
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
