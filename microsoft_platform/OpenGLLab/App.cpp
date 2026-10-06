#include "pch.hpp"

#include "App.hpp"

#include "AutolinkedNativeModules.g.h"
#include "ReactPackageProvider.hpp"
#include "AngleViewManager.hpp"

using namespace winrt;
using namespace xaml;
using namespace xaml::Controls;
using namespace xaml::Navigation;

using namespace Windows::ApplicationModel;
namespace winrt::OpenGLLab::implementation {
/// <summary>
/// Initializes the singleton application object.  This is the first line of
/// authored code executed, and as such is the logical equivalent of main() or
/// WinMain().
/// </summary>
App::App() noexcept {
    LogAngleHost("Starting React Native UWP host");
    InstanceSettings().NativeLogger([](Microsoft::ReactNative::LogLevel, hstring const &message) {
        LogAngleHost("React: " + to_string(message));
    });
    InstanceSettings().InstanceLoaded(
        [](Windows::Foundation::IInspectable const &,
           Microsoft::ReactNative::InstanceLoadedEventArgs const &args) {
            LogAngleHost(args.Failed() ? "React bundle failed to load" : "React bundle loaded");
        });
#if BUNDLE
    JavaScriptBundleFile(L"index.windows");
    InstanceSettings().UseFastRefresh(false);
#else
    JavaScriptBundleFile(L"index");
    InstanceSettings().UseFastRefresh(true);
#endif

#if _DEBUG
    InstanceSettings().UseDirectDebugger(true);
    InstanceSettings().UseDeveloperSupport(true);
#else
    InstanceSettings().UseDirectDebugger(false);
    InstanceSettings().UseDeveloperSupport(false);
#endif

    RegisterAutolinkedNativeModulePackages(PackageProviders()); // Includes any autolinked modules

    PackageProviders().Append(make<ReactPackageProvider>()); // Includes all modules in this project

    InitializeComponent();
}

/// <summary>
/// Invoked when the application is launched normally by the end user.  Other entry points
/// will be used such as when the application is launched to open a specific file.
/// </summary>
/// <param name="e">Details about the launch request and process.</param>
void App::OnLaunched(activation::LaunchActivatedEventArgs const &e) {
    super::OnLaunched(e);

    Frame rootFrame = Window::Current().Content().as<Frame>();
    rootFrame.Navigate(xaml_typename<MainPage>(), box_value(e.Arguments()));
}

/// <summary>
/// Invoked when the application is activated by some means other than normal launching.
/// </summary>
void App::OnActivated(Activation::IActivatedEventArgs const &e) {
    auto preActivationContent = Window::Current().Content();
    super::OnActivated(e);
    if (!preActivationContent && Window::Current()) {
        Frame rootFrame = Window::Current().Content().as<Frame>();
        rootFrame.Navigate(xaml_typename<MainPage>(), nullptr);
    }
}

/// <summary>
/// Invoked when application execution is being suspended.  Application state is saved
/// without knowing whether the application will be terminated or resumed with the contents
/// of memory still intact.
/// </summary>
/// <param name="sender">The source of the suspend request.</param>
/// <param name="e">Details about the suspend request.</param>
void App::OnSuspending([[maybe_unused]] Windows::Foundation::IInspectable const &sender,
                       [[maybe_unused]] SuspendingEventArgs const &e) {
    // Save application state and stop any background activity
}

/// <summary>
/// Invoked when Navigation to a certain page fails
/// </summary>
/// <param name="sender">The Frame which failed navigation</param>
/// <param name="e">Details about the navigation failure</param>
void App::OnNavigationFailed(Windows::Foundation::IInspectable const &,
                             NavigationFailedEventArgs const &e) {
    throw hresult_error(E_FAIL, hstring(L"Failed to load Page ") + e.SourcePageType().Name);
}

} // namespace winrt::OpenGLLab::implementation
