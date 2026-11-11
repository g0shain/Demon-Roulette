#pragma once
// Запрашивает у серверов игры случайный уровень для забега или открывает уровень по ID

#include "core/Utils.hpp"

#include <map>

class Fetcher : public CCObject, public LevelManagerDelegate {
public:
    static Fetcher* get();
    void beginRoll(int uid);
    void beginOpen(int levelId);

private:
    enum class Kind { Roll, Open };
    Kind m_kind = Kind::Roll;
    int m_uid = 0;
    int m_openId = 0;
    bool m_waiting = false;
    int m_attempts = 0;
    int m_cat = 0;
    int m_page = 0;
    std::map<int, int> m_hiPage;   // верхняя граница номера страницы по каждой категории

    void requestRoll();
    void retryOrFail();
    void shrinkRange();
    void loadLevelsFinished(CCArray* levels, char const*) override;
    void loadLevelsFailed(char const*) override;
};
