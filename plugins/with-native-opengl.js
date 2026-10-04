const { withXcodeProject, withPodfile } = require('expo/config-plugins');
const { readdirSync, existsSync } = require('node:fs');
const path = require('node:path');

module.exports = config => {
  config = withPodfile(config, config => {
    const artifact = path.join(config.modRequest.projectRoot, 'native/vendor/MetalANGLE.xcframework');
    if (!existsSync(artifact)) throw new Error('MetalANGLE is missing. Run npm run angle:prepare before prebuild.');
    const pod = "  pod 'ViewerMetalANGLE', :path => '../native'";
    config.modResults.contents = config.modResults.contents.replace(
      /:mac_catalyst_enabled => false/g, ':mac_catalyst_enabled => true'
    );
    if (!config.modResults.contents.includes(pod)) {
      const anchor = '  use_expo_modules!';
      if (!config.modResults.contents.includes(anchor)) throw new Error('Cannot locate app target in Podfile');
      config.modResults.contents = config.modResults.contents.replace(anchor, `${anchor}\n${pod}`);
    }
    return config;
  });
  return withXcodeProject(config, config => {
    const project = config.modResults;
    const target = project.getFirstTarget().uuid;
    const mainGroup = project.getFirstProject().firstProject.mainGroup;
    const configurations = project.pbxXCBuildConfigurationSection();
    for (const configuration of Object.values(configurations)) {
      if (!configuration.buildSettings || configuration.buildSettings.PRODUCT_NAME?.replace(/"/g, '') !== 'ReactOpenGLLab') continue;
      configuration.buildSettings.SUPPORTS_MACCATALYST = 'YES';
      configuration.buildSettings.SUPPORTS_MAC_DESIGNED_FOR_IPHONE_IPAD = 'NO';
      configuration.buildSettings.DERIVE_MACCATALYST_PRODUCT_BUNDLE_IDENTIFIER = 'YES';
    }
    if (!project.pbxGroupByName('Resources')) {
      const group = project.addPbxGroup([], 'Resources', '');
      project.getPBXGroupByKey(mainGroup).children.push({ value: group.uuid, comment: 'Resources' });
    }
    // xcode's writer serializes an undefined path as the literal "undefined".
    // Keep this a virtual group, relative to the ios project root.
    delete project.pbxGroupByName('Resources').path;
    const sources = ['../native/LegacyOpenGLView.m'];
    const resources = readdirSync(path.join(config.modRequest.projectRoot, 'native/resources/models'))
      .filter(name => name.endsWith('.obj'))
      .map(name => `../native/resources/models/${name}`);
    resources.push('../native/resources/shaders/sVertexLighting.vsh', '../native/resources/shaders/sVertexLighting.fsh');
    // Migrate existing generated projects before adding the relocated resources.
    for (const reference of Object.values(project.pbxFileReferenceSection())) {
      if (!reference || typeof reference !== 'object' || !reference.path) continue;
      const file = reference.path.replace(/^"|"$/g, '');
      if (file.startsWith('../Rend Example Collection/')) project.removeResourceFile(file, { target });
    }
    for (const file of sources) if (!project.hasFile(file)) project.addSourceFile(file, { target }, mainGroup);
    for (const file of resources) if (!project.hasFile(file)) project.addResourceFile(file, { target });
    // Remove stale links when regenerating an existing Apple GLES project.
    project.removeFramework('GLKit.framework', { target });
    project.removeFramework('OpenGLES.framework', { target });
    return config;
  });
};
