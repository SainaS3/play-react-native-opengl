const { withXcodeProject } = require('expo/config-plugins');
const { readdirSync } = require('node:fs');
const path = require('node:path');

module.exports = config => withXcodeProject(config, config => {
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
  project.addFramework('GLKit.framework', { target });
  project.addFramework('OpenGLES.framework', { target });
  return config;
});
