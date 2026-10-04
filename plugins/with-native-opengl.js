const { withXcodeProject, withPodfile } = require('expo/config-plugins');
const { readdirSync, existsSync } = require('node:fs');
const path = require('node:path');

module.exports = config => {
  config = withPodfile(config, config => {
    const artifact = path.join(config.modRequest.projectRoot, 'native/vendor/MetalANGLE.xcframework');
    if (!existsSync(artifact)) throw new Error('MetalANGLE is missing. Run npm run angle:prepare before prebuild.');
    const pod = "  pod 'ViewerMetalANGLE', :path => '../native'";
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
    if (!project.pbxGroupByName('Resources')) {
      const group = project.addPbxGroup([], 'Resources', '');
      project.getPBXGroupByKey(mainGroup).children.push({ value: group.uuid, comment: 'Resources' });
    }
    // xcode's writer serializes an undefined path as the literal "undefined".
    // Keep this a virtual group, relative to the ios project root.
    delete project.pbxGroupByName('Resources').path;
    const sources = ['../native/LegacyOpenGLView.m'];
    const resources = readdirSync(path.join(config.modRequest.projectRoot, 'Rend Example Collection/Resources'))
      .filter(name => name.endsWith('.obj'))
      .map(name => `../Rend Example Collection/Resources/${name}`);
    resources.push('../Rend Example Collection/sVertexLighting.vsh', '../Rend Example Collection/sVertexLighting.fsh');
    for (const file of sources) if (!project.hasFile(file)) project.addSourceFile(file, { target }, mainGroup);
    for (const file of resources) if (!project.hasFile(file)) project.addResourceFile(file, { target });
    // Remove stale links when regenerating an existing Apple GLES project.
    project.removeFramework('GLKit.framework', { target });
    project.removeFramework('OpenGLES.framework', { target });
    return config;
  });
};
