#pragma once
// Общие мелкие помощники, которыми пользуются все остальные файлы мода

#include <Geode/Geode.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

using namespace geode::prelude;

template <class T>
inline int idOf(T& v) {
    if constexpr (requires { v.value(); }) return static_cast<int>(v.value());
    else return static_cast<int>(v);
}

inline std::mt19937& rng() {
    static std::mt19937 r{std::random_device{}()};
    return r;
}
inline int randInt(int a, int b) { return std::uniform_int_distribution<int>(a, b)(rng()); }

inline std::string joinStr(std::vector<std::string> const& v, char sep) {
    std::string s;
    for (size_t i = 0; i < v.size(); i++) { if (i) s += sep; s += v[i]; }
    return s;
}
inline std::vector<std::string> splitKeep(std::string const& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) { out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    out.push_back(cur);
    return out;
}
inline std::string clean(std::string s) {
    for (auto& c : s) if (c == '|' || c == '\n' || c == '\r') c = ' ';
    return s;
}
inline int toInt(std::string const& s) { return std::atoi(s.c_str()); }


// У CCMenuItemSpriteExtra анимация нажатия возвращает кнопку к m_baseScale,
// поэтому после setScale его нужно выставить тоже
template <class T>
inline void setBtnScale(T* btn, float scale) {
    btn->setScale(scale);
    if constexpr (requires { btn->m_baseScale = scale; }) btn->m_baseScale = scale;
}
