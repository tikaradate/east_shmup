#include "vulkan_app.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>

static const char* requiredDeviceExtensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

static const uint32_t requiredDeviceExtensionCount =
    sizeof(requiredDeviceExtensions) / sizeof(requiredDeviceExtensions[0]);

static const bool enableValidationLayers = true;

static const char *validationLayers[] = {"VK_LAYER_KHRONOS_validation"};

struct Vertex {
    float pos[2];
    float color[3];
};

static const Vertex vertices[] = {
    {{ 0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
};

static bool findMemoryType(
    VkPhysicalDevice physicalDevice,
    uint32_t typeFilter,
    VkMemoryPropertyFlags properties,
    uint32_t *memoryTypeIndex
){
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for(uint32_t i = 0; i < memProperties.memoryTypeCount; i++){
        if((typeFilter & (1 << i)) &&
           (memProperties.memoryTypes[i].propertyFlags & properties) == properties){
            *memoryTypeIndex = i;
            return true;
        }
    }

    fprintf(stderr, "failed to find suitable memory type\n");
    return false;
}

static bool createBuffer(
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer *buffer,
    VkDeviceMemory *bufferMemory
){
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if(vkCreateBuffer(device, &bufferInfo, nullptr, buffer) != VK_SUCCESS){
        fprintf(stderr, "failed to create buffer\n");
        return false;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, *buffer, &memRequirements);

    uint32_t memoryTypeIndex = 0;
    if(!findMemoryType(
        physicalDevice,
        memRequirements.memoryTypeBits,
        properties,
        &memoryTypeIndex
    )){
        vkDestroyBuffer(device, *buffer, nullptr);
        *buffer = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    if(vkAllocateMemory(device, &allocInfo, nullptr, bufferMemory) != VK_SUCCESS){
        fprintf(stderr, "failed to allocate buffer memory\n");
        vkDestroyBuffer(device, *buffer, nullptr);
        *buffer = VK_NULL_HANDLE;
        return false;
    }

    if(vkBindBufferMemory(device, *buffer, *bufferMemory, 0) != VK_SUCCESS){
        fprintf(stderr, "failed to bind buffer memory\n");
        vkFreeMemory(device, *bufferMemory, nullptr);
        vkDestroyBuffer(device, *buffer, nullptr);
        *bufferMemory = VK_NULL_HANDLE;
        *buffer = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

static bool createVertexBuffer(VulkanApp *app){
    VkDeviceSize bufferSize = sizeof(vertices);

    if(!createBuffer(
        app->physicalDevice,
        app->device,
        bufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &app->vertexBuffer,
        &app->vertexBufferMemory
    )){
        return false;
    }

    void *data = nullptr;
    if(vkMapMemory(app->device, app->vertexBufferMemory, 0, bufferSize, 0, &data) != VK_SUCCESS){
        fprintf(stderr, "failed to map vertex buffer memory\n");

        vkDestroyBuffer(app->device, app->vertexBuffer, nullptr);
        vkFreeMemory(app->device, app->vertexBufferMemory, nullptr);
        app->vertexBuffer = VK_NULL_HANDLE;
        app->vertexBufferMemory = VK_NULL_HANDLE;

        return false;
    }

    memcpy(data, vertices, (size_t)bufferSize);
    vkUnmapMemory(app->device, app->vertexBufferMemory);

    return true;
}
static bool drawFrameRaw(VkDevice device, VkSwapchainKHR swapchain, VkQueue graphicsQueue, VkQueue presentQueue, const std::vector<VkCommandBuffer> &commandBuffers, VkSemaphore imageAvailableSemaphore, VkSemaphore renderFinishedSemaphore, VkFence inFlightFence){
    VkResult result = vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);

    if(result != VK_SUCCESS){
        std::fprintf(stderr, "Failed to wait for fences\n");
        return false;
    }

    uint32_t imageIndex = 0;
    result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

    if(result != VK_SUCCESS){
        std::fprintf(stderr, "Failed to acquire next image\n");
        return false;
    }

    result = vkResetFences(device, 1, &inFlightFence);

    if(result != VK_SUCCESS){
        std::fprintf(stderr, "Failed to reset fences\n");
        return false;
    }

    VkSemaphore waitSemaphores[] = { imageAvailableSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore signalSemaphores[] = { renderFinishedSemaphore };

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence);

    if(result != VK_SUCCESS){
        std::fprintf(stderr, "Failed to submit queue\n");
        return false;
    }

    VkSwapchainKHR swapchains[] = { swapchain };

    VkPresentInfoKHR presentInfo = {};  
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(presentQueue, &presentInfo);

    if(result != VK_SUCCESS){
        std::fprintf(stderr, "Failed to present queue\n");
        return false;
    }

    return true;
}


static bool createSyncObjects(VkDevice device, VkSemaphore *imageAvailableSemaphore, VkSemaphore *renderFinishedSemaphore, VkFence *inFlightFence){
    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if(vkCreateSemaphore(device, &semaphoreInfo, nullptr, imageAvailableSemaphore) != VK_SUCCESS){
        std::fprintf(stderr, "Failed to create image available semaphore\n");
        return false;
    }

    if(vkCreateSemaphore(device, &semaphoreInfo, nullptr, renderFinishedSemaphore) != VK_SUCCESS){
        std::fprintf(stderr, "Failed to create render finished semaphore\n");
        return false;
    }

    if(vkCreateFence(device, &fenceInfo, nullptr, inFlightFence) != VK_SUCCESS){
        std::fprintf(stderr, "Failed to create in-flight fence\n");
        return false;
    }

    return true;
}

static bool createCommandBuffers(VkDevice device, VkCommandPool commandPool, VkRenderPass renderPass, const std::vector<VkFramebuffer> &swapchainFramebuffers, VkExtent2D swapchainExtent, VkPipeline graphicsPipeline, VkBuffer vertexBuffer, std::vector<VkCommandBuffer> *commandBuffers){
    commandBuffers->resize(swapchainFramebuffers.size());

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers->size());

    VkResult result = vkAllocateCommandBuffers(
        device,
        &allocInfo,
        commandBuffers->data()
    );

    if(result != VK_SUCCESS){
        std::fprintf(stderr, "failed to allocate command buffers\n");
        commandBuffers->clear();
        return false;
    }

    for(size_t i = 0; i < commandBuffers->size(); i++){
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = nullptr;

        result = vkBeginCommandBuffer((*commandBuffers)[i], &beginInfo);

        if(result != VK_SUCCESS){
            std::fprintf(stderr, "failed to begin command buffer\n");
            return false;
        }

        VkClearValue clearColor = {};
        clearColor.color.float32[0] = 0.0f;
        clearColor.color.float32[1] = 0.0f;
        clearColor.color.float32[2] = 0.0f;
        clearColor.color.float32[3] = 1.0f;

        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = swapchainFramebuffers[i];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapchainExtent;
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(
            (*commandBuffers)[i],
            &renderPassInfo,
            VK_SUBPASS_CONTENTS_INLINE
        );

        vkCmdBindPipeline(
            (*commandBuffers)[i],
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            graphicsPipeline
        );

        VkBuffer vertexBuffers[] = {vertexBuffer};
        VkDeviceSize offsets[] = {0};

        vkCmdBindVertexBuffers(
            (*commandBuffers)[i],
            0,
            1,
            vertexBuffers,
            offsets
        );

        vkCmdDraw((*commandBuffers)[i], 3, 1, 0, 0);

        vkCmdEndRenderPass((*commandBuffers)[i]);

        result = vkEndCommandBuffer((*commandBuffers)[i]);

        if(result != VK_SUCCESS){
            std::fprintf(stderr, "failed to record command buffer\n");
            return false;
        }
    }

    return true;
}

static bool createCommandPool(VkDevice device, QueueFamilyIndices queueFamilyIndices, VkCommandPool *commandPool){
    VkCommandPoolCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

    createInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;

    createInfo.flags = 0;

    VkResult result = vkCreateCommandPool(
        device,
        &createInfo,
        nullptr,
        commandPool
    );

    if(result != VK_SUCCESS){
        std::fprintf(stderr, "failed to create command pool\n");
        return false;
    }

    return true;
}

static bool createGraphicsPipeline(VkDevice device, VkExtent2D swapchainExtent, VkRenderPass renderPass, VkShaderModule vertShaderModule, VkShaderModule fragShaderModule, VkPipelineLayout *pipelineLayout, VkPipeline *graphicsPipeline){
    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {
        vertShaderStageInfo,
        fragShaderStageInfo
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = 1;
    
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    
    VkVertexInputAttributeDescription attributeDescriptions[2] = {};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);

    vertexInputInfo.vertexAttributeDescriptionCount = 2;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;

    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchainExtent.width);
    viewport.height = static_cast<float>(swapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = swapchainExtent;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;

    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;

    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;

    rasterizer.cullMode = VK_CULL_MODE_NONE;

    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;

    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};

    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;

    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;

    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    VkResult result = vkCreatePipelineLayout(
        device,
        &pipelineLayoutInfo,
        nullptr,
        pipelineLayout
    );

    if(result != VK_SUCCESS){
        std::fprintf(stderr, "failed to create pipeline layout\n");
        return false;
    }

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;

    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = nullptr;

    pipelineInfo.layout = *pipelineLayout;

    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    result = vkCreateGraphicsPipelines(
        device,
        VK_NULL_HANDLE,
        1,
        &pipelineInfo,
        nullptr,
        graphicsPipeline
    );

    if(result != VK_SUCCESS){
        std::fprintf(stderr, "failed to create graphics pipeline\n");
        vkDestroyPipelineLayout(device, *pipelineLayout, nullptr);
        *pipelineLayout = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

static bool readBinaryFile(const char *path, std::vector<char> *buffer){
    FILE *file = std::fopen(path, "rb");

    if(file == nullptr){
        std::fprintf(stderr, "failed to open file: %s\n", path);
        return false;
    }

    if(std::fseek(file, 0, SEEK_END) != 0){
        std::fprintf(stderr, "failed to seek file: %s\n", path);
        std::fclose(file);
        return false;
    }

    long fileSize = std::ftell(file);

    if(fileSize < 0){
        std::fprintf(stderr, "failed to get file size: %s\n", path);
        std::fclose(file);
        return false;
    }

    std::rewind(file);

    buffer->resize(static_cast<size_t>(fileSize));

    size_t bytesRead = std::fread(
        buffer->data(),
        1,
        buffer->size(),
        file
    );

    std::fclose(file);

    if(bytesRead != buffer->size()){
        std::fprintf(stderr, "failed to read full file: %s\n", path);
        return false;
    }

    return true;
}

static bool createShaderModule(VkDevice device, const char *path, VkShaderModule *shaderModule){
    std::vector<char> code;

    if(!readBinaryFile(path, &code)){
        return false;
    }

    if(code.empty() || code.size() % 4 != 0){
        std::fprintf(stderr, "invalid SPIR-V file: %s\n", path);
        return false;
    }

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkResult result = vkCreateShaderModule(
        device,
        &createInfo,
        nullptr,
        shaderModule
    );

    if(result != VK_SUCCESS){
        std::fprintf(stderr, "failed to create shader module: %s\n", path);
        return false;
    }

    return true;
}

static bool createFramebuffers(VkDevice device, VkRenderPass renderPass, const std::vector<VkImageView> &swapchainImageViews, VkExtent2D swapchainExtent, std::vector<VkFramebuffer> *swapchainFramebuffers){
    swapchainFramebuffers->resize(swapchainImageViews.size());

    for(size_t i = 0; i < swapchainImageViews.size(); i++){
        VkImageView attachments[] = {
            swapchainImageViews[i]
        };

        VkFramebufferCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.renderPass = renderPass;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = attachments;
        createInfo.width = swapchainExtent.width;
        createInfo.height = swapchainExtent.height;
        createInfo.layers = 1;

        VkResult result = vkCreateFramebuffer(
            device,
            &createInfo,
            nullptr,
            &(*swapchainFramebuffers)[i]
        );

        if(result != VK_SUCCESS){
            std::fprintf(stderr, "failed to create framebuffer\n");

            for(size_t j = 0; j < i; j++){
                vkDestroyFramebuffer(device, (*swapchainFramebuffers)[j], nullptr);
            }

            swapchainFramebuffers->clear();
            return false;
        }
    }

    return true;
}

static bool createRenderPass(VkDevice device, VkFormat swapchainImageFormat, VkRenderPass *renderPass){
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = swapchainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;

    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;

    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

    createInfo.attachmentCount = 1;
    createInfo.pAttachments = &colorAttachment;

    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;

    createInfo.dependencyCount = 1;
    createInfo.pDependencies = &dependency;

    VkResult result = vkCreateRenderPass(device, &createInfo, nullptr, renderPass);

    if(result != VK_SUCCESS){
        std::fprintf(stderr, "failed to create render pass\n");
        return false;
    }

    return true;
}

static bool createSwapchainImageViews(VkDevice device, const std::vector<VkImage> &swapchainImages, VkFormat swapchainImageFormat, std::vector<VkImageView> *swapchainImageViews){
    swapchainImageViews->resize(swapchainImages.size());

    for(uint32_t i = 0; i < swapchainImages.size(); i++){
        VkImageViewCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapchainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapchainImageFormat;
    
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        VkResult result = vkCreateImageView(device, &createInfo, nullptr, &(*swapchainImageViews)[i]);

        if(result != VK_SUCCESS){
            std::fprintf(stderr, "failed to create swapchain image view\n");
            
            for(uint32_t j = 0; j < i; j++){
                vkDestroyImageView(device, (*swapchainImageViews)[j], nullptr);
            }

            swapchainImageViews->clear();

            return false;
        }
    }

    return true;
}

static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, GLFWwindow *window){
    if(capabilities.currentExtent.width != UINT32_MAX){
        return capabilities.currentExtent;
    } 
    int width = 0;
    int height = 0;

    glfwGetFramebufferSize(window, &width, &height);
    VkExtent2D actualExtent = {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)
    };

    actualExtent.width = std::clamp(
        actualExtent.width,
        capabilities.minImageExtent.width,
        capabilities.maxImageExtent.width
    );

    actualExtent.height = std::clamp(
        actualExtent.height,
        capabilities.minImageExtent.height,
        capabilities.maxImageExtent.height
    );

    return actualExtent;
}

static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes){
    for(const VkPresentModeKHR &presentMode : availablePresentModes){
        if(presentMode == VK_PRESENT_MODE_MAILBOX_KHR){
            return VK_PRESENT_MODE_MAILBOX_KHR;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats){
    for(const VkSurfaceFormatKHR &surfaceFormat : availableFormats){
        if(surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB && surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return surfaceFormat;
        }
    }

    return availableFormats[0];
}

static bool querySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface, SwapchainSupportDetails *details){
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details->capabilities);

    if(result != VK_SUCCESS){
        return false;
    }

    uint32_t formatCount = 0;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if(result != VK_SUCCESS){
        return false;
    }

    details->formats.resize(formatCount);
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details->formats.data());    
    
    if(result != VK_SUCCESS){
        return false;
    }

    uint32_t presentModeCount = 0;
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if(result != VK_SUCCESS){
        return false;
    }

    details->presentModes.resize(presentModeCount);
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details->presentModes.data());

    if(result != VK_SUCCESS){
        return false;
    }

    return true;
}

