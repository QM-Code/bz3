# src/engine/geometry/AGENTS.md

Read `src/engine/AGENTS.md` first.
This directory houses **mesh loading utilities** used by render backends and
resource management.

## What’s here
- `mesh_loader.*`
  - Loads meshes from disk (GLTF/GLB via Assimp).
  - Produces vertex/index buffers + texture references.

## How it connects
- Renderer/graphics backends call into mesh loader when models are requested.
- Game code typically does not call mesh loader directly.

## Common tasks
- Add mesh formats: extend loader and ensure backend compatibility.
- Fix asset loading: start with the loader here.
