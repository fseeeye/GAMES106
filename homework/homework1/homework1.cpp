/*
 * Copyright (C) 2020-2022 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#include "homework1.h"

 /*

	  glTF: model class

	  Contains everything required to render a skinned glTF model in Vulkan
	  This class is simplified compared to glTF's feature set but retains the basic glTF structure required for this sample

  */

/*
   Get a node's local matrix from the current translation, rotation and scale values
   These are calculated from the current animation an need to be calculated dynamically
*/
glm::mat4 VulkanglTFModel::Node::getLocalMatrix()
{
	return glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation) * glm::scale(glm::mat4(1.0f), scale) * matrix;
}

/*
	Release all Vulkan resources acquired for the model
*/
VulkanglTFModel::~VulkanglTFModel()
{
	for (auto node : nodes) {
		delete node;
	}
	// Release all Vulkan resources allocated for the model
	vkDestroyBuffer(vulkanDevice->logicalDevice, vertices.buffer, nullptr);
	vkFreeMemory(vulkanDevice->logicalDevice, vertices.memory, nullptr);
	vkDestroyBuffer(vulkanDevice->logicalDevice, indices.buffer, nullptr);
	vkFreeMemory(vulkanDevice->logicalDevice, indices.memory, nullptr);
	for (Image image : images) {
		vkDestroyImageView(vulkanDevice->logicalDevice, image.texture.view, nullptr);
		vkDestroyImage(vulkanDevice->logicalDevice, image.texture.image, nullptr);
		vkDestroySampler(vulkanDevice->logicalDevice, image.texture.sampler, nullptr);
		vkFreeMemory(vulkanDevice->logicalDevice, image.texture.deviceMemory, nullptr);
	}

	// TODO: destroy
}

/*
	glTF: loading functions

	The following functions take a glTF input model loaded via tinyglTF and convert all required data into our own structure
*/

void VulkanglTFModel::loadImages(tinygltf::Model& input)
{
	// Images can be stored inside the glTF (which is the case for the sample model), so instead of directly
	// loading them from disk, we fetch them from the glTF loader and upload the buffers
	images.resize(input.images.size());
	for (size_t i = 0; i < input.images.size(); i++) {
		tinygltf::Image& glTFImage = input.images[i];
		// Get the image data from the glTF loader
		unsigned char* buffer = nullptr;
		VkDeviceSize bufferSize = 0;
		bool deleteBuffer = false;
		// We convert RGB-only images to RGBA, as most devices don't support RGB-formats in Vulkan
		if (glTFImage.component == 3) {
			bufferSize = glTFImage.width * glTFImage.height * 4;
			buffer = new unsigned char[bufferSize];
			unsigned char* rgba = buffer;
			unsigned char* rgb = &glTFImage.image[0];
			for (size_t i = 0; i < glTFImage.width * glTFImage.height; ++i) {
				memcpy(rgba, rgb, sizeof(unsigned char) * 3);
				rgba += 4;
				rgb += 3;
			}
			deleteBuffer = true;
		}
		else {
			buffer = &glTFImage.image[0];
			bufferSize = glTFImage.image.size();
		}
		// Load texture from image buffer
		images[i].texture.fromBuffer(buffer, bufferSize, VK_FORMAT_R8G8B8A8_UNORM, glTFImage.width, glTFImage.height, vulkanDevice, copyQueue);
		if (deleteBuffer) {
			delete[] buffer;
		}
	}
}

void VulkanglTFModel::loadTextures(tinygltf::Model& input)
{
	textures.resize(input.textures.size());
	for (size_t i = 0; i < input.textures.size(); i++) {
		textures[i].imageIndex = input.textures[i].source;
	}
}

