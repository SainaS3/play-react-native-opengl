Pod::Spec.new do |s|
  s.name = 'ViewerMetalANGLE'
  s.version = '0.1.0'
  s.summary = 'Local MetalANGLE XCFramework for the native React OpenGL viewer'
  s.homepage = 'https://github.com/SainaS3/angle'
  s.license = { :type => 'BSD', :file => 'vendor/LICENSE' }
  s.author = 'MetalANGLE contributors'
  s.source = { :git => 'https://github.com/SainaS3/angle.git' }
  s.ios.deployment_target = '13.0'
  s.vendored_frameworks = 'vendor/MetalANGLE.xcframework'
  s.user_target_xcconfig = {
    'HEADER_SEARCH_PATHS' => '$(inherited) "$(PODS_XCFRAMEWORKS_BUILD_DIR)/ViewerMetalANGLE/MetalANGLE.framework/Headers"'
  }
end
