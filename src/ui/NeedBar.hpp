#pragma once
// Полоска «нужный процент» в стиле стандартных полосок GD

#include "core/Data.hpp"

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

void setBarFromRun(NeedBar* bar, RunData const& r);

// Ставит полоску на место прятанной полоски практики (или в запасную позицию)
NeedBar* attachBar(CCNode* host, RunData const& r, CCPoint fallback);

// Прячет стандартную полоску прогресса уровня, если она есть в биндингах
template <class T>
inline CCNode* hideVanillaBar(T* pl) {
    if constexpr (requires { static_cast<CCNode*>(pl->m_progressBar)->setVisible(false); }) {
        auto n = static_cast<CCNode*>(pl->m_progressBar);
        n->setVisible(false);
        return n;
    } else {
        return nullptr;
    }
}
