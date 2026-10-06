#include "pch.hpp"
#include "ReactPackageProvider.hpp"
#include "NativeModules.h"
#include "AngleViewManager.hpp"

using namespace winrt::Microsoft::ReactNative;

namespace winrt::OpenGLLab::implementation {

void ReactPackageProvider::CreatePackage(IReactPackageBuilder const &packageBuilder) noexcept {
    AddAttributedModules(packageBuilder, true);
    packageBuilder.AddViewManager(L"LegacyOpenGLView", [] { return MakeAngleViewManager(); });
}

} // namespace winrt::OpenGLLab::implementation
