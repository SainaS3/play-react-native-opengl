import { withXcodeProject, withPodfile, type ConfigPlugin } from 'expo/config-plugins';
import { readdirSync, existsSync } from 'node:fs';
import path from 'node:path';

const withNativeOpenGL: ConfigPlugin = (config) => {
  config = withPodfile(config, (config) => {
    const artifact = path.join(
      config.modRequest.projectRoot,
      'apple_platform/vendor/MetalANGLE.xcframework',
    );
    if (!existsSync(artifact))
      throw new Error('MetalANGLE is missing. Run npm run angle:prepare before prebuild.');
    const pod = "  pod 'ViewerMetalANGLE', :path => '../apple_platform'";
    // Update generated Podfiles from the previous directory layout.
    config.modResults.contents = config.modResults.contents.replace(
      /pod 'ViewerMetalANGLE', :path => '\.\.\/native'/g,
      "pod 'ViewerMetalANGLE', :path => '../apple_platform'",
    );
    config.modResults.contents = config.modResults.contents.replace(
      /:mac_catalyst_enabled => false/g,
      ':mac_catalyst_enabled => true',
    );
    if (!config.modResults.contents.includes(pod)) {
      const anchor = '  use_expo_modules!';
      if (!config.modResults.contents.includes(anchor))
        throw new Error('Cannot locate app target in Podfile');
      config.modResults.contents = config.modResults.contents.replace(anchor, `${anchor}\n${pod}`);
    }
    return config;
  });
  return withXcodeProject(config, (config) => {
    const project = config.modResults;
    const target = project.getFirstTarget().uuid;
    const mainGroup = project.getFirstProject().firstProject.mainGroup;
    const configurations = project.pbxXCBuildConfigurationSection();
    for (const configuration of Object.values(configurations)) {
      if (
        typeof configuration === 'string' ||
        typeof configuration.buildSettings.PRODUCT_NAME !== 'string' ||
        configuration.buildSettings.PRODUCT_NAME.replace(/"/g, '') !== 'ReactOpenGLLab'
      )
        continue;
      configuration.buildSettings.SUPPORTS_MACCATALYST = 'YES';
      configuration.buildSettings.SUPPORTS_MAC_DESIGNED_FOR_IPHONE_IPAD = 'NO';
      configuration.buildSettings.DERIVE_MACCATALYST_PRODUCT_BUNDLE_IDENTIFIER = 'YES';
    }
    if (!project.pbxGroupByName('Resources')) {
      const group = project.addPbxGroup([], 'Resources', '');
      const main = project.getPBXGroupByKey(mainGroup);
      if (!main) throw new Error('Cannot locate the main Xcode group');
      main.children.push({ value: group.uuid, comment: 'Resources' });
    }
    // xcode's writer serializes an undefined path as the literal "undefined".
    // Keep this a virtual group, relative to the ios project root.
    const resourcesGroup = project.pbxGroupByName('Resources');
    if (!resourcesGroup) throw new Error('Cannot locate the Xcode Resources group');
    delete resourcesGroup.path;
    // node-xcode removal matches basenames, which can also delete a relocated file.
    // Match full paths and remove every dependent reference instead.
    const removeFileAtPath = (file: string) => {
      const objects = project.hash.project.objects;
      const references = project.pbxFileReferenceSection();
      for (const [uuid, reference] of Object.entries(references)) {
        if (typeof reference === 'string' || typeof reference.path !== 'string' ||
            reference.path.replace(/^"|"$/g, '') !== file) continue;
        const removed = new Set([uuid]);
        const buildFiles = project.pbxBuildFileSection();
        for (const [buildUuid, buildFile] of Object.entries(buildFiles)) {
          if (typeof buildFile === 'string' || buildFile.fileRef !== uuid) continue;
          removed.add(buildUuid);
          delete buildFiles[buildUuid];
          delete buildFiles[`${buildUuid}_comment`];
        }
        for (const section of Object.values(objects)) {
          for (const entry of Object.values(section ?? {})) {
            if (!entry || typeof entry !== 'object') continue;
            for (const key of ['files', 'children']) {
              if (Array.isArray(entry[key]))
                entry[key] = entry[key].filter((item) => !removed.has(item.value));
            }
          }
        }
        delete references[uuid];
        delete references[`${uuid}_comment`];
      }
    };
    // Remove historical adapter names when upgrading an existing generated project.
    for (const file of [
      '../native/LegacyOpenGLView.m',
      '../native/LegacyOpenGLView.mm',
      '../native/shared/ViewerRenderer.cpp',
      '../apple_platform/LegacyOpenGLView.m',
      '../apple_platform/LegacyOpenGLView.mm',
      '../apple_platform/LegacyOpenGLViewManager.mm',
    ]) {
      removeFileAtPath(file);
    }
    const sources = [
      '../apple_platform/AngleView.mm',
      '../apple_platform/AngleViewManager.mm',
      '../shared/renderer/ViewerRenderer.cpp',
    ];
    const resources = readdirSync(
      path.join(config.modRequest.projectRoot, 'shared/resources/models'),
    )
      .filter((name) => name.endsWith('.obj'))
      .map((name) => `../shared/resources/models/${name}`);
    resources.push(
      '../shared/resources/shaders/sVertexLighting.vsh',
      '../shared/resources/shaders/sVertexLighting.fsh',
    );
    // Migrate existing generated projects before adding the relocated resources.
    for (const reference of Object.values(project.pbxFileReferenceSection())) {
      if (typeof reference === 'string' || typeof reference.path !== 'string') continue;
      const file = reference.path.replace(/^"|"$/g, '');
      if (
        file.startsWith('../Rend Example Collection/') ||
        file.startsWith('../native/resources/')
      ) {
        removeFileAtPath(file);
      }
    }
    for (const file of sources) {
      if (!project.hasFile(file)) project.addSourceFile(file, { target }, mainGroup);
      // node-xcode does not infer the Objective-C++ type for .mm files.
      for (const reference of Object.values(project.pbxFileReferenceSection())) {
        if (typeof reference !== 'string' && typeof reference.path === 'string' &&
            reference.path.replace(/^"|"$/g, '') === file)
          reference.lastKnownFileType = file.endsWith('.mm')
            ? 'sourcecode.cpp.objcpp'
            : 'sourcecode.cpp.cpp';
      }
    }
    for (const file of resources)
      if (!project.hasFile(file)) project.addResourceFile(file, { target });
    // Remove stale links when regenerating an existing Apple GLES project.
    project.removeFramework('GLKit.framework', { target });
    project.removeFramework('OpenGLES.framework', { target });
    return config;
  });
};

export default withNativeOpenGL;
