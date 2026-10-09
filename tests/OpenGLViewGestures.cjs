const fs = require('fs');
const vm = require('vm');
const ts = require('typescript');
const assert = require('node:assert/strict');
const updates = [];
const rotations = [];
const source = ts.transpileModule(fs.readFileSync('src/OpenGLView.tsx', 'utf8'), {
  compilerOptions: { module: ts.ModuleKind.CommonJS, jsx: ts.JsxEmit.ReactJSX }
}).outputText;
const exportsObject = {};
const jsx = (type, props) => ({type, props});
const mockRequire = (name) => {
  if (name === 'react') return {
    useRef: current => ({current}), useLayoutEffect: fn => fn(),
    useState: initial => [initial, value => { if (value && 'x' in value) rotations.push(value); }],
  };
  if (name === 'react/jsx-runtime') return {jsx, jsxs: jsx};
  if (name === 'react-native') return {
    Platform: {OS: 'ios'}, requireNativeComponent: () => 'NativeGLView',
    StyleSheet: {create: value => value, absoluteFillObject: {}}, Text: 'Text', View: 'View',
  };
  throw Error(name);
};
vm.runInNewContext(source, {require: mockRequire, exports: exportsObject, Math, Number});
const tree = exportsObject.OpenGLView({model:'cone.obj', zoom:1, reset:0,
  onZoomChange: value => updates.push(value), onInteractionStart: () => {}});
const [native, overlay] = tree.props.children;
const touch = (x, y) => ({pageX:x, pageY:y});
const event = (...touches) => ({nativeEvent:{touches}});
const p = overlay.props;
p.onLayout({nativeEvent:{layout:{width:200,height:200}}});
p.onResponderGrant(event(touch(0,0)));
p.onResponderStart(event(touch(0,0),touch(100,0)));
p.onResponderMove(event(touch(0,0),touch(150,0)));
assert.equal(updates.at(-1),1.5);
p.onResponderMove(event(touch(0,0),touch(400,0)));
assert.equal(updates.at(-1),3);
p.onResponderMove(event(touch(0,0),touch(10,0)));
assert.equal(updates.at(-1),0.5);
const before = rotations.length;
p.onResponderEnd(event(touch(30,30)));
assert.equal(rotations.length,before, 'Lifting a pinch finger must rebase without rotating');
p.onResponderMove(event(touch(50,30)));
assert.ok(Math.abs(rotations.at(-1).y - Math.PI/10) < 1e-8);
p.onResponderRelease();
native.props.onZoom({nativeEvent:{factor:1.25}});
assert.equal(updates.at(-1),0.625);
native.props.onZoom({nativeEvent:{factor:NaN}});
assert.equal(updates.at(-1),0.625);
console.log('PASS: pinch scaling, limits, pinch-to-drag transition, wheel event and invalid input');
