import fs from 'node:fs';
import vm from 'node:vm';
import ts from 'typescript';
import assert from 'node:assert/strict';
import type { GLSettings } from '../src/AngleView';
type Rotation = {
    x: number;
    y: number;
};
type Touch = {
    pageX: number;
    pageY: number;
};
type GestureEvent = {
    nativeEvent: {
        touches: Touch[];
    };
};
type OverlayHandlers = {
    onLayout: (event: {
        nativeEvent: {
            layout: {
                width: number;
                height: number;
            };
        };
    }) => void;
    onResponderGrant: (event: GestureEvent) => void;
    onResponderStart: (event: GestureEvent) => void;
    onResponderMove: (event: GestureEvent) => void;
    onResponderEnd: (event: GestureEvent) => void;
    onResponderRelease: () => void;
};
type NativeHandlers = {
    onZoom: (event: {
        nativeEvent: {
            factor: number;
        };
    }) => void;
};
type TestTree = {
    props: {
        children: [
            {
                props: NativeHandlers;
            },
            {
                props: OverlayHandlers;
            }
        ];
    };
};
const updates: number[] = [];
const rotations: Rotation[] = [];
const source = ts.transpileModule(fs.readFileSync('src/AngleView.tsx', 'utf8'), {
    compilerOptions: { module: ts.ModuleKind.CommonJS, jsx: ts.JsxEmit.ReactJSX }
}).outputText;
const exportsObject = {} as {
    AngleView: (settings: GLSettings) => TestTree;
};
const jsx = (type: unknown, props: unknown) => ({ type, props });
const mockRequire = (name: string) => {
    if (name === 'react')
        return {
            useRef: (current: unknown) => ({ current }),
            useLayoutEffect: (fn: () => void) => fn(),
            useState: (initial: unknown) => [initial, (value: unknown) => {
                    if (value && typeof value === 'object' && 'x' in value && 'y' in value &&
                        typeof value.x === 'number' && typeof value.y === 'number') {
                        rotations.push({ x: value.x, y: value.y });
                    }
                }],
        };
    if (name === 'react/jsx-runtime')
        return { jsx, jsxs: jsx };
    if (name === 'react-native')
        return {
            Platform: { OS: 'ios' }, requireNativeComponent: () => 'NativeGLView',
            StyleSheet: { create: (value: unknown) => value, absoluteFillObject: {} }, Text: 'Text', View: 'View',
        };
    throw Error(name);
};
vm.runInNewContext(source, { require: mockRequire, exports: exportsObject, Math, Number });
const tree = exportsObject.AngleView({ model: 'cone.obj', color: '#e8b56b', spinning: false, flying: false, wireframe: false, zoom: 1, reset: 0,
    onZoomChange: value => updates.push(value), onInteractionStart: () => { } });
const [native, overlay] = tree.props.children;
const touch = (x: number, y: number): Touch => ({ pageX: x, pageY: y });
const event = (...touches: Touch[]): GestureEvent => ({ nativeEvent: { touches } });
const p = overlay.props;
p.onLayout({ nativeEvent: { layout: { width: 200, height: 200 } } });
p.onResponderGrant(event(touch(0, 0)));
p.onResponderStart(event(touch(0, 0), touch(100, 0)));
p.onResponderMove(event(touch(0, 0), touch(150, 0)));
assert.equal(updates.at(-1), 1.5);
p.onResponderMove(event(touch(0, 0), touch(400, 0)));
assert.equal(updates.at(-1), 3);
p.onResponderMove(event(touch(0, 0), touch(10, 0)));
assert.equal(updates.at(-1), 0.5);
const before = rotations.length;
p.onResponderEnd(event(touch(30, 30)));
assert.equal(rotations.length, before, 'Lifting a pinch finger must rebase without rotating');
p.onResponderMove(event(touch(50, 30)));
assert.ok(Math.abs(rotations.at(-1)!.y - Math.PI / 10) < 1e-8);
p.onResponderRelease();
native.props.onZoom({ nativeEvent: { factor: 1.25 } });
assert.equal(updates.at(-1), 0.625);
native.props.onZoom({ nativeEvent: { factor: NaN } });
assert.equal(updates.at(-1), 0.625);
console.log('PASS: pinch scaling, limits, pinch-to-drag transition, wheel event and invalid input');
