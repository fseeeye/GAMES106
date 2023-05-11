# GAMES 106 - 作业1

## 作业要求与基本情况

作业要求：
- 支持gltf的骨骼动画。
- 支持gltf的PBR的材质，包括法线贴图等。
- 进阶作业：
	- 增加一个Tone Mapping的后处理 Render Pass，用于线性颜色的转换。tonemap选择ACES实现如下:
		``` C++
		// tonemap 所使用的函数
		float3 Tonemap_ACES(const float3 c) {
			// Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
			// const float a = 2.51;
			// const float b = 0.03;
			// const float c = 2.43;
			// const float d = 0.59;
			// const float e = 0.14;
			// return saturate((x*(a*x+b))/(x*(c*x+d)+e));

			//ACES RRT/ODT curve fit courtesy of Stephen Hill
			float3 a = c * (c + 0.0245786) - 0.000090537;
			float3 b = c * (0.983729 * c + 0.4329510) + 0.238081;
			return a / b;
		}
		```
	- 增加GLTF的滤镜功能。

作业初始状态：
- 读取了 GLTF 文件，但是模型是静态的，因为没有读取骨骼动画。
- BRDF 实现存在问题，仅将颜色纹理作为材质 + 仅一个方向光，没有实现 PBR 材质。

## TODO List

- [ ] 载入 GLTF 缺失的数据。
	- [X] 载入动画数据。
	- [ ] 读取纹理贴图(Texture mapping)：法线、自发光、PBR。
	- [ ] 更新数据至 mesh Uniform Buffer & Texture Sampler，并创建 Descriptor Set Layout & Descriptor Set
	- [ ] 渲染过程中绑定 Descriptor Set。
- [ ] 补充 GLTF Render Pass。
	- [ ] Vertex Shader 中支持骨骼动画计算。
	- [ ] Fragment Shader 实现 PBR 的直接光照和间接光照。
- [ ] 新增 Tonemap Render Pass，用于线性颜色的转换。

## 说明

- 编译 shader 至 SPRIV 的命令：
	- HLSL Fragment Shader : `dxc -E main <path\to\shader> -T ps_6_0 -spirv -Fo <path\to\spirv\output>`
	- HLSL Vertex Shader : `dxc -E main <path\to\shader> -T vs_6_0 -spirv -Fo <path\to\spirv\output>`
	- GLSL shader: `glslangValidator -V <path\to\shader> -o <path\to\spirv\output>`

## Ref

- GTLF 文档: https://github.com/KhronosGroup/glTF
- 骨骼动画示例：[examples/gltfskinning/](https://github.com/SaschaWillems/Vulkan/tree/master/examples/gltfskinning)
- PBR 材质示例
	- 直接光照: [examples/pbrbasic/](https://github.com/SaschaWillems/Vulkan/tree/master/examples/pbrbasic)
	- 环境光照: [examples/pbribl/](https://github.com/SaschaWillems/Vulkan/tree/master/examples/pbribl)
- GLTF + PBR 示例：[Vulkan-glTF-PBR](https://github.com/SaschaWillems/Vulkan-glTF-PBR)
