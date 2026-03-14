#pragma once
#include <string>
#include <World.h>

class UIWindow {
public:
    bool isOpen = true;

    virtual ~UIWindow() = default;

    // Every derived window MUST implement this
    virtual void Draw(World& world) = 0;

    // Optional: Return the window's title
    virtual std::string GetName() const = 0;
};