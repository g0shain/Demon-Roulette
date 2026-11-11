#include "ui/RouletteLayer.hpp"
#include "core/Data.hpp"
#include "core/Fetcher.hpp"
#include "core/RunLogic.hpp"
#include "ui/Widgets.hpp"

#include <Geode/ui/Notification.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/TextInput.hpp>

#include <algorithm>
#include <ctime>

class RouletteLayer : public CCLayer {
protected:
    enum class View { List, Create };
    View m_view = View::List;

    // состояние экрана создания забега
    unsigned m_cats = 0;
    bool m_fixed = false, m_skipAdds = true, m_inf = false;
    int m_skips = 0;
    TextInput* m_nameInput = nullptr;
    CCLabelBMFont* m_skipLabel = nullptr;

    bool init() override {
        if (!CCLayer::init()) return false;
        this->setKeypadEnabled(true);
        loadDefaults();
        build();
        return true;
    }

    void loadDefaults() {
        auto m = Mod::get();
        m_cats = 0;
        for (int i = 0; i < NCATS; i++)
            if (m->getSettingValue<bool>(CATS[i].key)) m_cats |= (1u << i);
        m_fixed = m->getSettingValue<bool>("step-from-required");
        m_skipAdds = m->getSettingValue<bool>("skip-adds-percent");
        m_inf = m->getSettingValue<bool>("skip-infinite");
        m_skips = static_cast<int>(m->getSettingValue<int64_t>("skip-count"));
    }

    void go(View v) {
        m_view = v;
        Loader::get()->queueInMainThread([self = Ref<RouletteLayer>(this)] { self->build(); });
    }

    // ---------- общий каркас ----------
    void build() {
        this->removeAllChildren();
        m_nameInput = nullptr;
        m_skipLabel = nullptr;
        auto win = CCDirector::get()->getWinSize();

        if (auto bg = CCSprite::create("GJ_gradientBG.png")) {
            bg->setAnchorPoint({0, 0});
            bg->setScaleX(win.width / bg->getContentWidth());
            bg->setScaleY(win.height / bg->getContentHeight());
            bg->setColor({36, 28, 96});
            this->addChild(bg, -10);
        }

        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        this->addChild(menu, 20);

        auto back = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png"), this,
            menu_selector(RouletteLayer::onBack));
        back->setPosition({25.f, win.height - 25.f});
        menu->addChild(back);

