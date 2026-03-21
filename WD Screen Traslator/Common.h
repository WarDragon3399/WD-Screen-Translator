#pragma once
#include <windows.h>
#include <string>

// Define the struct only ONCE here
struct TextBlock {
    std::wstring text;
    RECT box;
};