void VulkanglTFModel::loadMaterials(tinygltf::Model& input)
{
	materials.resize(input.materials.size());
	for (size_t i = 0; i < input.materials.size(); i++) {
		// We only read the most basic properties required for our sample
		tinygltf::Material glTFMaterial = input.materials[i];
		// // Get the base color factor
		// if (glTFMaterial.values.find("baseColorFactor") != glTFMaterial.values.end()) {
		// 	materials[i].baseColorFactor = glm::make_vec4(glTFMaterial.values["baseColorFactor"].ColorFactor().data());
		// }
		// // Get base color texture index
		// if (glTFMaterial.values.find("baseColorTexture") != glTFMaterial.values.end()) {
		// 	materials[i].baseColorTextureIndex = glTFMaterial.values["baseColorTexture"].TextureIndex();
		// }

		/* HOMEWORK1 : 读取材质 */
		materials[i].baseColorFactor = {
			glTFMaterial.pbrMetallicRoughness.baseColorFactor[0],
			glTFMaterial.pbrMetallicRoughness.baseColorFactor[1],
			glTFMaterial.pbrMetallicRoughness.baseColorFactor[2],
			glTFMaterial.pbrMetallicRoughness.baseColorFactor[3],
		};
		materials[i].baseColorImageIndex = textures[glTFMaterial.pbrMetallicRoughness.baseColorTexture.index].imageIndex;
		materials[i].metallicFactor = glTFMaterial.pbrMetallicRoughness.metallicFactor;
		materials[i].roughnessFactor = glTFMaterial.pbrMetallicRoughness.roughnessFactor;
		materials[i].metallicRoughnessImageIndex = textures[glTFMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index].imageIndex;
		materials[i].normalImageIndex = textures[glTFMaterial.normalTexture.index].imageIndex;
		if (glTFMaterial.occlusionTexture.index > -1)
		{
			materials[i].occlusionImageIndex = textures[glTFMaterial.occlusionTexture.index].imageIndex;
		}
		materials[i].emissiveFactor = {
			glTFMaterial.emissiveFactor[0],
			glTFMaterial.emissiveFactor[1],
			glTFMaterial.emissiveFactor[2]
		};
		if (glTFMaterial.emissiveTexture.index > -1)
		{
			materials[i].emissiveImageIndex = textures[glTFMaterial.emissiveTexture.index].imageIndex;
		}
	}
}

void VulkanglTFModel::loadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input,
                               VulkanglTFModel::Node* parent, uint32_t nodeIndex,
                               std::vector<uint32_t>& indexBuffer, std::vector<VulkanglTFModel::Vertex>& vertexBuffer)
{
	VulkanglTFModel::Node* node = new VulkanglTFModel::Node{};
	node->matrix = glm::mat4(1.0f);
	node->parent = parent;
	/* HOMEWORK1 : 载入 GLTF 载入动画数据 */
	node->index = nodeIndex;

	// Get the local node matrix
	// It's either made up from translation, rotation, scale or a 4x4 matrix
	if (inputNode.translation.size() == 3) {
		//node->matrix = glm::translate(node->matrix, glm::vec3(glm::make_vec3(inputNode.translation.data())));
		node->translation = glm::make_vec3(inputNode.translation.data());
	}
	if (inputNode.rotation.size() == 4) {
		//glm::quat q = glm::make_quat(inputNode.rotation.data());
		//node->matrix *= glm::mat4(q);
		node->rotation = glm::make_quat(inputNode.rotation.data());
	}
	if (inputNode.scale.size() == 3) {
		//node->matrix = glm::scale(node->matrix, glm::vec3(glm::make_vec3(inputNode.scale.data())));
		node->scale = glm::make_vec3(inputNode.scale.data());
	}
	if (inputNode.matrix.size() == 16) {
		//node->matrix = glm::make_mat4x4(inputNode.matrix.data());
		node->matrix = glm::make_mat4x4(inputNode.matrix.data());
	};

	// Load node's children
	if (inputNode.children.size() > 0) {
		for (size_t i = 0; i < inputNode.children.size(); i++) {
			loadNode(input.nodes[inputNode.children[i]], input, node, inputNode.children[i], indexBuffer, vertexBuffer);
		}
	}

	// 读取 Vertex Buffer & Index Buffer 信息
	// If the node contains mesh data, we load vertices and indices from the buffers
	// In glTF this is done via accessors and buffer views
	if (inputNode.mesh > -1) {
		const tinygltf::Mesh mesh = input.meshes[inputNode.mesh];
		// Iterate through all primitives of this node's mesh
		for (size_t i = 0; i < mesh.primitives.size(); i++) {
			const tinygltf::Primitive& glTFPrimitive = mesh.primitives[i];
			uint32_t firstIndex = static_cast<uint32_t>(indexBuffer.size());
			uint32_t vertexStart = static_cast<uint32_t>(vertexBuffer.size());
			uint32_t indexCount = 0;
			// Vertices
			{
				const float* positionBuffer = nullptr;
				const float* normalsBuffer = nullptr;
				const float* texCoordsBuffer = nullptr;
				const float* tangentBuffer = nullptr;
				size_t vertexCount = 0;

				// Get buffer data for vertex positions
				if (glTFPrimitive.attributes.find("POSITION") != glTFPrimitive.attributes.end()) {
					const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("POSITION")->second];
					const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
					positionBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
					vertexCount = accessor.count;
				}
				// Get buffer data for vertex normals
				if (glTFPrimitive.attributes.find("NORMAL") != glTFPrimitive.attributes.end()) {
					const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("NORMAL")->second];
					const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
					normalsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}
				// Get buffer data for vertex texture coordinates
				// glTF supports multiple sets, we only load the first one
				if (glTFPrimitive.attributes.find("TEXCOORD_0") != glTFPrimitive.attributes.end()) {
					const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("TEXCOORD_0")->second];
					const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
					texCoordsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}
				/* HOMEWORK1 : 读取切线信息 */
				if (glTFPrimitive.attributes.find("TANGENT") != glTFPrimitive.attributes.end())
				{
					const tinygltf::Accessor& tangentAccessor = input.accessors[glTFPrimitive.attributes.find("TANGENT")->second];
					const tinygltf::BufferView& tangentView = input.bufferViews[tangentAccessor.bufferView];
					tangentBuffer = reinterpret_cast<const float*>(&(input.buffers[tangentView.buffer].data[tangentAccessor.byteOffset + tangentView.byteOffset]));
				}

				// Append data to model's vertex buffer
				for (size_t v = 0; v < vertexCount; v++) {
					Vertex vert{};
					vert.pos = glm::vec4(glm::make_vec3(&positionBuffer[v * 3]), 1.0f);
					vert.normal = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
					vert.uv = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec3(0.0f);
					vert.color = glm::vec3(1.0f);
					vert.tangent = tangentBuffer ? glm::make_vec4(&tangentBuffer[v * 4]) : glm::vec4(0xff);
					vertexBuffer.push_back(vert);
				}
			}
			// Indices
			{
				const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.indices];
				const tinygltf::BufferView& bufferView = input.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = input.buffers[bufferView.buffer];

				indexCount += static_cast<uint32_t>(accessor.count);

				// glTF supports different component types of indices
				switch (accessor.componentType) {
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
					const uint32_t* buf = reinterpret_cast<const uint32_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					for (size_t index = 0; index < accessor.count; index++) {
						indexBuffer.push_back(buf[index] + vertexStart);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
					const uint16_t* buf = reinterpret_cast<const uint16_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					for (size_t index = 0; index < accessor.count; index++) {
						indexBuffer.push_back(buf[index] + vertexStart);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
					const uint8_t* buf = reinterpret_cast<const uint8_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					for (size_t index = 0; index < accessor.count; index++) {
						indexBuffer.push_back(buf[index] + vertexStart);
					}
					break;
				}
				default:
					std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
					return;
				}
			}
			Primitive primitive{};
			primitive.firstIndex = firstIndex;
			primitive.indexCount = indexCount;
			primitive.materialIndex = glTFPrimitive.material;
			node->mesh.primitives.push_back(primitive);
		}
	}

	if (parent) {
		parent->children.push_back(node);
	}
	else {
		nodes.push_back(node);
	}

	/* HOMEWORK1 : 载入 GLTF 载入动画数据 */
	if (!node->mesh.primitives.empty())
	{
		linearMeshNodes.push_back(node);
	}
}

