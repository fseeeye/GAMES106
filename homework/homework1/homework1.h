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
			VkBuffer buffer;
			VkDeviceMemory memory;
			VkDescriptorBufferInfo descriptor;
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

		/* HOMEWORK1 : 载入 GLTF 载入动画数据 */
		uint32_t index; 
		glm::vec3 translation{};
		glm::vec3 scale{ 1.0f };
		glm::quat rotation{};
		glm::mat4 matrix;
		glm::mat4 getLocalMatrix();

		~Node()
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
		glm::vec4 baseColorFactor = glm::vec4(1.0f);
		uint32_t baseColorTextureIndex;
	};

	// Contains the texture for a single glTF image
	// Images may be reused by texture objects and are as such separated
	struct Image
	{
		vks::Texture2D texture;
		// We also store (and create) a descriptor set that's used to access this texture from the fragment shader
		VkDescriptorSet descriptorSet;
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

	uint32_t activeAnimation = 0;

public:
	~VulkanglTFModel();

	// glTF loading functions

	void loadImages(tinygltf::Model& input);
	void loadTextures(tinygltf::Model& input);
	void loadMaterials(tinygltf::Model& input);
	void loadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input, VulkanglTFModel::Node* parent,
	              std::vector<uint32_t>& indexBuffer, std::vector<VulkanglTFModel::Vertex>& vertexBuffer);

	/* HOMEWORK1 : 载入 GLTF 载入骨骼和动画数据 */

	void loadAnimations(tinygltf::Model& input);

	Node* findNode(Node* parent, uint32_t index);
	Node* nodeFromIndex(uint32_t index);
	glm::mat4 getNodeMatrix(VulkanglTFModel::Node* node);

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
			glm::mat4 model;
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
		VkDescriptorSetLayout matrices;
		VkDescriptorSetLayout textures;
		/* HOMEWORK1 : 传递 glTF Node uniform vars */
		VkDescriptorSetLayout nodes;
	} descriptorSetLayouts;

public:
	VulkanExample();
	~VulkanExample() override;

	virtual void getEnabledFeatures() override;
	virtual void buildCommandBuffers() override;
	virtual void OnUpdateUIOverlay(vks::UIOverlay* overlay) override;

	void loadglTFFile(std::string filename);
	void loadAssets();
	void prepareUniformBuffers();
	void setupDescriptors();
	void preparePipelines();
	/**
	 * @brief Prepares all Vulkan resources and functions required to run the sample
	 */
	virtual void prepare() override;

	void updateUniformBuffers();
	/**
	 * @brief (Pure virtual) Render function to be implemented by the sample application
	 */
	virtual void render() override;
	virtual void viewChanged() override;
};
