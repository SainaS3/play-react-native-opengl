# Original viewer resources

These assets were relocated from `Rend Example Collection/` when the obsolete Rend source and Xcode project were removed. Their contents are unchanged.

- `models/`: ten original OBJ models and their MTL companions. The native viewer loads OBJ positions/faces and supplies its own selected diffuse color; it does not load MTL materials.
- `shaders/`: the original `sVertexLighting.vsh` and `.fsh`, including their author attribution and license text. These are the two shaders used by the current viewer.

The Xcode config plugin bundles OBJ files and shaders by their original filenames, so native NSBundle lookups keep working. The generator reads the OBJ filenames here to create the React model list. Historical example code and asset provenance can be inspected in Git history, including commit `6ac8b4c`.
