#pragma once
#include "MainPage.g.h"
#include <winrt/Microsoft.ReactNative.h>

namespace winrt::OpenGLLab::implementation {
struct MainPage : MainPageT<MainPage> {
    MainPage();
};
} // namespace winrt::OpenGLLab::implementation

namespace winrt::OpenGLLab::factory_implementation {
struct MainPage : MainPageT<MainPage, implementation::MainPage> {};
} // namespace winrt::OpenGLLab::factory_implementation
