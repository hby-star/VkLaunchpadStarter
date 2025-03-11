#ifndef LOADMODEL_H
#define LOADMODEL_H

#include <VulkanLaunchpad.h>
#include <vulkan/vulkan.hpp> 
#include <string>
#include <vector>
#include <glm/glm.hpp> 

struct DrawModelData
{
	VklGeometryData mGeometryData;
	uint32_t mNumModelIndices = 0;
	VkBuffer mModelVertexs = VK_NULL_HANDLE;
	VkDeviceMemory mModelVertexsMemory = VK_NULL_HANDLE;
	VkBuffer mModelIndices = VK_NULL_HANDLE;
	VkDeviceMemory mModelIndicesMemory = VK_NULL_HANDLE;
};

struct Vertex
{
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 texCoord;

	static VkVertexInputBindingDescription getBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription;

		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions()
	{
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions(3);

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, normal);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

		return attributeDescriptions;
	}
};

class Model
{
private:
	std::vector<DrawModelData> mDrawModelDatas;

public:
	Model() = default;
	~Model() = default;

	void createGeometryAndBuffers(const std::string& path_to_obj, glm::vec3 trans)
	{
		DrawModelData newDrawModelData;
		newDrawModelData.mGeometryData = vklLoadModelGeometry(path_to_obj);

		if (newDrawModelData.mGeometryData.normals.size() != newDrawModelData.mGeometryData.positions.size())
		{
			VKL_EXIT_WITH_ERROR("The number of normals does not match the number of positions.");
		}
		std::vector<Vertex> vertexs(newDrawModelData.mGeometryData.positions.size());
		for (size_t i = 0; i < newDrawModelData.mGeometryData.positions.size(); i++)
		{
			vertexs[i].pos = newDrawModelData.mGeometryData.positions[i] + trans;
			vertexs[i].normal = newDrawModelData.mGeometryData.normals[i];
			vertexs[i].texCoord = newDrawModelData.mGeometryData.textureCoordinates[i];
		}

		std::vector<uint32_t> indices = newDrawModelData.mGeometryData.indices;

		newDrawModelData.mNumModelIndices = static_cast<uint32_t>(indices.size());
		const auto device = vklGetDevice();
		auto dispatchLoader = vk::DispatchLoaderStatic();

		// 1. VERTEXS BUFFER (Buffer, Memory, Bind 'em together, copy data into it)
		{
			VkBufferCreateInfo vertexBufferCreateInfo{};
			vertexBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			vertexBufferCreateInfo.size = (sizeof(Vertex) * vertexs.size());
			vertexBufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; // <--- Mind the usage flags!
			auto result = vkCreateBuffer(device, &vertexBufferCreateInfo, NULL, &newDrawModelData.mModelVertexs);
			if (result != VK_SUCCESS)
			{
				VKL_EXIT_WITH_ERROR(std::string("Failed to create buffer for model positions with error: ") + to_string(result));
			}
			VkMemoryRequirements memoryRequirements{};
			vkGetBufferMemoryRequirements(device, newDrawModelData.mModelVertexs, &memoryRequirements);
			newDrawModelData.mModelVertexsMemory = vklAllocateMemoryForGivenRequirements(vertexBufferCreateInfo.size, memoryRequirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			result = vkBindBufferMemory(device, newDrawModelData.mModelVertexs, newDrawModelData.mModelVertexsMemory, 0);
			if (result != VK_SUCCESS)
			{
				VKL_EXIT_WITH_ERROR(std::string("Failed to bind the buffer memory for the model positions with error: ") + to_string(result));
			}

			void* mappedMemory;
			result = vkMapMemory(device, newDrawModelData.mModelVertexsMemory, 0, vertexBufferCreateInfo.size, 0, &mappedMemory);
			if (result != VK_SUCCESS)
			{
				VKL_EXIT_WITH_ERROR(std::string("Failed to map the memory for the model positions with error: ") + to_string(result));
			}

			memcpy(mappedMemory, vertexs.data(), vertexBufferCreateInfo.size);
			vkUnmapMemory(device, newDrawModelData.mModelVertexsMemory);
		}

		// 2. INDICES BUFFER (Buffer, Memory, Bind 'em together, copy data into it)
		{
			VkBufferCreateInfo indexBufferCreateInfo{};
			indexBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			indexBufferCreateInfo.size = sizeof(indices[0]) * indices.size();
			indexBufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT; // <--- Mind the usage flags!
			auto result = vkCreateBuffer(device, &indexBufferCreateInfo, NULL, &newDrawModelData.mModelIndices);
			if (result != VK_SUCCESS)
			{
				VKL_EXIT_WITH_ERROR(std::string("Failed to create buffer for model indices with error: ") + to_string(result));
			}
			VkMemoryRequirements memoryRequirements{};
			vkGetBufferMemoryRequirements(device, newDrawModelData.mModelIndices, &memoryRequirements);
			newDrawModelData.mModelIndicesMemory = vklAllocateMemoryForGivenRequirements(indexBufferCreateInfo.size, memoryRequirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			result = vkBindBufferMemory(device, newDrawModelData.mModelIndices, newDrawModelData.mModelIndicesMemory, 0);
			if (result != VK_SUCCESS)
			{
				VKL_EXIT_WITH_ERROR(std::string("Failed to bind the buffer memory for the model indices with error: ") + to_string(result));
			}
			// Copy the indices into the buffer:
			void* mappedMemory;
			result = vkMapMemory(device, newDrawModelData.mModelIndicesMemory, 0, indexBufferCreateInfo.size, 0, &mappedMemory);
			if (result != VK_SUCCESS)
			{
				VKL_EXIT_WITH_ERROR(std::string("Failed to map the memory for the model indices with error: ") + to_string(result));
			}
			memcpy(mappedMemory, indices.data(), indexBufferCreateInfo.size);
			vkUnmapMemory(device, newDrawModelData.mModelIndicesMemory);
		}

		// 3. Normal BUFFER (Buffer, Memory, Bind 'em together, copy data into it) 
		//{
		//	VkBufferCreateInfo normalBufferCreateInfo{};
		//	normalBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		//	normalBufferCreateInfo.size = sizeof(normals[0]) * normals.size();
		//	normalBufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		//	auto result = vkCreateBuffer(device, &normalBufferCreateInfo, NULL, &newDrawModelData.mModelNormals);
		//	if (result != VK_SUCCESS)
		//	{
		//		VKL_EXIT_WITH_ERROR(std::string("Failed to create buffer for model normals with error: ") + std::to_string(result));
		//	}
		//	VkMemoryRequirements memoryRequirements{};
		//	vkGetBufferMemoryRequirements(device, newDrawModelData.mModelNormals, &memoryRequirements);
		//	newDrawModelData.mModelNormalsMemory = vklAllocateMemoryForGivenRequirements(normalBufferCreateInfo.size, memoryRequirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		//	result = vkBindBufferMemory(device, newDrawModelData.mModelNormals, newDrawModelData.mModelNormalsMemory, 0);
		//	if (result != VK_SUCCESS)
		//	{
		//		VKL_EXIT_WITH_ERROR(std::string("Failed to bind the buffer memory for the model normals with error: ") + std::to_string(result));
		//	}
		//	void* mappedMemory;
		//	result = vkMapMemory(device, newDrawModelData.mModelNormalsMemory, 0, normalBufferCreateInfo.size, 0, &mappedMemory);
		//	if (result != VK_SUCCESS)
		//	{
		//		VKL_EXIT_WITH_ERROR(std::string("Failed to map the memory for the model normals with error: ") + std::to_string(result));
		//	}
		//	memcpy(mappedMemory, normals.data(), normalBufferCreateInfo.size);
		//	vkUnmapMemory(device, newDrawModelData.mModelNormalsMemory);
		//}

		mDrawModelDatas.push_back(newDrawModelData);
	}

	void Model::destroyAllModelBuffers()
	{
		auto device = vklGetDevice();

		for (int i = 0; i < mDrawModelDatas.size(); i++)
		{
			vkFreeMemory(device, mDrawModelDatas[i].mModelIndicesMemory, NULL);
			vkDestroyBuffer(device, mDrawModelDatas[i].mModelIndices, NULL);
			vkFreeMemory(device, mDrawModelDatas[i].mModelVertexsMemory, NULL);
			vkDestroyBuffer(device, mDrawModelDatas[i].mModelVertexs, NULL);
		}
	}

	VkBuffer Model::getModelVertexsBuffer(int index)
	{
		if (index >= mDrawModelDatas.size())
		{
			VKL_EXIT_WITH_ERROR("Model index out of bounds");
		}

		return mDrawModelDatas[index].mModelVertexs;
	}

	VkBuffer Model::getModelIndicesBuffer(int index)
	{
		if (index >= mDrawModelDatas.size())
		{
			VKL_EXIT_WITH_ERROR("Model index out of bounds");
		}

		return mDrawModelDatas[index].mModelIndices;
	}

	uint32_t Model::getNumModelIndices(int index)
	{
		if (index >= mDrawModelDatas.size())
		{
			VKL_EXIT_WITH_ERROR("Model index out of bounds");
		}

		return mDrawModelDatas[index].mNumModelIndices;
	}

	void Model::drawModel(int index)
	{
		if (!vklFrameworkInitialized())
		{
			VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to invoke vklFrameworkInitialized beforehand!");
		}

		if (index >= mDrawModelDatas.size())
		{
			VKL_EXIT_WITH_ERROR("Model index out of bounds");
		}

		const vk::CommandBuffer& cb = vklGetCurrentCommandBuffer();
		auto currentSwapChainImageIndex = vklGetCurrentSwapChainImageIndex();
		assert(currentSwapChainImageIndex < vklGetNumFramebuffers());
		assert(currentSwapChainImageIndex < vklGetNumClearValues());

		cb.bindPipeline(vk::PipelineBindPoint::eGraphics, vklGetBasicPipeline());

		cb.bindVertexBuffers(0u, { vk::Buffer{ getModelVertexsBuffer(index)} }, { vk::DeviceSize{0} });
		cb.bindIndexBuffer(mDrawModelDatas[index].mModelIndices, vk::DeviceSize{ 0 }, vk::IndexType::eUint32);
		cb.drawIndexed(mDrawModelDatas[index].mNumModelIndices, 1u, 0, 0, 0u);
	}

	void Model::drawModel(int index, VkPipeline pipeline)
	{
		if (!vklFrameworkInitialized())
		{
			VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to invoke vklFrameworkInitialized beforehand!");
		}

		if (index >= mDrawModelDatas.size())
		{
			VKL_EXIT_WITH_ERROR("Model index out of bounds");
		}

		const vk::CommandBuffer& cb = vklGetCurrentCommandBuffer();
		auto currentSwapChainImageIndex = vklGetCurrentSwapChainImageIndex();
		assert(currentSwapChainImageIndex < vklGetNumFramebuffers());
		assert(currentSwapChainImageIndex < vklGetNumClearValues());

		auto pipe = vk::Pipeline{ pipeline };
		cb.bindPipeline(vk::PipelineBindPoint::eGraphics, pipe);

		cb.bindVertexBuffers(0u, { vk::Buffer{ getModelVertexsBuffer(index)} }, { vk::DeviceSize{0} });
		cb.bindIndexBuffer(vk::Buffer{ getModelIndicesBuffer(index) }, vk::DeviceSize{ 0 }, vk::IndexType::eUint32);
		cb.drawIndexed(getNumModelIndices(index), 1u, 0, 0, 0u);
	}

	void Model::drawModel(int index, VkPipeline pipeline, VkDescriptorSet descriptor_set)
	{
		if (index >= mDrawModelDatas.size())
		{
			VKL_EXIT_WITH_ERROR("Model index out of bounds");
		}

		vklBindDescriptorSetToPipeline(descriptor_set, pipeline);
		drawModel(index, pipeline);
	}

	void drawAllModel(VkPipeline pipeline, VkDescriptorSet descriptor_set)
	{
		vklBindDescriptorSetToPipeline(descriptor_set, pipeline);
		if (!vklFrameworkInitialized())
		{
			VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to invoke vklFrameworkInitialized beforehand!");
		}

		const vk::CommandBuffer& cb = vklGetCurrentCommandBuffer();
		auto currentSwapChainImageIndex = vklGetCurrentSwapChainImageIndex();
		assert(currentSwapChainImageIndex < vklGetNumFramebuffers());
		assert(currentSwapChainImageIndex < vklGetNumClearValues());

		auto pipe = vk::Pipeline{ pipeline };
		cb.bindPipeline(vk::PipelineBindPoint::eGraphics, pipe);

		for (int i = 0; i < mDrawModelDatas.size(); i++)
		{
			cb.bindVertexBuffers(0u, { vk::Buffer{ getModelVertexsBuffer(i)} }, { vk::DeviceSize{0} });
			cb.bindIndexBuffer(vk::Buffer{ getModelIndicesBuffer(i) }, vk::DeviceSize{ 0 }, vk::IndexType::eUint32);
			cb.drawIndexed(getNumModelIndices(i), 1u, 0, 0, 0u);
		}
	}
};

#endif // LOADMODEL_H