/*
	glTF: Helper functions for locating glTF nodes
*/

VulkanglTFModel::Node* VulkanglTFModel::findNode(Node* parent, uint32_t index)
{
	Node* nodeFound = nullptr;
	if (parent->index == index)
	{
		return parent;
	}
	for (auto& child : parent->children)
	{
		nodeFound = findNode(child, index);
		if (nodeFound)
		{
			break;
		}
	}
	return nodeFound;
}

VulkanglTFModel::Node* VulkanglTFModel::nodeFromIndex(uint32_t index)
{
	Node* nodeFound = nullptr;
	for (auto& node : nodes)
	{
		nodeFound = findNode(node, index);
		if (nodeFound)
		{
			break;
		}
	}
	return nodeFound;
}

// Traverse the node hierarchy to the top-most parent to get the local matrix of the given node
glm::mat4 VulkanglTFModel::getNodeMatrix(VulkanglTFModel::Node* node)
{
	glm::mat4 nodeMatrix = node->getLocalMatrix();
	Node* currentParent = node->parent;
	while (currentParent)
	{
		nodeMatrix = currentParent->getLocalMatrix() * nodeMatrix;
		currentParent = currentParent->parent;
	}
	return nodeMatrix;
}

/*
	glTF: animation functions
*/

void VulkanglTFModel::loadAnimations(tinygltf::Model& input)
{
	animations.resize(input.animations.size());

	for (size_t i = 0; i < input.animations.size(); i++)
	{
		tinygltf::Animation glTFAnimation = input.animations[i];
		animations[i].name = glTFAnimation.name;

		// Samplers
		animations[i].samplers.resize(glTFAnimation.samplers.size());
		for (size_t j = 0; j < glTFAnimation.samplers.size(); j++)
		{
			tinygltf::AnimationSampler glTFSampler = glTFAnimation.samplers[j];
			AnimationSampler& dstSampler = animations[i].samplers[j];
			dstSampler.interpolation = glTFSampler.interpolation;

			// Read sampler keyframe input time values
			{
				const tinygltf::Accessor& accessor = input.accessors[glTFSampler.input];
				const tinygltf::BufferView& bufferView = input.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = input.buffers[bufferView.buffer];
				const void* dataPtr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
				const float* buf = static_cast<const float*>(dataPtr);
				for (size_t index = 0; index < accessor.count; index++)
				{
					dstSampler.inputs.push_back(buf[index]);
				}
				// Adjust animation's start and end times
				for (auto input : animations[i].samplers[j].inputs)
				{
					if (input < animations[i].start)
					{
						animations[i].start = input;
					};
					if (input > animations[i].end)
					{
						animations[i].end = input;
					}
				}
			}

			// Read sampler keyframe output translate/rotate/scale values
			{
				const tinygltf::Accessor& accessor = input.accessors[glTFSampler.output];
				const tinygltf::BufferView& bufferView = input.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = input.buffers[bufferView.buffer];
				const void* dataPtr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
				switch (accessor.type)
				{
				case TINYGLTF_TYPE_VEC3: {
					const glm::vec3* buf = static_cast<const glm::vec3*>(dataPtr);
					for (size_t index = 0; index < accessor.count; index++)
					{
						dstSampler.outputsVec4.push_back(glm::vec4(buf[index], 0.0f));
					}
					break;
				}
				case TINYGLTF_TYPE_VEC4: {
					const glm::vec4* buf = static_cast<const glm::vec4*>(dataPtr);
					for (size_t index = 0; index < accessor.count; index++)
					{
						dstSampler.outputsVec4.push_back(buf[index]);
					}
					break;
				}
				default: {
					std::cout << "unknown type" << std::endl;
					break;
				}
				}
			}
		}

		// Channels
		animations[i].channels.resize(glTFAnimation.channels.size());
		for (size_t j = 0; j < glTFAnimation.channels.size(); j++)
		{
			tinygltf::AnimationChannel glTFChannel = glTFAnimation.channels[j];
			AnimationChannel& dstChannel = animations[i].channels[j];
			dstChannel.path = glTFChannel.target_path;
			dstChannel.samplerIndex = glTFChannel.sampler;
			dstChannel.node = nodeFromIndex(glTFChannel.target_node);
		}
	}
}

