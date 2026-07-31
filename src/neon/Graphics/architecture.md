
# Entrypoint
`neon::gfx::Init()`

The `DeviceResources` struct contains all data owned by the render device.

`D3D12MA` is used for allocating heaps and creating resources - buffers and textures.

Resources will need to be Cleared, Discarded, or Copied before use.