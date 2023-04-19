# GAMES 106 - 作业1

## 作业要求

- 支持gltf的骨骼动画。
- 支持gltf的PBR的材质，包括法线贴图。
- 进阶作业：
    - 增加一个Tone Mapping的后处理 Render Pass。tonemap选择ACES实现如下:
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

## Ref

- GTLF 文档: https://github.com/KhronosGroup/glTF
- 骨骼动画示例：[examples/gltfskinning/](https://github.com/SaschaWillems/Vulkan/tree/master/examples/gltfskinning)
- PBR 材质示例
    - 直接光照: [examples/pbrbasic/](https://github.com/SaschaWillems/Vulkan/tree/master/examples/pbrbasic)
    - 环境光照: [examples/pbribl/](https://github.com/SaschaWillems/Vulkan/tree/master/examples/pbribl)
