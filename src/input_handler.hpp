#ifndef INPUT_HANDLER_HPP
#define INPUT_HANDLER_HPP

#include <SDL2/SDL.h>
#include <cstdint>
#include <functional>
#include <unordered_map>

//include "logger.hpp"

class InputHandler {
public:
    InputHandler();
    void handle_events();
    void add_binding(SDL_EventType event, std::function<void()> func);

    template<typename T>
    void add_binding(SDL_EventType event, T* obj, void (T::*method)())
    {
        _bindings[event] = std::bind(method, obj);
    }

    int32_t  mouse_x      {};
    int32_t  mouse_y      {};
    uint32_t mouse_down   {};
    uint8_t  mouse_button {};

private:
    std::unordered_map<SDL_EventType, std::function<void()>> _bindings {};
};

#endif // INPUT_HANDLER_HPP
