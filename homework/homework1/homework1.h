/*
 * Copyright (C) 2020-2022 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#pragma once

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#ifdef VK_USE_PLATFORM_ANDROID_KHR
#define TINYGLTF_ANDROID_LOAD_FROM_ASSETS
#endif
#include "tiny_gltf.h"

#include "vulkanexamplebase.h"

#include <optional>

#define ENABLE_VALIDATION false

// Contains everything required to render a glTF model in Vulkan
// This class is heavily simplified (compared to glTF's feature set) but retains the basic glTF structure
class VulkanglTFModel
{
public:
	// The class requires some Vulkan objects so it can create it's own resources
	vks::VulkanDevice* vulkanDevice;
	VkQueue copyQueue;

	/*
		Base glTF structures, see gltfscene sample for details
	*/

	// The vertex layout for the samples' model
	struct Vertex
	{
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec2 uv;
		glm::vec3 color;
		glm::vec4 tangent;
	};

	// Single vertex buffer for all primitives
	struct
	{
		VkBuffer buffer;
		VkDeviceMemory memory;
	} vertices;

	// Single index buffer for all primitives
	struct
	{
		int count;
		VkBuffer buffer;
		VkDeviceMemory memory;
	} indices;

	// The following structures roughly represent the glTF scene structure
	// To keep things simple, they only contain those properties that are required for this sample

	// A primitive contains the data for a single draw call
	struct Primitive
	{
		uint32_t firstIndex;
		uint32_t indexCount;
		int32_t materialIndex;
	};

	// Contains the node's (optional) geometry and can be made up of an arbitrary number of primitives
	struct Mesh
	{
		std::vector<Primitive> primitives;
		/* HOMEWORK1 : 传递 glTF Node uniform vars */
		struct UniformBuffer {
			vks::Buffer buffer;
			VkDescriptorSet descriptorSet;
			void* mapped;
		} uniformBuffer;
	};

	// A node represents an object in the glTF scene graph
	struct Node
	{
		Node* parent;
		std::vector<Node*> children;
		Mesh mesh;
		glm::mat4 matrix;
		/* HOMEWORK1 : 载入 GLTF 载入动画数据 */
		uint32_t index;
		glm::vec3 translation{};
		glm::vec3 scale{ 1.0f };
		glm::quat rotation{};
		glm::mat4 getLocalMatrix();

		~Node() noexcept
		{
			for (auto& child : children)
			{
				delete child;
			}
		}
	};

	// A glTF material stores information in e.g. the texture that is attached to it and colors
	struct Material
	{
		// glm::vec4 baseColorFactor = glm::vec4(1.0f);
		// uint32_t baseColorTextureIndex;=

		/* HOMEWORK1 : 读取材质 */
		glm::vec4 baseColorFactor = glm::vec4(1.0f); // length 4. default [1,1,1,1]
		int32_t baseColorImageIndex = -1;
		double metallicFactor = 1.0;   // default 1
		double roughnessFactor = 1.0;  // default 1
		int32_t metallicRoughnessImageIndex = -1;
		int32_t normalImageIndex = -1;
		int32_t occlusionImageIndex = -1;
		glm::vec3 emissiveFactor = glm::vec3(0.f);  // length 3. default [0, 0, 0]
		int32_t emissiveImageIndex = -1;

		// A descriptor set which is used to access all textures of this material from the fragment shader
		VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	};

	// 用于传递 PBR 材质贴图的 Factor 属性
	struct PushConstBlock
	{
		// 部分 PBR 材质属性
		glm::vec4 baseColorFactor = glm::vec4(1.0f); // length 4. default [1,1,1,1]
		glm::vec3 emissiveFactor = glm::vec3(0.f);  // length 3. default [0, 0, 0]
		float metallicFactor = 1.f;
		float roughnessFactor = 1.f;
	};

	// Contains the texture for a single glTF image
	// Images may be reused by texture objects and are as such separated
	// TODO: remove Image
	struct Image
	{
		vks::Texture2D texture;
	};

	// A glTF texture stores a reference to the image and a sampler
	// In this sample, we are only interested in the image
	struct Texture
	{
		int32_t imageIndex;
	};

	/*
		Animation related structures
	*/

	struct AnimationSampler
	{
		std::string interpolation;
		std::vector<float> inputs;
		std::vector<glm::vec4> outputsVec4;
	};

	struct AnimationChannel
	{
		std::string path;
		Node* node;
		uint32_t samplerIndex;
	};

	struct Animation
	{
		std::string name;
		std::vector<AnimationSampler> samplers;
		std::vector<AnimationChannel> channels;
		float start = std::numeric_limits<float>::max();
		float end = std::numeric_limits<float>::min();
		float currentTime = 0.0f;
	};

	/*
		Model data
	*/
	std::vector<Image> images;
	std::vector<Texture> textures;
	std::vector<Material> materials;
	std::vector<Node*> nodes;
	/* HOMEWORK1 : 载入 GLTF 载入骨骼和动画数据 */
	std::vector<Animation> animations;
	std::vector<Node*> linearMeshNodes;

	uint32_t activeAnimation = 0;

