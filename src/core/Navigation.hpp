#pragma once
// Переходы между экранами: в стеке сцен остаются только главное меню и нужный экран

#include "core/Utils.hpp"

void showScene(CCScene* scene);
void openLevelScene(GJGameLevel* level);
// Открывает уровень, дождавшись окончания текущего перехода; сам освобождает level
void openLevelSafe(GJGameLevel* level, int tries = 0);
