#include "core/Navigation.hpp"
#include "core/Data.hpp"

template <class T>
static int sceneDepth(T* d) {
    if constexpr (requires { d->m_pobScenesStack->count(); }) return static_cast<int>(d->m_pobScenesStack->count());
    else return 0;
}

// Показывает сцену так, чтобы в стеке остались только главное меню и она сама.
// Тогда стек не растёт, а Esc идёт по цепочке:
// страница уровня -> список уровней забега -> список забегов -> главное меню
void showScene(CCScene* scene) {
    auto dir = CCDirector::get();
    int depth = sceneDepth(dir);
    if (depth >= 2) {
        for (int i = depth; i > 2; --i) dir->popScene();
        dir->replaceScene(scene);
    } else if (depth == 1) {
        dir->pushScene(scene);
    } else {
        dir->replaceScene(scene);
    }
}

void openLevelScene(GJGameLevel* level) {
    g_nextInfoByUs = true;   // страница будет «нашей»: Esc с неё ведёт в список уровней забега
    auto scene = LevelInfoLayer::scene(level, false);
    g_nextInfoByUs = false;
    showScene(CCTransitionFade::create(0.5f, scene));
}

// Открывает уровень, дождавшись окончания текущего перехода между сценами
// (иначе replaceScene во время перехода теряется). Сам освобождает level.
void openLevelSafe(GJGameLevel* level, int tries) {
    auto scene = CCDirector::get()->getRunningScene();
    if (scene && typeinfo_cast<CCTransitionScene*>(scene) && tries < 180) {
        Loader::get()->queueInMainThread([level, tries] { openLevelSafe(level, tries + 1); });
        return;
    }
    openLevelScene(level);
    level->release();
}