static bool createSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, GLFWwindow *window, QueueFamilyIndices queueFamilyIndices, VkSwapchainKHR *swapchain, std::vector<VkImage> *swapchainImages, VkFormat *swapchainImageFormat, VkExtent2D *swapchainExtent){
    SwapchainSupportDetails swapchainSupport = {};

    if(!querySwapchainSupport(physicalDevice, surface, &swapchainSupport)){
        return false;
    }

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapchainSupport.formats);

    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapchainSupport.presentModes);

    VkExtent2D extent = chooseSwapExtent(swapchainSupport.capabilities, window);

    uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;

    if(swapchainSupport.capabilities.maxImageCount > 0 &&
       imageCount > swapchainSupport.capabilities.maxImageCount){
        imageCount = swapchainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndicesArray[] = {queueFamilyIndices.graphicsFamily, queueFamilyIndices.presentFamily};

    if(queueFamilyIndices.graphicsFamily != queueFamilyIndices.presentFamily){
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndicesArray;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = swapchainSupport.capabilities.currentTransform;

    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(device, &createInfo, nullptr, swapchain);

    if(result != VK_SUCCESS){
        std::fprintf(stderr, "failed to create swapchain\n");
        return false;
    }

    uint32_t actualImageCount = 0;

    VkResult imageResult = vkGetSwapchainImagesKHR(device, *swapchain, &actualImageCount, nullptr);

    if(imageResult != VK_SUCCESS){
        std::fprintf(stderr, "failed to get swapchain image count\n");
        return false;
    }

    swapchainImages->resize(actualImageCount);

    imageResult = vkGetSwapchainImagesKHR(device, *swapchain, &actualImageCount, swapchainImages->data());

    if(imageResult != VK_SUCCESS){
        std::fprintf(stderr, "failed to get swapchain images\n");
        return false;
    }

    *swapchainImageFormat = surfaceFormat.format;
    *swapchainExtent = extent;
    
    return true;
}

static bool checkDeviceExtensionSupport(VkPhysicalDevice device){
    uint32_t extensionCount = 0;

    vkEnumerateDeviceExtensionProperties(
        device,
        nullptr,
        &extensionCount,
        nullptr
    );

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);

    vkEnumerateDeviceExtensionProperties(
        device,
        nullptr,
        &extensionCount,
        availableExtensions.data()
    );

    for(uint32_t i = 0; i < requiredDeviceExtensionCount; i++){
        bool found = false;

        for(uint32_t j = 0; j < extensionCount; j++){
            if(std::strcmp(
                requiredDeviceExtensions[i],
                availableExtensions[j].extensionName
            ) == 0){
                found = true;
                break;
            }
        }

        if(!found){
            return false;
        }
    }

    return true;
}

