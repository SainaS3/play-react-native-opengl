// Preserve Expo's platform defaults for the Apple and web application.
const { getDefaultConfig } = require('expo/metro-config');
module.exports = getDefaultConfig(__dirname);