void VulkanglTFModel::prepareMeshUniformBuffers(vks::VulkanDevice* vkDevice)
{
	/* HOMEWORK1 : 传递 glTF Node uniform */
	for (auto& meshNode : linearMeshNodes)
	{
		if (!meshNode->mesh.primitives.empty())
		{
			// Vertex shader uniform buffer block for Descriptor set 2
			VK_CHECK_RESULT(vkDevice->createBuffer(
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&meshNode->mesh.uniformBuffer.buffer,
				sizeof(glm::mat4)));

			// Map persistent
			VK_CHECK_RESULT(meshNode->mesh.uniformBuffer.buffer.map());
		}
	}
}

void VulkanglTFModel::updateMeshUniformBuffers()
{
	/* HOMEWORK1 : 传递 glTF Node uniform */
	for (auto& meshNode : linearMeshNodes)
	{
		if (!meshNode->mesh.primitives.empty() && meshNode->mesh.uniformBuffer.buffer.mapped)
		{
			glm::mat4 nodeMatrix = getNodeMatrix(meshNode);
			memcpy(meshNode->mesh.uniformBuffer.buffer.mapped, static_cast<const void*>(&nodeMatrix), sizeof(glm::mat4));
		}
	}
}

// POI: Update the current animation
void VulkanglTFModel::updateAnimation(float deltaTime)
{
	if (activeAnimation > static_cast<uint32_t>(animations.size()) - 1)
	{
		std::cout << "No animation with index " << activeAnimation << std::endl;
		return;
	}
	Animation& animation = animations[activeAnimation];
	animation.currentTime += deltaTime;
	if (animation.currentTime > animation.end)
	{
		animation.currentTime -= animation.end;
	}

	for (auto& channel : animation.channels)
	{
		AnimationSampler& sampler = animation.samplers[channel.samplerIndex];
		for (size_t i = 0; i < sampler.inputs.size() - 1; i++)
		{
			if (sampler.interpolation != "LINEAR")
			{
				std::cout << "This sample only supports linear interpolations\n";
				continue;
			}

			// Get the input keyframe values for the current time stamp
			if ((animation.currentTime >= sampler.inputs[i]) && (animation.currentTime <= sampler.inputs[i + 1]))
			{
				float a = (animation.currentTime - sampler.inputs[i]) / (sampler.inputs[i + 1] - sampler.inputs[i]);
				if (channel.path == "translation")
				{
					channel.node->translation = glm::mix(sampler.outputsVec4[i], sampler.outputsVec4[i + 1], a);
				}
				else if (channel.path == "rotation")
				{
					glm::quat q1;
					q1.x = sampler.outputsVec4[i].x;
					q1.y = sampler.outputsVec4[i].y;
					q1.z = sampler.outputsVec4[i].z;
					q1.w = sampler.outputsVec4[i].w;

					glm::quat q2;
					q2.x = sampler.outputsVec4[i + 1].x;
					q2.y = sampler.outputsVec4[i + 1].y;
					q2.z = sampler.outputsVec4[i + 1].z;
					q2.w = sampler.outputsVec4[i + 1].w;

					channel.node->rotation = glm::normalize(glm::slerp(q1, q2, a));
				}
				else if (channel.path == "scale")
				{
					channel.node->scale = glm::mix(sampler.outputsVec4[i], sampler.outputsVec4[i + 1], a);
				}
			}
		}
	}

	updateMeshUniformBuffers();
}

