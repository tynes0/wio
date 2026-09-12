#pragma once
#include <functional>
#include <stdexcept>
#include <string>
#include <thread>

namespace wir_native {
struct Vec { float x, y; };
template<class T> struct Cell { T value; };
inline void Scale(Vec& value, float factor) { value.x *= factor; value.y *= factor; }
inline float Sum(const Vec& value) { return value.x + value.y; }
inline float Sum(Vec&) { return -999; } // A view extension must select const&.
inline std::function<int(int)> stored;
inline void Store(std::function<int(int)> callback) { stored=std::move(callback); }
inline bool Expired() {
    bool rejected=false;
    try { stored(1); } catch (const std::exception& e) { rejected=std::string(e.what()).find("expired")!=std::string::npos; }
    stored={}; return rejected;
}
inline bool WrongThread(std::function<int(int)> callback) {
    bool rejected=false;
    std::thread worker([&] { try { callback(1); } catch (const std::exception& e) { rejected=std::string(e.what()).find("wrong thread")!=std::string::npos; } });
    worker.join(); return rejected;
}
inline void Append(std::string& value) { value += "-native"; }
inline const std::string& Label() { static const std::string value = "borrowed"; return value; }
inline int& Slot() { static int value = 1; return value; }
inline int ReadSlot() { return Slot(); }
inline void Write(int& value) { value = 7; Slot() = 7; }
inline int Call(int value, std::function<int(int)> callback, void* context) {
    return callback(value) + *static_cast<int*>(context);
}
inline void* Context() { return &Slot(); }
template<class T> T Identity(T value) { return value; }
template<class T> T Zero() { return T{}; }
template<class T> Cell<T> MakeCell(T value) { return {value}; }
inline int Fail() { throw std::runtime_error("native failure"); }
}
