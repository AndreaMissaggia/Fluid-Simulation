#include "input_handler.hpp"
#include <SDL2/SDL_events.h>

InputHandler::InputHandler() {}

void InputHandler::handle_events()
{
    SDL_Event e {};
    while (SDL_PollEvent(&e) != 0)
    {
        switch (e.type)
        {
            case SDL_MOUSEMOTION:
                mouse_x = e.motion.x;
                mouse_y = e.motion.y;
            break;

            case SDL_MOUSEBUTTONDOWN:
                mouse_down = 1;
                mouse_button = e.button.button;
            break;

            case SDL_MOUSEBUTTONUP:
                mouse_down = 0;
                break;
        }

        if (_bindings.contains((SDL_EventType) e.type))
            _bindings[(SDL_EventType) e.type]();
    }
}

void InputHandler::add_binding(SDL_EventType event, std::function<void()> func)
{
    _bindings[event] = func;
}