static bool createLogicalDevice(VkPhysicalDevice physicalDevice, QueueFamilyIndices queueFamilyIndices, VkDevice *device, VkQueue *graphicsQueue, VkQueue *presentQueue){
    float queuePriority = 1.0f;
    
    VkDeviceQueueCreateInfo queueCreateInfos[2] = {};
    uint32_t queueCreateInfoCount = 0;

    queueCreateInfos[queueCreateInfoCount].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfos[queueCreateInfoCount].queueFamilyIndex = queueFamilyIndices.graphicsFamily;
    queueCreateInfos[queueCreateInfoCount].queueCount = 1;
    queueCreateInfos[queueCreateInfoCount].pQueuePriorities = &queuePriority;

    queueCreateInfoCount++;

    if(queueFamilyIndices.graphicsFamily != queueFamilyIndices.presentFamily){
        queueCreateInfos[queueCreateInfoCount].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfos[queueCreateInfoCount].queueFamilyIndex = queueFamilyIndices.presentFamily;
        queueCreateInfos[queueCreateInfoCount].queueCount = 1;
        queueCreateInfos[queueCreateInfoCount].pQueuePriorities = &queuePriority;

        queueCreateInfoCount++;
    }
    
    VkPhysicalDeviceFeatures deviceFeatures = {};
    
    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = queueCreateInfoCount;
    createInfo.pQueueCreateInfos = queueCreateInfos;
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = requiredDeviceExtensionCount;
    createInfo.ppEnabledExtensionNames = requiredDeviceExtensions;
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = nullptr;
    
    VkResult result = vkCreateDevice(physicalDevice, &createInfo, nullptr, device);

    if(result != VK_SUCCESS){
        std::fprintf(stderr, "Failed to create logical device\n");
        return false;
    }
        std::fprintf(stdout, "created logical device\n");

    vkGetDeviceQueue(*device, queueFamilyIndices.graphicsFamily, 0, graphicsQueue);

    if(queueFamilyIndices.presentFamily == queueFamilyIndices.graphicsFamily){
        *presentQueue = *graphicsQueue;
    } else {
        vkGetDeviceQueue(*device, queueFamilyIndices.presentFamily, 0, presentQueue);
    }

    return true;
}

