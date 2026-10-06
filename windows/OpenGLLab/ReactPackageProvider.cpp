#include "pch.h"
#include "ReactPackageProvider.h"
#include "NativeModules.h"
#include "AngleViewManager.h"

using namespace winrt::Microsoft::ReactNative;

namespace winrt::OpenGLLab::implementation
{

void ReactPackageProvider::CreatePackage(IReactPackageBuilder const &packageBuilder) noexcept
{
    AddAttributedModules(packageBuilder, true);
    packageBuilder.AddViewManager(L"LegacyOpenGLView", [] { return MakeAngleViewManager(); });
}

} // namespace winrt::OpenGLLab::implementation
