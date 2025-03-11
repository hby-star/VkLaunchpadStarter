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
	VkBuffer mModelPositions = VK_NULL_HANDLE;
	VkDeviceMemory mModelPositionsMemory = VK_NULL_HANDLE;
	VkBuffer mModelIndices = VK_NULL_HANDLE;
	VkDeviceMemory mModelIndicesMemory = VK_NULL_HANDLE;
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
		std::vector<glm::vec3> positions = newDrawModelData.mGeometryData.positions;
		for (size_t i = 0; i < positions.size(); i++)
		{
			positions[i] = positions[i] + trans;
		}
		std::vector<uint32_t> indices = newDrawModelData.mGeometryData.indices;
		std::vector<glm::vec3> normals = newDrawModelData.mGeometryData.normals;
		std::vector<glm::vec2> textureCoordinates = newDrawModelData.mGeometryData.textureCoordinates;

		newDrawModelData.mNumModelIndices = static_cast<uint32_t>(indices.size());
		const auto device = vklGetDevice();
		auto dispatchLoader = vk::DispatchLoaderStatic();

		// 1. POSITIONS BUFFER (Buffer, Memory, Bind 'em together, copy data into it)
		{
			VkBufferCreateInfo posBufferCreateInfo{};
			posBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			posBufferCreateInfo.size = (sizeof(positions[0]) * positions.size());
			posBufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; // <--- Mind the usage flags!
			auto result = vkCreateBuffer(device, &posBufferCreateInfo, NULL, &newDrawModelData.mModelPositions);
			if (result != VK_SUCCESS)
			{
				VKL_EXIT_WITH_ERROR(std::string("Failed to create buffer for model positions with error: ") + to_string(result));
			}
			VkMemoryRequirements memoryRequirements{};
			vkGetBufferMemoryRequirements(device, newDrawModelData.mModelPositions, &memoryRequirements);
			newDrawModelData.mModelPositionsMemory = vklAllocateMemoryForGivenRequirements(posBufferCreateInfo.size, memoryRequirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			result = vkBindBufferMemory(device, newDrawModelData.mModelPositions, newDrawModelData.mModelPositionsMemory, 0);
			if (result != VK_SUCCESS)
			{
				VKL_EXIT_WITH_ERROR(std::string("Failed to bind the buffer memory for the model positions with error: ") + to_string(result));
			}
			// Copy the positions into the buffer:
			void* mappedMemory;
			result = vkMapMemory(device, newDrawModelData.mModelPositionsMemory, 0, posBufferCreateInfo.size, 0, &mappedMemory);
			if (result != VK_SUCCESS)
			{
				VKL_EXIT_WITH_ERROR(std::string("Failed to map the memory for the model positions with error: ") + to_string(result));
			}
			memcpy(mappedMemory, positions.data(), posBufferCreateInfo.size);
			vkUnmapMemory(device, newDrawModelData.mModelPositionsMemory);
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

		mDrawModelDatas.push_back(newDrawModelData);
	}

	void Model::destroyAllModelBuffers()
	{
		auto device = vklGetDevice();

		for (int i = 0; i < mDrawModelDatas.size(); i++)
		{
			vkFreeMemory(device, mDrawModelDatas[i].mModelIndicesMemory, NULL);
			vkDestroyBuffer(device, mDrawModelDatas[i].mModelIndices, NULL);
			vkFreeMemory(device, mDrawModelDatas[i].mModelPositionsMemory, NULL);
			vkDestroyBuffer(device, mDrawModelDatas[i].mModelPositions, NULL);
		}
	}

	VkBuffer Model::getModelPositionsBuffer(int index)
	{
		if (index >= mDrawModelDatas.size())
		{
			VKL_EXIT_WITH_ERROR("Model index out of bounds");
		}

		return mDrawModelDatas[index].mModelPositions;
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

		cb.bindVertexBuffers(0u, { mDrawModelDatas[index].mModelPositions }, { vk::DeviceSize{ 0 } });
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

		cb.bindVertexBuffers(0u, { vk::Buffer{ getModelPositionsBuffer(index)} }, { vk::DeviceSize{0} });
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
			cb.bindVertexBuffers(0u, { vk::Buffer{ getModelPositionsBuffer(i)} }, { vk::DeviceSize{0} });
			cb.bindIndexBuffer(vk::Buffer{ getModelIndicesBuffer(i) }, vk::DeviceSize{ 0 }, vk::IndexType::eUint32);
			cb.drawIndexed(getNumModelIndices(i), 1u, 0, 0, 0u);
		}
	}
};

#endif // LOADMODEL_H