/*
	glTF: rendering functions
*/

// Draw a single node including child nodes (if present)
void VulkanglTFModel::drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout,
	VulkanglTFModel::Node* node)
{
	/* HOMEWORK1 : 传递 glTF Node uniform */
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 2, 1, &node->mesh.uniformBuffer.descriptorSet, 0, nullptr);

	if (!node->mesh.primitives.empty()) {
		//// Pass the node's matrix via push constants
		//// Traverse the node hierarchy to the top-most parent to get the final matrix of the current node
		//glm::mat4 nodeMatrix = getNodeMatrix(node);

		for (VulkanglTFModel::Primitive& primitive : node->mesh.primitives) {
			if (primitive.indexCount > 0) {
				// Pass PBR material attributes to the vertex shader using push constants
				PushConstBlock pushConstBlock = {};
				pushConstBlock.baseColorFactor = materials[primitive.materialIndex].baseColorFactor;
				pushConstBlock.emissiveFactor = materials[primitive.materialIndex].emissiveFactor;
				pushConstBlock.metallicFactor = static_cast<float>(materials[primitive.materialIndex].metallicFactor);
				pushConstBlock.roughnessFactor = static_cast<float>(materials[primitive.materialIndex].roughnessFactor);
				vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstBlock),
						&pushConstBlock);

				// Bind the descriptor for the current primitive's texture
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &materials[primitive.materialIndex].descriptorSet, 0, nullptr);
				vkCmdDrawIndexed(commandBuffer, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
			}
		}
	}
}


void VulkanglTFModel::draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout)
{
	// All vertices and indices are stored in single buffers, so we only need to bind once
	VkDeviceSize offsets[1] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertices.buffer, offsets);
	vkCmdBindIndexBuffer(commandBuffer, indices.buffer, 0, VK_INDEX_TYPE_UINT32);
	// Render all nodes at top-level
	for (auto& node : linearMeshNodes) {
		drawNode(commandBuffer, pipelineLayout, node);
	}
}

/*
 * Vulkan Example Implements
 */

VulkanExample::VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
{
	title = "homework1";
	camera.type = Camera::CameraType::lookat;
	camera.flipY = true;
	camera.setPosition(glm::vec3(0.0f, -0.1f, -1.0f));
	camera.setRotation(glm::vec3(0.0f, 45.0f, 0.0f));
	camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);
}

VulkanExample::~VulkanExample()
{
	// Clean up used Vulkan resources
		// Note : Inherited destructor cleans up resources stored in base class
	vkDestroyPipeline(device, pipelines.solid, nullptr);
	if (pipelines.wireframe != VK_NULL_HANDLE) {
		vkDestroyPipeline(device, pipelines.wireframe, nullptr);
	}

	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.matrices, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.textures, nullptr);
	// TODO: destroy descriptorset layout

	shaderData.buffer.destroy();
}

void VulkanExample::getEnabledFeatures()
{
	// Fill mode non solid is required for wireframe display
	if (deviceFeatures.fillModeNonSolid) {
		enabledFeatures.fillModeNonSolid = VK_TRUE;
	};
}

void VulkanExample::buildCommandBuffers()
{
	VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

	VkClearValue clearValues[2];
	clearValues[0].color = defaultClearColor;
	clearValues[0].color = { { 0.25f, 0.25f, 0.25f, 1.0f } };;
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = width;
	renderPassBeginInfo.renderArea.extent.height = height;
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;

	const VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
	const VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);

	for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
	{
		renderPassBeginInfo.framebuffer = frameBuffers[i];
		VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

		vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
		vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);
		// Bind scene matrices descriptor to set 0
		vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
		vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, wireframe ? pipelines.wireframe : pipelines.solid);

		// 绘制各个 glTF Node
		glTFModel.draw(drawCmdBuffers[i], pipelineLayout);

		// 绘制 UI
		drawUI(drawCmdBuffers[i]);

		vkCmdEndRenderPass(drawCmdBuffers[i]);
		VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
	}
}

