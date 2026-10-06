#pragma once
#include <winrt/Microsoft.ReactNative.h>
#include <string>
winrt::Microsoft::ReactNative::IViewManager MakeAngleViewManager();
void LogAngleHost(std::string const &message) noexcept;
