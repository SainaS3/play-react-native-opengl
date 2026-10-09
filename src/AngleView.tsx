import { useLayoutEffect, useRef, useState } from 'react';
import {
  Platform,
  requireNativeComponent,
  StyleSheet,
  Text,
  View,
  type ViewProps,
  type GestureResponderEvent,
  type NativeSyntheticEvent,
} from 'react-native';
export type GLSettings = {
  model: string;
  color: string;
  spinning: boolean;
  flying: boolean;
  wireframe: boolean;
  reset: number;
  zoom: number;
  onZoomChange: (zoom: number) => void;
  onInteractionStart: () => void;
};
type NativeProps = ViewProps & {
  model: string;
  meshColor: string;
  spinning: boolean;
  flying: boolean;
  wireframe: boolean;
  resetToken: number;
  zoom: number;
  rotationX: number;
  rotationY: number;
  onZoom: (event: NativeSyntheticEvent<{ factor: number }>) => void;
  onError: (event: NativeSyntheticEvent<{ message: string }>) => void;
};
const NativeGLView =
  Platform.OS === 'ios' || Platform.OS === 'windows'
    ? requireNativeComponent<NativeProps>('AngleView')
    : null;
export function AngleView(settings: GLSettings) {
  const [rotation, setRotation] = useState({ x: 0, y: 0 });
  const rotationRef = useRef(rotation);
  const drag = useRef<{ x: number; y: number; rotationX: number; rotationY: number } | null>(null);
  const pinch = useRef<{ distance: number; zoom: number } | null>(null);
  const zoomRef = useRef(settings.zoom);
  useLayoutEffect(() => { zoomRef.current = settings.zoom; }, [settings.zoom]);
  function changeZoom(value: number) {
    if (!Number.isFinite(value)) return;
    zoomRef.current = Math.max(0.5, Math.min(3, value));
    settings.onZoomChange(zoomRef.current);
  }
  function handleGesture(event: GestureResponderEvent) {
    const touches = event.nativeEvent.touches;
    if (touches.length >= 2) {
      drag.current = null;
      const [a, b] = touches;
      const distance = Math.hypot(a.pageX - b.pageX, a.pageY - b.pageY);
      if (!pinch.current) {
        if (distance > 0) pinch.current = { distance, zoom: zoomRef.current };
      } else {
        changeZoom(pinch.current.zoom * distance / pinch.current.distance);
      }
      return;
    }
    pinch.current = null;
    if (touches.length === 0) {
      drag.current = null;
      return;
    }
    const { pageX, pageY } = touches[0];
    const start = drag.current;
    if (!start) {
      drag.current = { x: pageX, y: pageY,
        rotationX: rotationRef.current.x, rotationY: rotationRef.current.y };
      return;
    }
    const size = Math.min(viewport.current.width, viewport.current.height);
    const next = {
      x: start.rotationX + ((pageY - start.y) / size) * Math.PI,
      y: start.rotationY + ((pageX - start.x) / size) * Math.PI,
    };
    rotationRef.current = next;
    setRotation(next);
  }
  function endGesture() {
    drag.current = null;
    pinch.current = null;
  }
  const viewport = useRef({ width: 1, height: 1 });
  useLayoutEffect(() => {
    const initial = { x: 0, y: 0 };
    rotationRef.current = initial;
    endGesture();
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
        zoom={settings.zoom}
        rotationX={rotation.x}
        rotationY={rotation.y}
        onZoom={(event) => changeZoom(zoomRef.current * event.nativeEvent.factor)}
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
          endGesture();
          handleGesture(event);
          settings.onInteractionStart();
        }}
        onResponderStart={handleGesture}
        onResponderMove={handleGesture}
        onResponderEnd={handleGesture}
        onResponderRelease={endGesture}
        onResponderTerminate={endGesture}
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