static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface){
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for(uint32_t i = 0; i < queueFamilyCount; i++){
        if(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT){
            indices.graphicsFamily = i;
            indices.hasGraphicsFamily = true;
        }

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

        if(presentSupport){
            indices.presentFamily = i;
            indices.hasPresentFamily = true;
        }

        if(indices.isComplete()){
            break;
        }
    }

    return indices;
}

static bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface){
    QueueFamilyIndices indices = findQueueFamilies(device, surface);
    
    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapchainAdequate = false;
    if(extensionsSupported){
        SwapchainSupportDetails swapchainSupport;
        if(querySwapchainSupport(device, surface, &swapchainSupport)){
            swapchainAdequate =
                !swapchainSupport.formats.empty()
                && !swapchainSupport.presentModes.empty();
        }
    }

    return indices.isComplete() && extensionsSupported && swapchainAdequate;
}

static bool pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, VkPhysicalDevice *device, QueueFamilyIndices *queueFamilyIndices){
    uint32_t physicalCount = 0;
    VkResult result = vkEnumeratePhysicalDevices(instance, &physicalCount, nullptr);

    if((physicalCount == 0 ) || (result != VK_SUCCESS)) {
        return false;
    }

    
    std::vector<VkPhysicalDevice> physicalDevices(physicalCount);
    result = vkEnumeratePhysicalDevices(instance, &physicalCount, physicalDevices.data());
    
    if(result != VK_SUCCESS){
        return false;
    }
    
    
    for(const VkPhysicalDevice &current_device : physicalDevices){
        if(isDeviceSuitable(current_device, surface)){
            *device = current_device;
            *queueFamilyIndices = findQueueFamilies(*device, surface);
            return true;
        }
    }
    
    return false;
}

