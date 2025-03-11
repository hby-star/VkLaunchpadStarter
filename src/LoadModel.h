#ifndef LOADMODEL_H
#define LOADMODEL_H

#include "LoadModel.h"
#include <VulkanLaunchpad.h>
#include <vulkan/vulkan.hpp> 
#include <string>
#include <vector>
#include <glm/glm.hpp> 

class Model
{
public:
	Model() = default;
	~Model() = default;

	void loadModelAndCreateGeometryAndBuffers(const std::string& path_to_obj);
	void destroyModelBuffers();

	VkBuffer getModelPositionsBuffer();
	VkBuffer getModelIndicesBuffer();
	uint32_t getNumModelIndices();

	void drawModel();
	void drawModel(VkPipeline pipeline);
	void drawModel(VkPipeline pipeline, VkDescriptorSet descriptor_set);

private:
	VklGeometryData mGeometryData;
	uint32_t mNumModelIndices = 0;
	VkBuffer mModelPositions = VK_NULL_HANDLE;
	VkDeviceMemory mModelPositionsMemory = VK_NULL_HANDLE;
	VkBuffer mModelIndices = VK_NULL_HANDLE;
	VkDeviceMemory mModelIndicesMemory = VK_NULL_HANDLE;
};

#endif // LOADMODEL_H