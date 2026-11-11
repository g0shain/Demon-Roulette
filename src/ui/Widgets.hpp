#pragma once
// Мелкие элементы оформления: скруглённые плашки, объёмные кнопки, панели

#include "core/Utils.hpp"

ccColor3B shade(ccColor3B c, float f);   // затемняет цвет
ccColor3B tint(ccColor3B c, float f);    // осветляет цвет (f = доля белого)

CCScale9Sprite* panel(CCNode* parent, float x, float y, float w, float h, ccColor3B col, int opacity, int z);
void bevel(CCNode* node, float w, float h, ccColor3B col);
CCNode* makeBtn(std::string const& text, ccColor3B col, float w, float h, float fontScale);
CCNode* makeChip(std::string const& bigText, std::string const& caption, ccColor3B col, float w, float h);
void bigPanel(CCNode* parent, float x, float y, float w, float h, int z);
