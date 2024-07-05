# PKAssetTools
A repository of build tools used to cook assets for use in PKRenderer.

## Features
- GLSL To spriv compilation.
- .obj to custom binary mesh format conversion.
- Lossless file compression (Huffman encoding).

## Shader Format
- Converts glsl shader files to **.pkshader** files.
- Supports multiple compilation (in source glsl: #pragma pk_multi_compile VARIANT0 VARIANT1).
- The following attributes are parsed & stored from the source glsl:
	- Vertex attributes.
	- Descriptor sets.
	- Material property layout.
	- Rasterization parameters (blend mode, cull face, depth test mode, etc.).
	- multi compile keyword map.

## Mesh Format
- Converts .obj files to binary files (**.pkmesh**) containing interleaved vertex data, index buffer & vertex buffer attributes.
- Optimizes output vertex & index buffers using zeux meshoptimizer.

## Planned Features
- Add support for gltf conversion.
- Implement some form of asset packaging.

## Dependencies
- C++ 17 support required
- [mikktspace](http://www.mikktspace.com/)
- [meshoptimizer](https://github.com/zeux/meshoptimizer)
- [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader)
- [SPIRV-Reflect](https://github.com/KhronosGroup/SPIRV-Reflect)
- [shaderc](https://github.com/google/shaderc)
	- Due to the large sizes of the static libraries they were omitted from this repository. You can get them from the link above.
	- shaderc_combined.lib & shaderc_combinedd.lib specifically.