void VulkanExample::OnUpdateUIOverlay(vks::UIOverlay* overlay)
{
	if (overlay->header("Settings")) {
		if (overlay->checkBox("Wireframe", &wireframe)) {
			buildCommandBuffers();
		}
	}
}

void VulkanExample::loadglTFFile(std::string filename)
{
	tinygltf::Model glTFInput;
	tinygltf::TinyGLTF gltfContext;
	std::string error, warning;

	this->device = device;

#if defined(__ANDROID__)
	// On Android all assets are packed with the apk in a compressed form, so we need to open them using the asset manager
	// We let tinygltf handle this, by passing the asset manager of our app
	tinygltf::asset_manager = androidApp->activity->assetManager;
#endif
	bool fileLoaded = gltfContext.LoadASCIIFromFile(&glTFInput, &error, &warning, filename);

	// Pass some Vulkan resources required for setup and rendering to the glTF model loading class
	glTFModel.vulkanDevice = vulkanDevice;
	glTFModel.copyQueue = queue;

	std::vector<uint32_t> indexBuffer;
	std::vector<VulkanglTFModel::Vertex> vertexBuffer;

	if (fileLoaded) {
		glTFModel.loadImages(glTFInput);
		glTFModel.loadTextures(glTFInput);
		glTFModel.loadMaterials(glTFInput);
		const tinygltf::Scene& scene = glTFInput.scenes[0];
		for (size_t i = 0; i < scene.nodes.size(); i++) {
			const tinygltf::Node node = glTFInput.nodes[scene.nodes[i]];
			glTFModel.loadNode(node, glTFInput, nullptr, scene.nodes[i], indexBuffer, vertexBuffer);
		}
		/* HOMEWORK1 : 载入 GLTF 载入动画数据 */
		glTFModel.loadAnimations(glTFInput);
	}
	else {
		vks::tools::exitFatal("Could not open the glTF file.\n\nThe file is part of the additional asset pack.\n\nRun \"download_assets.py\" in the repository root to download the latest version.", -1);
		return;
	}

	// Create and upload vertex and index buffer
	// We will be using one single vertex buffer and one single index buffer for the whole glTF scene
	// Primitives (of the glTF model) will then index into these using index offsets

	size_t vertexBufferSize = vertexBuffer.size() * sizeof(VulkanglTFModel::Vertex);
	size_t indexBufferSize = indexBuffer.size() * sizeof(uint32_t);
	glTFModel.indices.count = static_cast<uint32_t>(indexBuffer.size());

	struct StagingBuffer {
		VkBuffer buffer;
		VkDeviceMemory memory;
	} vertexStaging, indexStaging;

	// Create host visible staging buffers (source)
	VK_CHECK_RESULT(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		vertexBufferSize,
		&vertexStaging.buffer,
		&vertexStaging.memory,
		vertexBuffer.data()));
	// Index data
	VK_CHECK_RESULT(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		indexBufferSize,
		&indexStaging.buffer,
		&indexStaging.memory,
		indexBuffer.data()));

	// Create device local buffers (target)
	VK_CHECK_RESULT(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		vertexBufferSize,
		&glTFModel.vertices.buffer,
		&glTFModel.vertices.memory));
	VK_CHECK_RESULT(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		indexBufferSize,
		&glTFModel.indices.buffer,
		&glTFModel.indices.memory));

	// Copy data from staging buffers (host) do device local buffer (gpu)
	VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	VkBufferCopy copyRegion = {};

	copyRegion.size = vertexBufferSize;
	vkCmdCopyBuffer(
		copyCmd,
		vertexStaging.buffer,
		glTFModel.vertices.buffer,
		1,
		&copyRegion);

	copyRegion.size = indexBufferSize;
	vkCmdCopyBuffer(
		copyCmd,
		indexStaging.buffer,
		glTFModel.indices.buffer,
		1,
		&copyRegion);

	vulkanDevice->flushCommandBuffer(copyCmd, queue, true);

	// Free staging resources
	vkDestroyBuffer(device, vertexStaging.buffer, nullptr);
	vkFreeMemory(device, vertexStaging.memory, nullptr);
	vkDestroyBuffer(device, indexStaging.buffer, nullptr);
	vkFreeMemory(device, indexStaging.memory, nullptr);
}

