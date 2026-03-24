# Downloaded Source Assets

This folder stores externally downloaded source assets for future integration.

## Poly Haven

Downloaded on 2026-03-24:

- `materials/concrete_wall_006/`
  - `concrete_wall_006_diff_2k.jpg`
  - `concrete_wall_006_rough_2k.jpg`
  - `concrete_wall_006_nor_gl_2k.jpg`
- `materials/concrete_floor/`
  - `concrete_floor_diff_2k.jpg`
  - `concrete_floor_rough_2k.jpg`
  - `concrete_floor_nor_gl_2k.jpg`
- `materials/painted_concrete/`
  - `painted_concrete_diff_2k.jpg`
  - `painted_concrete_rough_2k.jpg`
  - `painted_concrete_nor_gl_2k.jpg`
- `models/wooden_crate_02/`
  - `wooden_crate_02_1k.gltf`
  - `wooden_crate_02.bin`
  - `textures/wooden_crate_02_diff_1k.jpg`
  - `textures/wooden_crate_02_arm_1k.jpg`
  - `textures/wooden_crate_02_nor_gl_1k.jpg`
- `models/Barrel_02/`
  - `Barrel_02_1k.gltf`
  - `Barrel_02.bin`
  - `textures/Barrel_02_diff_1k.jpg`
  - `textures/Barrel_02_arm_1k.jpg`
  - `textures/Barrel_02_nor_gl_1k.jpg`

Each asset folder also contains `_meta.json` from the Poly Haven API.

## Weapon Pack Lead

Potential free gun pack source found:

- Quaternius Ultimate Guns
  - Page: `https://quaternius.com/packs/ultimategun.html`
  - Download flow currently points to a Google Drive folder, which was not script-friendly in this environment.

## Integration Note

These assets are downloaded and ready, but the project does not yet have a full external mesh/material loading pipeline wired into the Vulkan renderer. They are staged here for the next asset-integration pass.
