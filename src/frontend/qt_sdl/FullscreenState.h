#pragma once

enum class WindowRestoreState
{
    Normal,
    Maximized
};

inline WindowRestoreState getRestoreState(bool wasMaximized)
{
    return wasMaximized
        ? WindowRestoreState::Maximized
        : WindowRestoreState::Normal;
}