void VulkanExample::loadAssets()
{
	loadglTFFile(getAssetPath() + "buster_drone/busterDrone.gltf");

	// Create default texture assets
	constexpr unsigned long long width = 2048;
	constexpr unsigned long long height = 2048;

	std::vector<unsigned char> buffer(width * height * 4, MAXBYTE);
	const auto bufferSize = static_cast<VkDeviceSize>(buffer.size());
	defaultOcclusionTexture.fromBuffer(buffer.data(), bufferSize, VK_FORMAT_R8G8B8A8_UNORM, width, height, vulkanDevice, queue);

	buffer.resize(width * height * 4, 0);
	defaultEmissiveTexture.fromBuffer(buffer.data(), bufferSize, VK_FORMAT_R8G8B8A8_UNORM, width, height, vulkanDevice, queue);
}

void VulkanExample::setupDescriptors()
{
	/*
		This sample uses separate descriptor sets (and layouts) for the matrices and materials (textures)
	*/

	/* HOMEWORK1 : 传递 glTF Node uniform */
	std::vector<VkDescriptorPoolSize> poolSizes = {
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 + static_cast<uint32_t>(glTFModel.linearMeshNodes.size())),
		// One combined image sampler per model image/texture
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(glTFModel.materials.size() * 5)),
	};
	// One set for matrices and one per model image/texture
	const uint32_t maxSetCount = static_cast<uint32_t>(glTFModel.images.size()) + static_cast<uint32_t>(glTFModel.linearMeshNodes.size()) + 1;
	VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, maxSetCount);
	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
	
	{
		// Descriptor set layout 1 : passing scene matrices
		VkDescriptorSetLayoutBinding setLayoutBinding = vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0);
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(&setLayoutBinding, 1);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.matrices));

		// Setup Descriptor Set : scene matrices
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.matrices, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));
		VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &shaderData.buffer.descriptor);
		vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
	}

	{
		// Descriptor set layout 1 : passing PBR material textures
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {};
		setLayoutBindings.emplace_back(vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0));
		setLayoutBindings.emplace_back(vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1));
		setLayoutBindings.emplace_back(vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2));
		setLayoutBindings.emplace_back(vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3));
		setLayoutBindings.emplace_back(vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4));
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.textures));

		// Descriptor Sets : material textures
		for (auto& material : glTFModel.materials) {
			// TODO: create descriptor set for textures of PBR material and set descriptors to it.
			/* HOMEWORK1 : 传递 PBR 材质贴图 */
			// Allocate Descriptor Set
			const VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.textures, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &material.descriptorSet));

			// Prepare descriptors
			if (material.baseColorImageIndex <=  -1)
			{
				throw std::runtime_error("Cannot load image of Base Color Texture.");
			}
			if (material.metallicRoughnessImageIndex <= -1)
			{
				throw std::runtime_error("Cannot load image of Metallic Roughness Texture.");
			}
			if (material.normalImageIndex <= -1)
			{
				throw std::runtime_error("Cannot load image of Normal Texture.");
			}
			std::array<VkDescriptorImageInfo*, 5> textureDescriptors = {
				&glTFModel.images[material.baseColorImageIndex].texture.descriptor,
				&glTFModel.images[material.metallicRoughnessImageIndex].texture.descriptor,
				&glTFModel.images[material.normalImageIndex].texture.descriptor,
				material.occlusionImageIndex > -1 ? &glTFModel.images[material.occlusionImageIndex].texture.descriptor : &defaultOcclusionTexture.descriptor,
				material.emissiveImageIndex > -1 ? &glTFModel.images[material.emissiveImageIndex].texture.descriptor : &defaultEmissiveTexture.descriptor,
			};

			// Create VkWriteDescriptorSets
			std::array<VkWriteDescriptorSet, 5> writeDescriptorSets = {};
			for (size_t i = 0; i < textureDescriptors.size(); i++)
			{
				writeDescriptorSets[i] = vks::initializers::writeDescriptorSet(material.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, i, textureDescriptors[i]);
			}

			// Update all descriptors of this set
			vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
		}
	}

	/* HOMEWORK1 : 传递 glTF Node uniform */
	{
		// Descriptor set layout 2 : passing mesh data of glTF Node 
		VkDescriptorSetLayoutBinding setLayoutBinding = vks::initializers::descriptorSetLayoutBinding(
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(&setLayoutBinding, 1);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.nodes))

		// Descriptor Sets : mesh data of glTF Node 
		for (auto& meshNode : glTFModel.linearMeshNodes)
		{
			if (!meshNode->mesh.primitives.empty())
			{
				const VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.nodes, 1);
				VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &meshNode->mesh.uniformBuffer.descriptorSet));
				//VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &node->mesh.uniformBuffer.descriptorSet));
				VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(
					meshNode->mesh.uniformBuffer.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0,
					&meshNode->mesh.uniformBuffer.buffer.descriptor);
				vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
			}
		}
	}

	// Pipeline Layout using both descriptor sets (set 0 = matrices, set 1 = material, set2 = node)
	std::array<VkDescriptorSetLayout, 3> setLayouts = { descriptorSetLayouts.matrices, descriptorSetLayouts.textures, descriptorSetLayouts.nodes };
	VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(
		setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));

	//// Push constants to push the local matrices of a primitive to the vertex shader
	VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(VulkanglTFModel::PushConstBlock), 0);
	//// Push constant ranges are part of the pipeline layout
	pipelineLayoutCI.pushConstantRangeCount = 1;
	pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;

	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout))
}

