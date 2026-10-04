# Custom renderer and painter ideas, within the original concept

This notebook proposes future experiments; none replaces the native OpenGL viewer.

## Start from the existing separation

React Native owns controls, document selection and tool settings. A native OpenGL ES surface owns mesh/stroke data, GPU resources and its display loop. Send coarse operations across the boundary rather than per-frame values. This is the same conceptual division as the original blog experiment.

For a painter, begin with one native brush and one layer. Keep confirmed input samples and active stroke preview in the native engine. React can select color/size, show layers and invoke undo. Measure input-to-display latency on your phone before adding a reconciler.

On the ANGLE branch, that GLES renderer executes through MetalANGLE and Metal. This backend adaptation is separate from the document/brush design and from a future React reconciler. See [Graphics concept](GRAPHICS-CONCEPT.md).

## Three rates

1. UI/document changes: layer order, completed strokes, tool selection, undo/redo.
2. Input samples: document-space coordinates, pressure, tilt, timestamp and pointer identity.
3. Native frame work: interpolate brush stamps, update dirty tiles and composite.

React state need not advance for every input sample or frame. At pointer down snapshot brush settings; during movement render a provisional stroke; at pointer up commit one stroke command/history entry and transfer its result. Avoid applying the preview and committed stroke twice, which darkens translucent paint.

Separate the saved Document from Viewport (zoom/pan/rotation), active ToolSession and Selection. Convert screen coordinates through the inverse viewport transform. Decide whether brush radius is measured in screen or document space.

## A later custom React renderer

An eventual domain tree could describe Layer/Stroke/Image objects. Its reconciler would translate changes into native scene commands, while the Objective-C/C++ graphics engine still draws frames. The renderer itself would not implement the brush or GPU shaders.

React's [reconciler documentation](https://raw.githubusercontent.com/facebook/react/main/packages/react-reconciler/README.md) describes experimental host configuration. Pin/test its React version pair. Begin with a recording backend to validate create/update/reorder/remove commands before touching GPU resources.

Responsibilities: detached object creation during speculative render; atomic commit batches to native; stable IDs/keys and layer ordering; invalidation; ownership of shared textures; lifecycle cleanup; refs; hit testing, capture/cancel and transformed input; scene reconstruction after graphics-context loss. Native rendering must not traverse a partly updated scene.

## Direct OpenGL brush pipeline

Start with textured quads or tessellated ribbons, fixed sample spacing relative to brush size and pressure interpolation. Define opacity per stamp versus per stroke. Erasing removes layer alpha rather than painting paper color. Decide premultiplied alpha and color-space policy explicitly.

A 4096² RGBA8 texture consumes 64 MiB. Ten such layers need about 640 MiB before history and preview buffers. Start small, then add tiles, dirty rectangles and bounded caches. Avoid synchronous GPU readback during drawing.

Undo can use deterministic stroke commands plus occasional raster checkpoints. Save brush versions and random seeds alongside samples. Persist versioned document content and asset references, not GL buffer handles.

## Experiments to do in order

1. Existing OBJ viewer: validate native mesh lifecycle, selection/reset and independent display loop.
2. One-layer native brush: pressure-aware samples, cancellation and latency measurement.
3. Layer compositor: order/opacity/erase and exact undo/redo; save/load roundtrip.
4. Custom React host tree: translate layer/stroke commits into native operations; test abandoned renders and reorder behavior.
5. Tile engine: bounded memory, dirty-region updates, context restoration and export.

Questions for later: raster painter or vector illustrator; flat canvas or painting on a 3D mesh; target device/canvas size; pressure/tilt brush requirements; renderer learning versus shipping an editor. Keep those decisions separate from restoring this original viewer concept.
