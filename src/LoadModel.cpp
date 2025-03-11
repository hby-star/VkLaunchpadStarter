# include "LoadModel.h"

void Model::loadModelAndCreateGeometryAndBuffers(const std::string& path_to_obj)
{
	mGeometryData = vklLoadModelGeometry(path_to_obj);
	std::vector<glm::vec3> positions = mGeometryData.positions;
	std::vector<uint32_t> indices = mGeometryData.indices;
	std::vector<glm::vec3> normals = mGeometryData.normals;
	std::vector<glm::vec2> textureCoordinates = mGeometryData.textureCoordinates;

	mNumModelIndices = static_cast<uint32_t>(indices.size());
	const auto device = vklGetDevice();
	auto dispatchLoader = vk::DispatchLoaderStatic();

	// 1. POSITIONS BUFFER (Buffer, Memory, Bind 'em together, copy data into it)
	{
		VkBufferCreateInfo posBufferCreateInfo{};
		posBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		posBufferCreateInfo.size = (sizeof(positions[0]) * positions.size());
		posBufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; // <--- Mind the usage flags!
		auto result = vkCreateBuffer(device, &posBufferCreateInfo, NULL, &mModelPositions);
		if (result != VK_SUCCESS)
		{
			VKL_EXIT_WITH_ERROR(std::string("Failed to create buffer for model positions with error: ") + to_string(result));
		}
		VkMemoryRequirements memoryRequirements{};
		vkGetBufferMemoryRequirements(device, mModelPositions, &memoryRequirements);
		mModelPositionsMemory = vklAllocateMemoryForGivenRequirements(posBufferCreateInfo.size, memoryRequirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		result = vkBindBufferMemory(device, mModelPositions, mModelPositionsMemory, 0);
		if (result != VK_SUCCESS)
		{
			VKL_EXIT_WITH_ERROR(std::string("Failed to bind the buffer memory for the model positions with error: ") + to_string(result));
		}
		// Copy the positions into the buffer:
		void* mappedMemory;
		result = vkMapMemory(device, mModelPositionsMemory, 0, posBufferCreateInfo.size, 0, &mappedMemory);
		if (result != VK_SUCCESS)
		{
			VKL_EXIT_WITH_ERROR(std::string("Failed to map the memory for the model positions with error: ") + to_string(result));
		}
		memcpy(mappedMemory, positions.data(), posBufferCreateInfo.size);
		vkUnmapMemory(device, mModelPositionsMemory);
	}

	// 2. INDICES BUFFER (Buffer, Memory, Bind 'em together, copy data into it)
	{
		VkBufferCreateInfo indexBufferCreateInfo{};
		indexBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		indexBufferCreateInfo.size = sizeof(indices[0]) * indices.size();
		indexBufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT; // <--- Mind the usage flags!
		auto result = vkCreateBuffer(device, &indexBufferCreateInfo, NULL, &mModelIndices);
		if (result != VK_SUCCESS)
		{
			VKL_EXIT_WITH_ERROR(std::string("Failed to create buffer for model indices with error: ") + to_string(result));
		}
		VkMemoryRequirements memoryRequirements{};
		vkGetBufferMemoryRequirements(device, mModelIndices, &memoryRequirements);
		mModelIndicesMemory = vklAllocateMemoryForGivenRequirements(indexBufferCreateInfo.size, memoryRequirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		result = vkBindBufferMemory(device, mModelIndices, mModelIndicesMemory, 0);
		if (result != VK_SUCCESS)
		{
			VKL_EXIT_WITH_ERROR(std::string("Failed to bind the buffer memory for the model indices with error: ") + to_string(result));
		}
		// Copy the indices into the buffer:
		void* mappedMemory;
		result = vkMapMemory(device, mModelIndicesMemory, 0, indexBufferCreateInfo.size, 0, &mappedMemory);
		if (result != VK_SUCCESS)
		{
			VKL_EXIT_WITH_ERROR(std::string("Failed to map the memory for the model indices with error: ") + to_string(result));
		}
		memcpy(mappedMemory, indices.data(), indexBufferCreateInfo.size);
		vkUnmapMemory(device, mModelIndicesMemory);
	}
}

void Model::destroyModelBuffers()
{
	auto device = vklGetDevice();
	vkFreeMemory(device, mModelIndicesMemory, NULL);
	vkDestroyBuffer(device, mModelIndices, NULL);
	vkFreeMemory(device, mModelPositionsMemory, NULL);
	vkDestroyBuffer(device, mModelPositions, NULL);
}

VkBuffer Model::getModelPositionsBuffer()
{
	return mModelPositions;
}

VkBuffer Model::getModelIndicesBuffer()
{
	return mModelIndices;
}

uint32_t Model::getNumModelIndices()
{
	return mNumModelIndices;
}

void Model::drawModel()
{
	if (!vklFrameworkInitialized())
	{
		VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to invoke vklFrameworkInitialized beforehand!");
	}

	const vk::CommandBuffer& cb = vklGetCurrentCommandBuffer();
	auto currentSwapChainImageIndex = vklGetCurrentSwapChainImageIndex();
	assert(currentSwapChainImageIndex < vklGetNumFramebuffers());
	assert(currentSwapChainImageIndex < vklGetNumClearValues());

	cb.bindPipeline(vk::PipelineBindPoint::eGraphics, vklGetBasicPipeline());

	cb.bindVertexBuffers(0u, { mModelPositions }, { vk::DeviceSize{ 0 } });
	cb.bindIndexBuffer(mModelIndices, vk::DeviceSize{ 0 }, vk::IndexType::eUint32);
	cb.drawIndexed(mNumModelIndices, 1u, 0u, 0, 0u);
}

void Model::drawModel(VkPipeline pipeline)
{
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

	cb.bindVertexBuffers(0u, { vk::Buffer{ getModelPositionsBuffer()} }, { vk::DeviceSize{0} });
	cb.bindIndexBuffer(vk::Buffer{ getModelIndicesBuffer() }, vk::DeviceSize{ 0 }, vk::IndexType::eUint32);
	cb.drawIndexed(getNumModelIndices(), 1u, 0u, 0, 0u);
}

void Model::drawModel(VkPipeline pipeline, VkDescriptorSet descriptor_set)
{
	vklBindDescriptorSetToPipeline(descriptor_set, pipeline);
	drawModel(pipeline);
}