/**
 * @brief Prepare and initialize uniform buffer containing shader uniforms
 */
void VulkanExample::prepareUniformBuffers()
{
	// Vertex shader uniform buffer block for Descriptor set 0
	VK_CHECK_RESULT(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&shaderData.buffer,
		sizeof(shaderData.values)));

	// Map persistent
	VK_CHECK_RESULT(shaderData.buffer.map());

	/* HOMEWORK1 : 传递 glTF Node uniform */
	glTFModel.prepareMeshUniformBuffers(vulkanDevice);

	updateUniformBuffers();
}

void VulkanExample::updateUniformBuffers()
{
	shaderData.values.projection = camera.matrices.perspective;
	shaderData.values.view = camera.matrices.view;
	shaderData.values.viewPos = camera.viewPos;
	memcpy(shaderData.buffer.mapped, &shaderData.values, sizeof(shaderData.values));

	/* HOMEWORK1 : 传递 glTF Node uniform */
	glTFModel.updateMeshUniformBuffers();
}

void VulkanExample::preparePipelines()
{
	// 配置各类 States
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
	VkPipelineRasterizationStateCreateInfo rasterizationStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
	VkPipelineColorBlendAttachmentState blendAttachmentStateCI = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
	VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentStateCI);
	VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
	VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
	VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
	const std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);

	// 配置 Vertex Input State
	const std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
		vks::initializers::vertexInputBindingDescription(0, sizeof(VulkanglTFModel::Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
	};
	const std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
		vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VulkanglTFModel::Vertex, pos)),        // Location 0: Position
		vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VulkanglTFModel::Vertex, normal)),     // Location 1: Normal
		vks::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VulkanglTFModel::Vertex, uv)),         // Location 2: Texture coordinates
		vks::initializers::vertexInputAttributeDescription(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VulkanglTFModel::Vertex, color)),      // Location 3: Color
		vks::initializers::vertexInputAttributeDescription(0, 4, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VulkanglTFModel::Vertex, tangent)), // Location 4: Tangent
	};
	VkPipelineVertexInputStateCreateInfo vertexInputStateCI = vks::initializers::pipelineVertexInputStateCreateInfo();
	vertexInputStateCI.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
	vertexInputStateCI.pVertexBindingDescriptions = vertexInputBindings.data();
	vertexInputStateCI.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
	vertexInputStateCI.pVertexAttributeDescriptions = vertexInputAttributes.data();

	// 加载 Shaders
	const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
		loadShader(getHomeworkShadersPath() + "homework1/mesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
		loadShader(getHomeworkShadersPath() + "homework1/mesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
	};

	// 准备 Pipeline Create Info
	VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass, 0); // 指定 Pipeline Layout 和 Render Pass
	pipelineCI.pVertexInputState = &vertexInputStateCI;
	pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
	pipelineCI.pRasterizationState = &rasterizationStateCI;
	pipelineCI.pColorBlendState = &colorBlendStateCI;
	pipelineCI.pMultisampleState = &multisampleStateCI;
	pipelineCI.pViewportState = &viewportStateCI;
	pipelineCI.pDepthStencilState = &depthStencilStateCI;
	pipelineCI.pDynamicState = &dynamicStateCI;
	pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCI.pStages = shaderStages.data();

	// 创建 Solid rendering pipeline
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.solid));

	// 创建 Wire frame rendering pipeline
	if (deviceFeatures.fillModeNonSolid) {
		rasterizationStateCI.polygonMode = VK_POLYGON_MODE_LINE;
		rasterizationStateCI.lineWidth = 1.0f;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.wireframe));
	}
}

void VulkanExample::prepare()
{
	VulkanExampleBase::prepare();
	loadAssets();
	prepareUniformBuffers();
	setupDescriptors();
	preparePipelines();
	buildCommandBuffers();
	prepared = true;
}

void VulkanExample::render()
{
	renderFrame();
	if (camera.updated) {
		updateUniformBuffers();
	}
	/* HOMEWORK1 :更新动画数据 */
	// Advance animation
	if (!paused)
	{
		glTFModel.updateAnimation(frameTimer);
	}
}

void VulkanExample::viewChanged()
{
	updateUniformBuffers();
}

VULKAN_EXAMPLE_MAIN()