public:
	~VulkanglTFModel();

	// glTF loading functions

	void loadImages(tinygltf::Model& input);
	void loadTextures(tinygltf::Model& input);
	void loadMaterials(tinygltf::Model& input);
	void loadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input, VulkanglTFModel::Node* parent,
	              uint32_t nodeIndex, std::vector<uint32_t>& indexBuffer, std::vector<VulkanglTFModel::Vertex>& vertexBuffer);

	/* HOMEWORK1 : 载入 GLTF 载入骨骼和动画数据 */

	Node* findNode(Node* parent, uint32_t index);
	Node* nodeFromIndex(uint32_t index);
	glm::mat4 getNodeMatrix(VulkanglTFModel::Node* node);

	void loadAnimations(tinygltf::Model& input);
	void prepareMeshUniformBuffers(vks::VulkanDevice* vkDevice);
	void updateMeshUniformBuffers();
	void updateAnimation(float deltaTime);

	void drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, VulkanglTFModel::Node* node);
	void draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout);
};


class VulkanExample final : public VulkanExampleBase
{
public:
	bool wireframe = false;

	VulkanglTFModel glTFModel;

	struct ShaderData {
		vks::Buffer buffer;
		struct Values {
			glm::mat4 projection;
			glm::mat4 view;
			glm::vec4 lightPos = glm::vec4(5.0f, 5.0f, -5.0f, 1.0f);
			glm::vec4 viewPos;
		} values;
	} shaderData;

	struct Pipelines {
		VkPipeline solid = VK_NULL_HANDLE;
		VkPipeline wireframe = VK_NULL_HANDLE;
	} pipelines;

	VkPipelineLayout pipelineLayout;
	VkDescriptorSet descriptorSet;

	struct DescriptorSetLayouts {
		// 场景相关的 Descriptor Set，包括：MV Matrix / Camera Pos / Light Pos
		VkDescriptorSetLayout matrices;
		// PBR 材质相关的 Descriptor Set
		VkDescriptorSetLayout textures;
		/* HOMEWORK1 : 传递 glTF Node uniform vars */
		VkDescriptorSetLayout nodes;
	} descriptorSetLayouts;

	// 默认的纯色 Texture
	vks::Texture2D defaultOcclusionTexture;
	vks::Texture2D defaultEmissiveTexture;

public:
	VulkanExample();
	~VulkanExample() override;

	virtual void getEnabledFeatures() override;
	virtual void buildCommandBuffers() override;
	virtual void OnUpdateUIOverlay(vks::UIOverlay* overlay) override;

	void loadglTFFile(std::string filename);
	void loadAssets();
	void prepareUniformBuffers();
	void updateUniformBuffers();
	void setupDescriptors();
	void preparePipelines();
	/**
	 * @brief Prepares all Vulkan resources and functions required to run the sample
	 */
	virtual void prepare() override;

	/**
	 * @brief (Pure virtual) Render function to be implemented by the sample application
	 */
	virtual void render() override;
	virtual void viewChanged() override;
};
