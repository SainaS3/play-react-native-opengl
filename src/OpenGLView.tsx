import { useState } from 'react';
import { Platform, requireNativeComponent, StyleSheet, Text, View, type ViewProps, type NativeSyntheticEvent } from 'react-native';
export type GLSettings = { model: string; color: string; spinning: boolean; flying: boolean; wireframe: boolean; reset: number };
type NativeProps = ViewProps & {
  model: string; meshColor: string; spinning: boolean; flying: boolean; wireframe: boolean; resetToken: number;
  onError: (event: NativeSyntheticEvent<{ message: string }>) => void;
};
const NativeGLView = Platform.OS === 'ios' || Platform.OS === 'windows' ? requireNativeComponent<NativeProps>('LegacyOpenGLView') : null;
export function OpenGLView(settings: GLSettings) {
  const [error, setError] = useState<string | null>(null);
  if (!NativeGLView) return <Text style={styles.message}>Use the Windows UWP or iOS native build to run the ANGLE renderer.</Text>;
  return <View style={styles.fill}>
    <NativeGLView style={styles.fill} model={settings.model} meshColor={settings.color} spinning={settings.spinning}
      flying={settings.flying} wireframe={settings.wireframe} resetToken={settings.reset}
      onError={event => setError(event.nativeEvent.message)} />
    {error && <View style={styles.error}><Text style={styles.message}>{error}</Text></View>}
  </View>;
}
const styles = StyleSheet.create({fill: {flex: 1}, error: {...StyleSheet.absoluteFillObject, backgroundColor: '#111b29', justifyContent: 'center'}, message: {padding: 20, color: '#eee8de'}});
