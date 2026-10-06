import { Component, type ReactNode, useState } from 'react';
import {
  View,
  Text,
  TextInput,
  Pressable,
  ScrollView,
  StyleSheet,
  StatusBar,
  Platform,
} from 'react-native';
import { SafeAreaProvider, SafeAreaView } from 'react-native-safe-area-context';
import { OpenGLView } from './OpenGLView';
import modelNames from './generated/modelNames.json';

class SceneBoundary extends Component<{ children: ReactNode }, { error: string | null }> {
  state = { error: null as string | null };
  static getDerivedStateFromError(error: Error) {
    return { error: error.message };
  }
  render() {
    return this.state.error ? (
      <View style={styles.error}>
        <Text style={styles.body}>GPU scene unavailable: {this.state.error}</Text>
        <Text style={styles.body}>
          Use the Windows UWP or iOS native build. See docs/UWP.md or docs/SETUP.md.
        </Text>
      </View>
    ) : (
      this.props.children
    );
  }
}

function Button({
  label,
  onPress,
  active = false,
}: {
  label: string;
  onPress: () => void;
  active?: boolean;
}) {
  return (
    <Pressable
      accessibilityRole="button"
      accessibilityState={{ selected: active }}
      onPress={onPress}
      style={[styles.button, active && styles.active]}
    >
      <Text style={styles.buttonText}>{label}</Text>
    </Pressable>
  );
}

export default function App() {
  const [model, setModel] = useState('cone.obj');
  const [query, setQuery] = useState('');
  const [spinning, setSpinning] = useState(true);
  const [flying, setFlying] = useState(false);
  const [wireframe, setWireframe] = useState(false);
  const [reset, setReset] = useState(0);
  const [color, setColor] = useState('#e8b56b');
  function choose(name: string) {
    setModel(name);
    setFlying(false);
    setReset((value) => value + 1);
  }
  return (
    <SafeAreaProvider>
      <SafeAreaView style={styles.root}>
        <StatusBar barStyle="light-content" />
        <View style={styles.header}>
          <Text style={styles.eyebrow}>REACT × GPU / EXPERIMENT 01</Text>
          <Text style={styles.title}>React controls. Native OpenGL.</Text>
          <Text style={styles.body}>
            The original OBJ viewer concept: React UI over native OpenGL ES.
          </Text>
        </View>
        <View style={styles.viewport}>
          <SceneBoundary>
            <OpenGLView {...{ model, color, spinning, flying, wireframe, reset }} />
          </SceneBoundary>
          <View pointerEvents="none" style={styles.caption}>
            <Text style={styles.model}>{model}</Text>
            <Text style={styles.small}>
              {Platform.OS === 'windows'
                ? 'C++ / ANGLE owns the scene and animation loop'
                : 'Objective-C owns the scene and animation loop'}
            </Text>
          </View>
        </View>
        <ScrollView
          style={styles.controls}
          contentContainerStyle={styles.controlsContent}
          keyboardShouldPersistTaps="handled"
        >
          <View style={styles.row}>
            <Button
              label={spinning ? 'Pause' : 'Rotate'}
              active={spinning}
              onPress={() => setSpinning(!spinning)}
            />
            <Button label="Fly" active={flying} onPress={() => setFlying(!flying)} />
            <Button
              label="Reset"
              onPress={() => {
                setFlying(false);
                setReset((value) => value + 1);
              }}
            />
            <Button label="Wireframe" active={wireframe} onPress={() => setWireframe(!wireframe)} />
            <Button
              label="Random"
              onPress={() => choose(modelNames[Math.floor(Math.random() * modelNames.length)])}
            />
          </View>
          <View style={styles.row}>
            {['#e8b56b', '#7bbaa3', '#8ba8ef', '#d88fb1'].map((value) => (
              <Pressable
                key={value}
                accessibilityRole="button"
                accessibilityLabel={`Color ${value}`}
                onPress={() => setColor(value)}
                style={[
                  styles.swatch,
                  { backgroundColor: value },
                  color === value && styles.selectedSwatch,
                ]}
              />
            ))}
          </View>
          <TextInput
            accessibilityLabel="Search models"
            placeholder="Search the original OBJ collection…"
            placeholderTextColor="#8291a5"
            value={query}
            onChangeText={setQuery}
            style={styles.search}
          />
          <View style={styles.row}>
            {modelNames
              .filter((name) => name.toLowerCase().includes(query.toLowerCase()))
              .map((name) => (
                <Button
                  key={name}
                  label={name}
                  active={name === model}
                  onPress={() => choose(name)}
                />
              ))}
          </View>
          {!modelNames.some((name) => name.toLowerCase().includes(query.toLowerCase())) && (
            <Text style={styles.body}>No matching models.</Text>
          )}
        </ScrollView>
      </SafeAreaView>
    </SafeAreaProvider>
  );
}

const styles = StyleSheet.create({
  root: { flex: 1, backgroundColor: '#0b121c' },
  header: { padding: 20, gap: 8 },
  eyebrow: { color: '#e8b56b', fontSize: 10, letterSpacing: 2 },
  title: { color: '#f3eee5', fontSize: 26, fontWeight: '600' },
  body: { color: '#a8b5c6', fontSize: 13, lineHeight: 20 },
  viewport: {
    flex: 1,
    minHeight: 220,
    backgroundColor: '#111b29',
    marginHorizontal: 16,
    borderRadius: 16,
    overflow: 'hidden',
  },
  caption: { position: 'absolute', bottom: 16, left: 16, gap: 4 },
  model: { color: '#f3eee5', fontSize: 18, fontWeight: '600' },
  small: { color: '#8291a5', fontSize: 11, lineHeight: 18 },
  controls: { flexGrow: 0, maxHeight: 300 },
  controlsContent: { padding: 20, gap: 16 },
  row: { flexDirection: 'row', flexWrap: 'wrap', gap: 8 },
  button: {
    paddingHorizontal: 13,
    paddingVertical: 10,
    borderRadius: 8,
    backgroundColor: '#1b2737',
    borderWidth: 1,
    borderColor: '#2a394e',
  },
  active: { backgroundColor: '#3b3022', borderColor: '#e8b56b' },
  buttonText: { color: '#eee8de', fontSize: 12 },
  swatch: { width: 28, height: 28, borderRadius: 14 },
  selectedSwatch: { borderWidth: 2, borderColor: '#fff' },
  search: {
    color: '#f3eee5',
    backgroundColor: '#1b2737',
    borderRadius: 8,
    padding: 12,
    fontSize: 13,
  },
  error: { flex: 1, justifyContent: 'center', padding: 20, gap: 12 },
});
