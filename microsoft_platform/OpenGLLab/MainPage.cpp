#include "pch.hpp"
#include "MainPage.hpp"
#if __has_include("MainPage.g.cpp")
#include "MainPage.g.cpp"
#endif

#include "App.hpp"

using namespace winrt;
using namespace xaml;

namespace winrt::OpenGLLab::implementation {
MainPage::MainPage() {
    InitializeComponent();
    auto app = Application::Current().as<App>();
    ReactRootView().ReactNativeHost(app->Host());
}
} // namespace winrt::OpenGLLab::implementation