        if (m_view == View::List) buildList(menu, win);
        else buildCreate(menu, win);
    }

    CCMenuItemSpriteExtra* addButton(CCMenu* menu, char const* text, SEL_MenuHandler cb,
                                     CCPoint pos, float scale = 1.f, int tag = 0) {
        auto spr = ButtonSprite::create(text);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, cb);
        btn->setPosition(pos);
        setBtnScale(btn, scale);
        btn->setTag(tag);
        menu->addChild(btn);
        return btn;
    }

    CCMenuItemSpriteExtra* addBtn(CCMenu* menu, std::string const& text, ccColor3B col, float w, float h,
                                  SEL_MenuHandler cb, CCPoint pos, int tag = 0, float fontScale = 0.5f) {
        auto btn = CCMenuItemSpriteExtra::create(makeBtn(text, col, w, h, fontScale), this, cb);
        btn->setPosition(pos);
        btn->setTag(tag);
        menu->addChild(btn);
        return btn;
    }

    // ---------- список забегов ----------
    void buildList(CCMenu* menu, CCSize win) {
        float cx = win.width / 2;
        size_t n = g_runs.size();

        auto title = CCLabelBMFont::create("Demon Roulette", "goldFont.fnt");
        title->setScale(0.85f);
        title->setPosition({cx, win.height - 24.f});
        this->addChild(title, 5);

        auto sub = CCLabelBMFont::create(fmt::format("{} run{}", n, n == 1 ? "" : "s").c_str(), "chatFont.fnt");
        sub->setScale(0.85f);
        sub->setColor({195, 185, 235});
        sub->setPosition({cx, win.height - 43.f});
        this->addChild(sub, 5);

        addBtn(menu, "+ Create run", {60, 200, 90}, 112.f, 30.f,
               menu_selector(RouletteLayer::onCreate), {win.width - 72.f, win.height - 28.f});
        addBtn(menu, "Delete all", {150, 55, 70}, 92.f, 24.f,
               menu_selector(RouletteLayer::onDeleteAll), {70.f, 26.f}, 0, 0.4f);

        // панель со списком
        float px = 34.f, py = 48.f, pw = win.width - 68.f, ph = win.height - 48.f - 62.f;
        bigPanel(this, px, py, pw, ph, 2);

        CCSize sz{pw - 12.f, ph - 12.f};
        auto scroll = ScrollLayer::create(sz);
        scroll->setPosition({px + 6.f, py + 6.f});
        this->addChild(scroll, 5);

        float slot = 62.f, rowW = sz.width;
        float total = std::max(sz.height, slot * static_cast<float>(n));
        scroll->m_contentLayer->setContentSize({rowW, total});

        if (n == 0) {
            auto empty = CCLabelBMFont::create("No runs yet", "bigFont.fnt");
            empty->setScale(0.6f);
            empty->setPosition({rowW / 2, total - sz.height / 2 + 12.f});
            scroll->m_contentLayer->addChild(empty);
            auto hint = CCLabelBMFont::create("Press Create run to start", "chatFont.fnt");
            hint->setColor({195, 185, 235});
            hint->setPosition({rowW / 2, total - sz.height / 2 - 12.f});
            scroll->m_contentLayer->addChild(hint);
        }

        auto rowMenu = CCMenu::create();
        rowMenu->setPosition({0, 0});
        // размер меню = весь список, иначе прокрутка прячет его как «вышедшее за экран»
        rowMenu->setContentSize({rowW, total});
        scroll->m_contentLayer->addChild(rowMenu, 5);

        int row = 0;
        for (int k = static_cast<int>(n) - 1; k >= 0; --k, ++row) {   // сверху новые
            auto& r = g_runs[k];
            float y = total - (row + 1) * slot;
            float rh = slot - 6.f;
            float cy = y + 3.f + rh / 2;

            // плашка забега: светлая окантовка + заливка + блик сверху
            panel(scroll->m_contentLayer, 2.f, y + 3.f, rowW - 4.f, rh, {135, 115, 215}, 255, 0);
            panel(scroll->m_contentLayer, 4.f, y + 5.f, rowW - 8.f, rh - 4.f, {58, 44, 92}, 255, 0);
            panel(scroll->m_contentLayer, 8.f, y + 3.f + rh / 2, rowW - 16.f, rh / 2 - 5.f, {255, 255, 255}, 16, 0);

            // плашка статуса слева
            bool gaveUp = r.finished && r.result == "gave up";
            ccColor3B chipCol = r.finished
                ? (gaveUp ? ccColor3B{200, 75, 75} : ccColor3B{75, 200, 100})
                : (r.uid == g_activeUid ? ccColor3B{255, 185, 45} : ccColor3B{80, 150, 240});
            std::string chipBig = r.finished ? (gaveUp ? "X" : "OK") : fmt::format("{}%", r.required);
            std::string chipSmall = r.finished ? (gaveUp ? "gave up" : "done") : "need";
            auto chip = makeChip(chipBig, chipSmall, chipCol, 54.f, 42.f);
            chip->setPosition({10.f, cy - 21.f});
            scroll->m_contentLayer->addChild(chip, 2);

            std::string nm = r.name + (r.uid == g_activeUid ? "  (active)" : "");
            auto name = CCLabelBMFont::create(nm.c_str(), "bigFont.fnt");
            name->setAnchorPoint({0, 0.5f});
            name->setPosition({74.f, cy + 10.f});
            name->limitLabelWidth(rowW - 74.f - 160.f, 0.55f, 0.2f);
            scroll->m_contentLayer->addChild(name, 2);

            std::string st;
            if (r.finished) st = fmt::format("Finished ({}) | passed {}", r.result, r.passed);
            else if (r.awaiting) st = fmt::format("Passed {} | next needs {}%", r.passed, r.required);
            else st = fmt::format("Level {} | needs {}% | passed {}", r.passed + 1, r.required, r.passed);
            auto status = CCLabelBMFont::create(st.c_str(), "chatFont.fnt");
            status->setAnchorPoint({0, 0.5f});
            status->setScale(0.8f);
            status->setColor({205, 195, 235});
            status->setPosition({74.f, cy - 11.f});
            scroll->m_contentLayer->addChild(status, 2);

            auto enter = CCMenuItemSpriteExtra::create(
                makeBtn("Enter", {60, 200, 90}, 70.f, 28.f, 0.5f), this, menu_selector(RouletteLayer::onEnterRun));
            enter->setTag(r.uid);
            enter->setPosition({rowW - 114.f, cy});
            rowMenu->addChild(enter);

            auto del = CCMenuItemSpriteExtra::create(
                makeBtn("Delete", {200, 70, 80}, 62.f, 28.f, 0.45f), this, menu_selector(RouletteLayer::onDeleteRun));
            del->setTag(r.uid);
            del->setPosition({rowW - 44.f, cy});
            rowMenu->addChild(del);
        }
        scroll->moveToTop();
    }

    // ---------- создание забега ----------
    static ccColor3B catColor(int i) {
        static const ccColor3B C[] = {
            {110, 230, 255}, {120, 255, 120}, {120, 200, 255}, {255, 210, 90}, {255, 150, 60}, {255, 100, 160},
            {200, 255, 160}, {255, 230, 120}, {255, 160, 100}, {255, 100, 130}, {210, 110, 255}};
        return C[i];
    }

    // info >= 0 добавляет рядом кнопку «?», по нажатию объясняющую функцию
    void addToggle(CCMenu* menu, float x, float y, char const* text, bool on, int tag, SEL_MenuHandler cb,
                   int info = -1, ccColor3B col = ccColor3B{255, 255, 255}) {
        auto t = CCMenuItemToggler::createWithStandardSprites(this, cb, 0.5f);
        t->setPosition({x, y});
        t->setTag(tag);
        t->toggle(on);
        menu->addChild(t);
        auto label = CCLabelBMFont::create(text, "bigFont.fnt");
        label->setAnchorPoint({0, 0.5f});
        label->setScale(0.38f);
        label->setColor(col);
        label->setPosition({x + 16.f, y});
        this->addChild(label, 6);

        if (info >= 0) {
            auto b = CCMenuItemSpriteExtra::create(
                makeBtn("?", {110, 110, 190}, 20.f, 20.f, 0.5f), this, menu_selector(RouletteLayer::onInfo));
            b->setTag(info);
            b->setPosition({x + 16.f + label->getScaledContentWidth() + 16.f, y});
            menu->addChild(b);
        }
    }

    void onInfo(CCObject* sender) {
        int i = static_cast<CCNode*>(sender)->getTag();
        char const* title = "";
        std::string text;
        if (i == 1) {
            title = "Skip adds +1%";
            text = "ON: skipping a level raises the requirement by 1%\n(skip at 10% -> the next level needs 11%).\n"
                   "OFF: the requirement stays the same (10%).";
        } else if (i == 2) {
            title = "Fixed +1 step";
            text = "OFF: the next level needs the percent you reached + 1\n(needed 1%, got 2% -> next needs 3%).\n"
                   "ON: the next level needs the previous requirement + 1,\nno matter how much you got\n(needed 1%, got 2% -> next needs 2%).";
        } else {
            return;
        }
        createQuickPopup(title, text, "OK", nullptr, [](FLAlertLayer*, bool) {});
    }

    std::string skipText() const {
        return m_inf ? std::string("Skips: infinite") : fmt::format("Skips: {}", m_skips);
    }

    void panelHeader(float cx, float y, char const* text) {
        auto h = CCLabelBMFont::create(text, "goldFont.fnt");
        h->setScale(0.5f);
        h->setPosition({cx, y});
        this->addChild(h, 6);
    }

    void buildCreate(CCMenu* menu, CCSize win) {
        float cx = win.width / 2;

        auto head = CCLabelBMFont::create("New run", "goldFont.fnt");
        head->setScale(0.8f);
        head->setPosition({cx, win.height - 24.f});
        this->addChild(head, 6);

        // карточка с названием
        bigPanel(this, cx - 150.f, win.height - 72.f, 300.f, 34.f, 2);
        auto nameLbl = CCLabelBMFont::create("Name", "bigFont.fnt");
        nameLbl->setScale(0.4f);
        nameLbl->setAnchorPoint({0, 0.5f});
        nameLbl->setPosition({cx - 138.f, win.height - 55.f});
        this->addChild(nameLbl, 6);
        m_nameInput = TextInput::create(210.f, "Run name", "bigFont.fnt");
        m_nameInput->setPosition({cx + 30.f, win.height - 55.f});
        m_nameInput->setString(fmt::format("Run {}", g_nextNum));
        m_nameInput->setMaxCharCount(20);
        this->addChild(m_nameInput, 6);

        // три панели
        float pt = win.height - 84.f, pb = 58.f, ph = pt - pb;
        float lx = 18.f, lw = 162.f, mx = 190.f, mw = 162.f, rx = 362.f, rw = 189.f;
        float cols[3][2] = {{lx, lw}, {mx, mw}, {rx, rw}};
        for (auto& c : cols) {
            bigPanel(this, c[0], pb, c[1], ph, 2);
        }
        panelHeader(lx + lw / 2, pt - 14.f, "Regular levels");
        panelHeader(mx + mw / 2, pt - 14.f, "Demons");
        panelHeader(rx + rw / 2, pt - 14.f, "Options");

        for (int i = 0; i < 6; i++)
            addToggle(menu, lx + 20.f, pt - 40.f - i * 22.f, CATS[i].name, (m_cats >> i) & 1u, i,
                      menu_selector(RouletteLayer::onCat), -1, catColor(i));
        for (int i = 6; i < NCATS; i++)
            addToggle(menu, mx + 20.f, pt - 40.f - (i - 6) * 22.f, CATS[i].name, (m_cats >> i) & 1u, i,
                      menu_selector(RouletteLayer::onCat), -1, catColor(i));

        m_skipLabel = CCLabelBMFont::create(skipText().c_str(), "bigFont.fnt");
        m_skipLabel->setScale(0.4f);
        m_skipLabel->setAnchorPoint({0, 0.5f});
        m_skipLabel->setPosition({rx + 12.f, pt - 40.f});
        this->addChild(m_skipLabel, 6);
        addBtn(menu, "-", {110, 110, 190}, 22.f, 22.f, menu_selector(RouletteLayer::onSkipDelta),
               {rx + rw - 46.f, pt - 40.f}, -1, 0.6f);
        addBtn(menu, "+", {110, 110, 190}, 22.f, 22.f, menu_selector(RouletteLayer::onSkipDelta),
               {rx + rw - 20.f, pt - 40.f}, 1, 0.6f);
        addToggle(menu, rx + 18.f, pt - 70.f, "Infinite skips", m_inf, 0, menu_selector(RouletteLayer::onOpt));
        addToggle(menu, rx + 18.f, pt - 96.f, "Skip adds +1%", m_skipAdds, 1, menu_selector(RouletteLayer::onOpt), 1);
        addToggle(menu, rx + 18.f, pt - 122.f, "Fixed +1 step", m_fixed, 2, menu_selector(RouletteLayer::onOpt), 2);

        addBtn(menu, "Create", {60, 200, 90}, 124.f, 32.f, menu_selector(RouletteLayer::onConfirmCreate),
               {cx - 72.f, 30.f}, 0, 0.6f);
        addBtn(menu, "Cancel", {110, 110, 140}, 104.f, 32.f, menu_selector(RouletteLayer::onCancel),
               {cx + 72.f, 30.f}, 0, 0.6f);
    }

    // ---------- обработчики ----------
    void onBack(CCObject*) { keyBackClicked(); }

    void onCancel(CCObject*) { go(View::List); }

    void onCreate(CCObject*) {
        loadDefaults();
        go(View::Create);
    }

    void onCat(CCObject* sender) {
        int i = static_cast<CCNode*>(sender)->getTag();
        m_cats ^= (1u << i);
    }

    void onOpt(CCObject* sender) {
        int t = static_cast<CCNode*>(sender)->getTag();
        if (t == 0) m_inf = !m_inf;
        else if (t == 1) m_skipAdds = !m_skipAdds;
        else if (t == 2) m_fixed = !m_fixed;
        if (m_skipLabel) m_skipLabel->setString(skipText().c_str());
    }

    void onSkipDelta(CCObject* sender) {
        int d = static_cast<CCNode*>(sender)->getTag();
        m_skips = std::clamp(m_skips + d, 0, 99);
        if (m_skipLabel) m_skipLabel->setString(skipText().c_str());
    }

    void onConfirmCreate(CCObject*) {
        if (!m_cats) {
            Notification::create("Select at least one difficulty", NotificationIcon::Error)->show();
            return;
        }
        std::string nm = m_nameInput ? m_nameInput->getString() : std::string();
        while (!nm.empty() && nm.front() == ' ') nm.erase(nm.begin());
        while (!nm.empty() && nm.back() == ' ') nm.pop_back();
        if (nm.empty()) nm = fmt::format("Run {}", g_nextNum);

        RunData r;
        r.uid = g_nextUid++;
        r.name = clean(nm);
        r.created = static_cast<long long>(std::time(nullptr));
        r.cats = m_cats;
        r.fixedStep = m_fixed;
        r.skipAdds = m_skipAdds;
        r.infSkips = m_inf;
        r.skipLimit = m_skips;
        r.required = 1;
        g_nextNum++;
        int uid = r.uid;
        g_runs.push_back(std::move(r));
        g_activeUid = uid;
        saveAll();

        Fetcher::get()->beginRoll(uid);   // первый уровень открывается сам
        go(View::List);
    }

    void onEnterRun(CCObject* sender) {
        openRunLevels(static_cast<CCNode*>(sender)->getTag());
    }

    void onDeleteRun(CCObject* sender) {
        int uid = static_cast<CCNode*>(sender)->getTag();
        createQuickPopup("Delete run?", "This run will be removed from the list.", "No", "Yes",
            [self = Ref<RouletteLayer>(this), uid](FLAlertLayer*, bool yes) {
                if (!yes) return;
                g_runs.erase(std::remove_if(g_runs.begin(), g_runs.end(),
                    [uid](RunData const& r) { return r.uid == uid; }), g_runs.end());
                if (g_activeUid == uid) g_activeUid = 0;
                saveAll();
                self->go(View::List);
            });
    }

    void onDeleteAll(CCObject*) {
        if (g_runs.empty()) return;
        createQuickPopup("Delete all runs?", "Every run will be removed. This cannot be undone.", "No", "Yes",
            [self = Ref<RouletteLayer>(this)](FLAlertLayer*, bool yes) {
                if (!yes) return;
                g_runs.clear();
                g_activeUid = 0;
                saveAll();
                self->go(View::List);
            });
    }

public:
    static RouletteLayer* create() {
        auto ret = new RouletteLayer();
        if (ret && ret->init()) { ret->autorelease(); return ret; }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    static CCScene* scene() {
        auto sc = CCScene::create();
        sc->addChild(RouletteLayer::create());
        return sc;
    }

    void keyBackClicked() override {
        if (m_view == View::List) CCDirector::get()->popScene();
        else go(View::List);
    }
};


CCScene* rouletteScene() { return RouletteLayer::scene(); }
