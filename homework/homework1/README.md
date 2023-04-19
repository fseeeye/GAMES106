# GAMES 106 - ��ҵ1

## ��ҵҪ��

- ֧��gltf�Ĺ���������
- ֧��gltf��PBR�Ĳ��ʣ�����������ͼ��
- ������ҵ��
    - ����һ��Tone Mapping�ĺ��� Render Pass��tonemapѡ��ACESʵ������:
        ``` C++
        // tonemap ��ʹ�õĺ���
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
    - ����GLTF���˾����ܡ�

## Ref

- GTLF �ĵ�: https://github.com/KhronosGroup/glTF
- ��������ʾ����[examples/gltfskinning/](https://github.com/SaschaWillems/Vulkan/tree/master/examples/gltfskinning)
- PBR ����ʾ��
    - ֱ�ӹ���: [examples/pbrbasic/](https://github.com/SaschaWillems/Vulkan/tree/master/examples/pbrbasic)
    - ��������: [examples/pbribl/](https://github.com/SaschaWillems/Vulkan/tree/master/examples/pbribl)