static bool checkValidationLayerSupport(){
    uint32_t layerCount = 0;
    VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    
    if(result != VK_SUCCESS){
        return false;
    }

    std::vector<VkLayerProperties> availableLayers(layerCount);

    result = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    if(result != VK_SUCCESS){
        return false;
    }

    for(const VkLayerProperties &layer : availableLayers){
        if(std::strcmp(layer.layerName, validationLayers[0]) == 0){
            return true;
        }
    }

    return false;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData
){
    std::fprintf(stderr, "%s\n", pCallbackData->pMessage);
    return VK_FALSE;
}

static bool createSurface(VkInstance instance, GLFWwindow *window, VkSurfaceKHR *surface){
    VkResult surfaceResult = glfwCreateWindowSurface(instance, window, nullptr, surface);

    if(surfaceResult != VK_SUCCESS){
        std::fprintf(stderr, "Failed to create window surface: VkResult %d\n", surfaceResult);

        return false;
    }

    return true;
}

static void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT *createInfo){
    createInfo->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo->messageSeverity = 
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo->messageType = 
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo->pfnUserCallback = debugCallback;
    createInfo->pUserData = nullptr;
}

static bool setupDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT *debugMessenger){
    if(!enableValidationLayers) {
        return true;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(&createInfo);

    PFN_vkCreateDebugUtilsMessengerEXT func = 
        reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT")
        );

    if(func == nullptr){
        std::fprintf(stderr, "Failed to load vkCreateDebugUtilsMessengerEXT\n");
        return false;
    }

    VkResult result = func(instance, &createInfo, nullptr, debugMessenger);

    if(result != VK_SUCCESS){
        std::fprintf(stderr, "Failed to create debug messenger: VkResult %d\n", result);
        return false;
    }

    return true;
}

