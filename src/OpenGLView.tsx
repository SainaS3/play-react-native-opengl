import { useLayoutEffect, useRef, useState } from 'react';
import {
  Platform,
  requireNativeComponent,
  StyleSheet,
  Text,
  View,
  type ViewProps,
  type NativeSyntheticEvent,
} from 'react-native';
export type GLSettings = {
  model: string;
  color: string;
  spinning: boolean;
  flying: boolean;
  wireframe: boolean;
  reset: number;
  onInteractionStart: () => void;
};
type NativeProps = ViewProps & {
  model: string;
  meshColor: string;
  spinning: boolean;
  flying: boolean;
  wireframe: boolean;
  resetToken: number;
  rotationX: number;
  rotationY: number;
  onError: (event: NativeSyntheticEvent<{ message: string }>) => void;
};
const NativeGLView =
  Platform.OS === 'ios' || Platform.OS === 'windows'
    ? requireNativeComponent<NativeProps>('LegacyOpenGLView')
    : null;
export function OpenGLView(settings: GLSettings) {
  const [rotation, setRotation] = useState({ x: 0, y: 0 });
  const rotationRef = useRef(rotation);
  const drag = useRef<{ x: number; y: number; rotationX: number; rotationY: number } | null>(null);
  const viewport = useRef({ width: 1, height: 1 });
  useLayoutEffect(() => {
    const initial = { x: 0, y: 0 };
    rotationRef.current = initial;
    drag.current = null;
    setRotation(initial);
  }, [settings.model, settings.reset]);
  const [error, setError] = useState<string | null>(null);
  if (!NativeGLView)
    return (
      <Text style={styles.message}>
        Use the Windows UWP or iOS native build to run the ANGLE renderer.
      </Text>
    );
  return (
    <View style={styles.fill}>
      <NativeGLView
        style={styles.fill}
        model={settings.model}
        meshColor={settings.color}
        spinning={settings.spinning}
        flying={settings.flying}
        wireframe={settings.wireframe}
        resetToken={settings.reset}
        rotationX={rotation.x}
        rotationY={rotation.y}
        onError={(event) => setError(event.nativeEvent.message)}
      />
      <View
        style={styles.gestureSurface}
        onLayout={(event) => {
          const { width, height } = event.nativeEvent.layout;
          viewport.current = { width: Math.max(1, width), height: Math.max(1, height) };
        }}
        onStartShouldSetResponder={() => !error}
        onResponderGrant={(event) => {
          const { pageX, pageY } = event.nativeEvent;
          drag.current = {
            x: pageX, y: pageY,
            rotationX: rotationRef.current.x, rotationY: rotationRef.current.y,
          };
          settings.onInteractionStart();
        }}
        onResponderMove={(event) => {
          const start = drag.current;
          if (!start) return;
          const { pageX, pageY } = event.nativeEvent;
          const size = Math.min(viewport.current.width, viewport.current.height);
          const next = {
            x: start.rotationX + ((pageY - start.y) / size) * Math.PI,
            y: start.rotationY + ((pageX - start.x) / size) * Math.PI,
          };
          rotationRef.current = next;
          setRotation(next);
        }}
        onResponderRelease={() => { drag.current = null; }}
        onResponderTerminate={() => { drag.current = null; }}
        onResponderTerminationRequest={() => false}
      />
      {error && (
        <View style={styles.error}>
          <Text style={styles.message}>{error}</Text>
        </View>
      )}
    </View>
  );
}
const styles = StyleSheet.create({
  fill: { flex: 1 },
  // XAML needs a background brush to hit-test an otherwise empty overlay.
  gestureSurface: { ...StyleSheet.absoluteFillObject, backgroundColor: 'transparent' },
  error: { ...StyleSheet.absoluteFillObject, backgroundColor: '#111b29', justifyContent: 'center' },
  message: { padding: 20, color: '#eee8de' },
});
