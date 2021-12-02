# PKAsset
A repository of build tools used to cook assets for use in PKRenderer.

## Shader Format
Converts glsl shader files to binary spirv files (**.pkshader**) along with vertex attribute & descriptor information.

## Mesh Format
Converts .obj files to binary files (**.pkmesh**) containing interleaved vertex data, index buffer & vertex buffer attributes.

## Planned Features
- Utilize [meshoptimizer](https://github.com/zeux/meshoptimizer) to optimize mesh data for rendering.
- Add support for gltf conversion.
- Implement lossless compression for output formats.
- Implement some form of asset packaging.

## Dependencies
- C++ 17 support required
- [mikktspace](http://www.mikktspace.com/)
- [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader)
- [SPIRV-Reflect](https://github.com/KhronosGroup/SPIRV-Reflect)
- [shaderc](https://github.com/google/shaderc)
	- Due to the large sizes of the static libraries they were omitted from this repository. You can get them from the link above