static void destroyDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT *debugMessenger){
    if(!enableValidationLayers) {
        return;
    }

    if(*debugMessenger == VK_NULL_HANDLE){
        return;
    }

    PFN_vkDestroyDebugUtilsMessengerEXT func = 
        reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT")
        );

    if(func == nullptr){
        std::fprintf(stderr, "Failed to load vkDestroyDebugUtilsMessengerEXT\n");
        return;
    } else {
        func(instance, *debugMessenger, nullptr);
    }
}

static bool createInstance(VkInstance *instance) {
    if(enableValidationLayers && !checkValidationLayerSupport()){
        std::fprintf(stderr, "Validation layers requested, but not available\n");
        return false;
    }
    
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "East Shmup";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "East Shmup Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    uint32_t extensionCount = 0;
    const char **extensions = glfwGetRequiredInstanceExtensions(&extensionCount);

    if (extensions == nullptr || extensionCount == 0) {
        std::fprintf(stderr, "GLFW did not return required Vulkan extensions\n");
        return false;
    }

    std::vector<const char*> enabledExtensions;
    for(uint32_t i = 0; i < extensionCount; i++){
        enabledExtensions.push_back(extensions[i]);
    }
    if(enableValidationLayers){
        enabledExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = enabledExtensions.size();
    createInfo.ppEnabledExtensionNames = enabledExtensions.data();

    if(enableValidationLayers){
        createInfo.enabledLayerCount = 1;
        createInfo.ppEnabledLayerNames = validationLayers;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.ppEnabledLayerNames = nullptr;
    }

    VkResult result = vkCreateInstance(&createInfo, nullptr, instance);

    if (result != VK_SUCCESS) {
        std::fprintf(stderr, "Failed to create Vulkan instance: VkResult %d\n", result);
        return false;
    }

    return true;
}

bool initVulkan(VulkanApp *app, GLFWwindow *window){
    app->window = window;

    if(!createInstance(&app->instance)){
        return false;
    }

    std::fprintf(stdout, "Vulkan instance created\n");

    if(!setupDebugMessenger(app->instance, &app->debugMessenger)){
        return false;
    }

    std::fprintf(stdout, "DebugMessenger created\n");

    if(!createSurface(app->instance, app->window, &app->surface)){
        return false;
    }

    std::fprintf(stdout, "Surface created\n");

    if(!pickPhysicalDevice(
        app->instance,
        app->surface,
        &app->physicalDevice,
        &app->queueFamilyIndices
    )){
        return false;
    }

    std::fprintf(stdout, "Vulkan physical device selected\n");
    std::fprintf(stdout, "graphics family: %d\n", app->queueFamilyIndices.graphicsFamily);
    std::fprintf(stdout, "present family: %d\n", app->queueFamilyIndices.presentFamily);

    if(!createLogicalDevice(
        app->physicalDevice,
        app->queueFamilyIndices,
        &app->device,
        &app->graphicsQueue,
        &app->presentQueue
    )){
        return false;
    }

    std::fprintf(stdout, "Vulkan logical device created\n");

    if(!createSwapchain(
        app->physicalDevice,
        app->device,
        app->surface,
        app->window,
        app->queueFamilyIndices,
        &app->swapchain,
        &app->swapchainImages,
        &app->swapchainImageFormat,
        &app->swapchainExtent
    )){
        return false;
    }

    std::fprintf(stdout, "Swapchain created\n");
    std::fprintf(stdout, "swapchain image count: %zu\n", app->swapchainImages.size());

    if(!createSwapchainImageViews(
        app->device,
        app->swapchainImages,
        app->swapchainImageFormat,
        &app->swapchainImageViews
    )){
        return false;
    }

    std::fprintf(stdout, "Swapchain image views created\n");

    if(!createRenderPass(
        app->device,
        app->swapchainImageFormat,
        &app->renderPass
    )){
        return false;
    }

    std::fprintf(stdout, "Render pass created\n");

    if(!createFramebuffers(
        app->device,
        app->renderPass,
        app->swapchainImageViews,
        app->swapchainExtent,
        &app->swapchainFramebuffers
    )){
        return false;
    }

    std::fprintf(stdout, "Framebuffers created\n");

    VkShaderModule vertShaderModule = VK_NULL_HANDLE;
    VkShaderModule fragShaderModule = VK_NULL_HANDLE;

    if(!createShaderModule(app->device, "src/shaders/triangle.vert.spv", &vertShaderModule)){
        return false;
    }

    if(!createShaderModule(app->device, "src/shaders/triangle.frag.spv", &fragShaderModule)){
        vkDestroyShaderModule(app->device, vertShaderModule, nullptr);
        return false;
    }

    std::fprintf(stdout, "Shader modules created\n");

    if(!createGraphicsPipeline(
        app->device,
        app->swapchainExtent,
        app->renderPass,
        vertShaderModule,
        fragShaderModule,
        &app->pipelineLayout,
        &app->graphicsPipeline
    )){
        vkDestroyShaderModule(app->device, fragShaderModule, nullptr);
        vkDestroyShaderModule(app->device, vertShaderModule, nullptr);
        return false;
    }

    std::fprintf(stdout, "Graphics pipeline created\n");

    vkDestroyShaderModule(app->device, fragShaderModule, nullptr);
    vkDestroyShaderModule(app->device, vertShaderModule, nullptr);

    std::fprintf(stdout, "Shader modules destroyed\n");

    if(!createCommandPool(
        app->device,
        app->queueFamilyIndices,
        &app->commandPool
    )){
        return false;
    }

    std::fprintf(stdout, "Command pool created\n");


    if(!createVertexBuffer(app)){
        return false;
    }

    std::fprintf(stdout, "Vertex buffer created\n");

    if(!createCommandBuffers(
        app->device,
        app->commandPool,
        app->renderPass,
        app->swapchainFramebuffers,
        app->swapchainExtent,
        app->graphicsPipeline,
        app->vertexBuffer,
        &app->commandBuffers
    )){
        return false;
    }

    std::fprintf(stdout, "Command buffers created\n");

    if(!createSyncObjects(
        app->device,
        &app->imageAvailableSemaphore,
        &app->renderFinishedSemaphore,
        &app->inFlightFence
    )){
        return false;
    }

    std::fprintf(stdout, "Sync objects created\n");

    return true;
}

bool drawFrame(VulkanApp *app){
    return drawFrameRaw(
        app->device,
        app->swapchain,
        app->graphicsQueue,
        app->presentQueue,
        app->commandBuffers,
        app->imageAvailableSemaphore,
        app->renderFinishedSemaphore,
        app->inFlightFence
    );
}

void cleanupVulkan(VulkanApp *app){
    if(app->device != VK_NULL_HANDLE){
        vkDeviceWaitIdle(app->device);
    }

    if(app->inFlightFence != VK_NULL_HANDLE){
        vkDestroyFence(app->device, app->inFlightFence, nullptr);
        app->inFlightFence = VK_NULL_HANDLE;
    }

    if(app->renderFinishedSemaphore != VK_NULL_HANDLE){
        vkDestroySemaphore(app->device, app->renderFinishedSemaphore, nullptr);
        app->renderFinishedSemaphore = VK_NULL_HANDLE;
    }

    if(app->imageAvailableSemaphore != VK_NULL_HANDLE){
        vkDestroySemaphore(app->device, app->imageAvailableSemaphore, nullptr);
        app->imageAvailableSemaphore = VK_NULL_HANDLE;
    }

    if(app->commandPool != VK_NULL_HANDLE){
        vkDestroyCommandPool(app->device, app->commandPool, nullptr);
        app->commandPool = VK_NULL_HANDLE;
        app->commandBuffers.clear();
        std::fprintf(stdout, "Command pool destroyed\n");
    }


    if(app->vertexBuffer != VK_NULL_HANDLE){
        vkDestroyBuffer(app->device, app->vertexBuffer, nullptr);
        app->vertexBuffer = VK_NULL_HANDLE;
    }

    if(app->vertexBufferMemory != VK_NULL_HANDLE){
        vkFreeMemory(app->device, app->vertexBufferMemory, nullptr);
        app->vertexBufferMemory = VK_NULL_HANDLE;
    }

    if(app->graphicsPipeline != VK_NULL_HANDLE){
        vkDestroyPipeline(app->device, app->graphicsPipeline, nullptr);
        app->graphicsPipeline = VK_NULL_HANDLE;
        std::fprintf(stdout, "Graphics pipeline destroyed\n");
    }

    if(app->pipelineLayout != VK_NULL_HANDLE){
        vkDestroyPipelineLayout(app->device, app->pipelineLayout, nullptr);
        app->pipelineLayout = VK_NULL_HANDLE;
        std::fprintf(stdout, "Pipeline layout destroyed\n");
    }

    for(size_t i = 0; i < app->swapchainFramebuffers.size(); i++){
        vkDestroyFramebuffer(app->device, app->swapchainFramebuffers[i], nullptr);
    }

    if(!app->swapchainFramebuffers.empty()){
        app->swapchainFramebuffers.clear();
        std::fprintf(stdout, "Framebuffers destroyed\n");
    }

    if(app->renderPass != VK_NULL_HANDLE){
        vkDestroyRenderPass(app->device, app->renderPass, nullptr);
        app->renderPass = VK_NULL_HANDLE;
        std::fprintf(stdout, "Render pass destroyed\n");
    }

    for(size_t i = 0; i < app->swapchainImageViews.size(); i++){
        vkDestroyImageView(app->device, app->swapchainImageViews[i], nullptr);
    }

    if(!app->swapchainImageViews.empty()){
        app->swapchainImageViews.clear();
        std::fprintf(stdout, "Swapchain image views destroyed\n");
    }

    if(app->swapchain != VK_NULL_HANDLE){
        vkDestroySwapchainKHR(app->device, app->swapchain, nullptr);
        app->swapchain = VK_NULL_HANDLE;
        std::fprintf(stdout, "Swapchain destroyed\n");
    }

    if(app->device != VK_NULL_HANDLE){
        vkDestroyDevice(app->device, nullptr);
        app->device = VK_NULL_HANDLE;
        std::fprintf(stdout, "Device destroyed\n");
    }

    if(app->surface != VK_NULL_HANDLE){
        vkDestroySurfaceKHR(app->instance, app->surface, nullptr);
        app->surface = VK_NULL_HANDLE;
        std::fprintf(stdout, "Surface destroyed\n");
    }

    if(app->debugMessenger != VK_NULL_HANDLE){
        destroyDebugMessenger(app->instance, &app->debugMessenger);
        app->debugMessenger = VK_NULL_HANDLE;
        std::fprintf(stdout, "DebugMessenger destroyed\n");
    }

    if(app->instance != VK_NULL_HANDLE){
        vkDestroyInstance(app->instance, nullptr);
        app->instance = VK_NULL_HANDLE;
        std::fprintf(stdout, "Vulkan instance destroyed\n");
    }
}