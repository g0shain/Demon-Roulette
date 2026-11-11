#pragma once
// Логика забега: прохождение уровня, скип, сдача, открытие списка уровней забега

#include "core/Data.hpp"

void goNext(int uid);
PassKind markPassed(int uid);
void showPassPopup(int uid, PassKind kind);
void processPass(int uid);
void handleResult(int uid);
void doSkip(int uid);
void confirmSkip(int uid);
void openRunLevels(int uid);
void giveUpRun(int uid);
void confirmGiveUp(int uid);
