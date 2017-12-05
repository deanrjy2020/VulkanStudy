#include "01HelloTriangle.h"

//#include <vulkan/vulkan.h>

#include <iostream>
#include <fstream>
#include <algorithm>
#include <set>

// Unfortunately, because the debugCallback function is an extension function, it is not automatically loaded. We have to look up its address ourselves.
VkResult CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback) {
	auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pCallback);
	} else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
	if (func != nullptr) {
		func(instance, callback, pAllocator);
	}
}

void HelloTriangle::run() {
	initWindow();
	initVulkan();
	mainLoop();
	cleanup();
}

void HelloTriangle::initWindow() {
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
}

void HelloTriangle::initVulkan() {
	createInstance();
	setupDebugCallback();
	createSurface();
	pickPhysicalDevice();
	createLogicalDevice();

	createSwapChain();
	createImageViews();

	createRenderPass();
	createGraphicsPipeline();
	createFramebuffers();

	createCommandPool();
	createCommandBuffers();

	createSemaphores();
}

void HelloTriangle::mainLoop() {
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		drawFrame();
	}

	// wait for the logical device to finish operations before exiting mainLoop and destroying the window.
	vkDeviceWaitIdle(device);
}

// the disadvantage of this approach is that we need to stop all rendering before creating the new swap chain. 
// It is possible to create a new swap chain while drawing commands on an image from the old swap chain are still in-flight. 
// You need to pass the previous swap chain to the oldSwapChain field in the VkSwapchainCreateInfoKHR struct and
// destroy the old swap chain as soon as you've finished using it.
void HelloTriangle::cleanupSwapChain() {
	for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
		vkDestroyFramebuffer(device, swapChainFramebuffers[i], nullptr);
	}

	// free the cmd buffer, reuse the pool.
	vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());

	vkDestroyPipeline(device, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyRenderPass(device, renderPass, nullptr);

	for (size_t i = 0; i < swapChainImageViews.size(); i++) {
		vkDestroyImageView(device, swapChainImageViews[i], nullptr);
	}

	vkDestroySwapchainKHR(device, swapChain, nullptr);
}

void HelloTriangle::cleanup() {
	cleanupSwapChain();
	vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
	vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
	vkDestroyCommandPool(device, commandPool, nullptr);
	vkDestroyDevice(device, nullptr);
	DestroyDebugReportCallbackEXT(instance1, callback, nullptr);
	vkDestroySurfaceKHR(instance1, surface, nullptr);
	vkDestroyInstance(instance1, nullptr);
	vkDestroyInstance(instance2, nullptr);

	glfwDestroyWindow(window);

	glfwTerminate();
}

void HelloTriangle::createInstance() {
	// Check the layer we want is supported.
	if (enableValidationLayers && !checkValidationLayerSupport()) {
		throw std::runtime_error("validation layers requested, but not available!");
	}

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Hello Triangle";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	// create instance1 by glfwExtensions
	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	auto glfwExtensions = getRequiredExtensions();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(glfwExtensions.size());
	createInfo.ppEnabledExtensionNames = glfwExtensions.data();
	if (enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	} else {
		createInfo.enabledLayerCount = 0;
	}

	if (vkCreateInstance(&createInfo, nullptr, &instance1) != VK_SUCCESS) {
		throw std::runtime_error("failed to create instance1!");
	}

	{
		// Enumerate all the extension
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
		std::cout << "all available INSTANCE extensions:" << std::endl;
		for (const auto& extension : extensions) {
			std::cout << "\t" << extension.extensionName << ", specVersion" << extension.specVersion << std::endl;
		}

		// create instance2 by vkEnumerateInstanceExtensionProperties
		std::vector<char*> extensionsName(extensionCount);
		for (uint32_t i = 0; i < extensionCount; ++i) {
			extensionsName[i] = extensions[i].extensionName;
		}

		createInfo.enabledExtensionCount = extensionCount;
		createInfo.ppEnabledExtensionNames = extensionsName.data();
		if (vkCreateInstance(&createInfo, nullptr, &instance2) != VK_SUCCESS) {
			throw std::runtime_error("failed to create instance2!");
		}
	}
}

void HelloTriangle::setupDebugCallback() {
	if (!enableValidationLayers) return;

	VkDebugReportCallbackCreateInfoEXT createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	// what kind of  meg we want to retrieve:
	// VK_DEBUG_REPORT_INFORMATION_BIT_EXT
	// VK_DEBUG_REPORT_WARNING_BIT_EXT
	// VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT
	// VK_DEBUG_REPORT_ERROR_BIT_EXT
	// VK_DEBUG_REPORT_DEBUG_BIT_EXT
	createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
	createInfo.pfnCallback = debugCallback;

	if (CreateDebugReportCallbackEXT(instance1, &createInfo, nullptr, &callback) != VK_SUCCESS) {
		throw std::runtime_error("failed to set up debug callback!");
	}
}

void HelloTriangle::createSurface() {
	if (glfwCreateWindowSurface(instance1, window, nullptr, &surface) != VK_SUCCESS) {
		throw std::runtime_error("failed to create window surface!");
	}
}

void HelloTriangle::pickPhysicalDevice() {
	// get the phy device count
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance1, &deviceCount, nullptr);
	if (deviceCount == 0) {
		throw std::runtime_error("failed to find GPUs with Vulkan support!");
	}
	std::cout << "instance1 deviceCount:" << deviceCount << std::endl;

	// just test for instance2
	{
		uint32_t deviceCount2 = 0;
		vkEnumeratePhysicalDevices(instance2, &deviceCount2, nullptr);
		if (deviceCount2 == 0) {
			throw std::runtime_error("failed to find GPUs with Vulkan support!");
		}
		std::cout << "instance2 deviceCount:" << deviceCount2 << std::endl;
	}

	// get all the phy devices (how many GPUs).
	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance1, &deviceCount, devices.data());

	// only pick the first suitable phy device.
	for (const auto& device : devices) {
		if (isDeviceSuitable(device)) {
			physicalDevice = device;
			break;
		}
	}
	if (physicalDevice == VK_NULL_HANDLE) {
		throw std::runtime_error("failed to find a suitable GPU!");
	}
}

// need the QueueFamilyIndices to create the logical device.
void HelloTriangle::createLogicalDevice() {
	QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<int> uniqueQueueFamilies = { indices.graphicsFamilyIdx, indices.presentFamilyIdx };

	// priorities to queues to influence the scheduling of command buffer execution using floating point numbers between 0.0 and 1.0
	float queuePriority = 1.0f;
	for (int queueFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	// currently,do not need nay features
	VkPhysicalDeviceFeatures deviceFeatures = {};

	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();
	if (enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	} else {
		createInfo.enabledLayerCount = 0;
	}

	// Logical devices don't interact directly with instances, which is why it's not included as a parameter.
	// The queues are automatically created along with the logical device
	if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
		throw std::runtime_error("failed to create logical device!");
	}

	vkGetDeviceQueue(device, indices.graphicsFamilyIdx/*which QueueFamily*/,
		0/*which queueCount in that QF*/, &graphicsQueue);
	vkGetDeviceQueue(device, indices.presentFamilyIdx, 0, &presentQueue);
}

void HelloTriangle::createSwapChain(bool redoQuery) {
	SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice, redoQuery);

	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
	VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

	// choose image number in the swap chain
	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}
	printf("swap chain image queue count = %d\n", imageCount);

	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1; // This is always 1 unless you are developing a stereoscopic 3D application.
									 // specifies what kind of operations we'll use the images in the swap chain for.
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	// we need to specify how to handle swap chain images that will be used across 
	// multiple queue families. That will be the case in our application if the 
	// graphics queue family is different from the presentation queue. 
	// We'll be drawing on the images in the swap chain from the graphics queue 
	// and then submitting them on the presentation queue.
	QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
	uint32_t queueFamilyIndices[] = { (uint32_t)indices.graphicsFamilyIdx, (uint32_t)indices.presentFamilyIdx };
	if (indices.graphicsFamilyIdx != indices.presentFamilyIdx) {
		// Images can be used across multiple queue families without explicit ownership transfers.
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	} else {
		// An image is owned by one queue family at a time and ownership must be 
		// explicitly transfered before using it in another queue family. 
		// This option offers the best performance.
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0; // Optional
		createInfo.pQueueFamilyIndices = nullptr; // Optional
	}
	// We can specify that a certain transform should be applied to images in the 
	// swap chain if it is supported (supportedTransforms in capabilities), 
	// like a 90 degree clockwise rotation or horizontal flip. 
	// To specify that you do not want any transformation, 
	// simply specify the current transformation.
	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	// The compositeAlpha field specifies if the alpha channel should be used for 
	// blending with other windows in the window system. 
	// You'll almost always want to simply ignore the alpha channel
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
		throw std::runtime_error("failed to create swap chain!");
	}

	// retrieving the image in the swap chain
	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
	swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
	// save them for future use.
	this->swapChainImageFormat = surfaceFormat.format;
	this->swapChainExtent = extent;
}

void HelloTriangle::createImageViews() {
	// An image view is sufficient to start using an image as a texture, 
	// but it's not quite ready to be used as a render target just yet (need FB).
	swapChainImageViews.resize(swapChainImages.size());

	for (size_t i = 0; i < swapChainImages.size(); i++) {
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = swapChainImages[i];
		// The viewType parameter allows you to treat images as 1D textures, 
		// 2D textures, 3D textures and cube maps.
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = swapChainImageFormat;
		// you can map all of the channels to the red channel for a monochrome 
		// texture. You can also map constant values of 0 and 1 to a channel. 
		// here we'll stick to the default mapping.
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		// used as color targets
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create image views!");
		}
	}
}

void HelloTriangle::createRenderPass() {
	VkAttachmentDescription colorAttachment = {};
	// The format of the color attachment should match the format of the swap chain images
	colorAttachment.format = swapChainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	// what to do with the data in the attachment before rendering.
	//		VK_ATTACHMENT_LOAD_OP_LOAD: Preserve the existing contents of the attachment
	//		VK_ATTACHMENT_LOAD_OP_CLEAR: Clear the values to a constant at the start
	//		VK_ATTACHMENT_LOAD_OP_DONT_CARE : Existing contents are undefined; we don't care about them
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	// what to do with the data in the attachment after rendering.
	//		VK_ATTACHMENT_STORE_OP_STORE: Rendered contents will be stored in memory and can be read later
	//		VK_ATTACHMENT_STORE_OP_DONT_CARE: Contents of the framebuffer will be undefined after the rendering operation.
	// apply to color and depth data, stencilLoadOp / stencilStoreOp apply to stencil data.
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	// specifies which layout the image will have before the render pass begins.
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	// specifies the layout to automatically transition to when the render pass finishes.
	// for presentation using the swap chain after rendering,
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// Subpasses and attachment references
	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	// The index of the attachment in this array is directly referenced from the fragment shader with the layout(location = 0) out vec4 outColor directive.
	// others:
	//		pInputAttachments: Attachments that are read from a shader
	//		pResolveAttachments: Attachments used for multisampling color attachments
	//		pDepthStencilAttachment : Attachments for depth and stencil data
	//		pPreserveAttachments : Attachments that are not used by this subpass, but for which the data must be preserved
	subpass.pColorAttachments = &colorAttachmentRef;

	// Subpass dependencies, 这里不是很懂...
	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	// Render pass
	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create render pass!");
	}
}

void HelloTriangle::createGraphicsPipeline() {
	// setup shaders.
	auto vertShaderCode = readFile("shaders/01HelloTriangleVert.spv");
	auto fragShaderCode = readFile("shaders/01HelloTriangleFrag.spv");

	VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
	VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	// That means that it's possible to combine multiple fragment shaders into 
	// a single shader module and use different entry points to differentiate 
	// between their behaviors. Here use the main.
	vertShaderStageInfo.pName = "main";
	// It allows you to specify values for shader constants. 
	// You can use a single shader module where its behavior can be configured 
	// at pipeline creation by specifying different values for the constants used in it. 
	// This is more efficient than configuring the shader using variables at render time, 
	// because the compiler can do optimizations like eliminating if statements that 
	// depend on these values.
	vertShaderStageInfo.pSpecializationInfo = nullptr;

	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	// Vertex input
	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

	// Input assembly
	// 1, what kind of geometry will be drawn from the vertices and 
	//		VK_PRIMITIVE_TOPOLOGY_POINT_LIST: points from vertices
	//		VK_PRIMITIVE_TOPOLOGY_LINE_LIST: line from every 2 vertices without reuse
	//		VK_PRIMITIVE_TOPOLOGY_LINE_STRIP : the end vertex of every line is used as start vertex for the next line
	//		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : triangle from every 3 vertices without reuse
	//		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP : the second and third vertex of every triangle are 
	//												used as first two vertices of the next triangle
	// 2, if primitive restart should be enabled
	// Normally, the vertices are loaded from the vertex buffer by index in sequential order, 
	// but with an element buffer you can specify the indices to use yourself.
	// If you set the primitiveRestartEnable member to VK_TRUE, then it's possible to break up 
	// lines and triangles in the _STRIP topology modes by using a special index of 0xFFFF or 0xFFFFFFFF.
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	// Viewports and scissors
	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swapChainExtent.width;
	viewport.height = (float)swapChainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = swapChainExtent;

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	// Rasterizer
	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	// If depthClampEnable is set to VK_TRUE, then fragments that are beyond the near 
	// and far planes are clamped to them as opposed to discarding them. 
	// This is useful in some special cases like shadow maps.
	rasterizer.depthClampEnable = VK_FALSE;
	// If rasterizerDiscardEnable is set to VK_TRUE, then geometry never passes through 
	// the rasterizer stage. This basically disables any output to the framebuffer.
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	// The polygonMode determines how fragments are generated for geometry.
	//		VK_POLYGON_MODE_FILL: fill the area of the polygon with fragments
	//		VK_POLYGON_MODE_LINE: polygon edges are drawn as lines
	//		VK_POLYGON_MODE_POINT : polygon vertices are drawn as points
	// Using any mode other than fill requires enabling a GPU feature.
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	// any line thicker than 1.0f requires you to enable the wideLines GPU feature.
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	// This is sometimes used for shadow mapping
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f; // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

	// Multisampling
	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = nullptr; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	// Depth and stencil testing
	// skip

	// Color blending, two steps
	//		1, Mix the old and new value to produce a final color
	//		2, Combine the old and new value using a bitwise operation
	// 1,
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {}; // configuration per attached framebuffer
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
	// pseudocode:
	// if (blendEnable) {
	//		finalColor.rgb = (srcColorBlendFactor * newColor.rgb) <colorBlendOp> (dstColorBlendFactor * oldColor.rgb);
	//		finalColor.a = (srcAlphaBlendFactor * newColor.a) <alphaBlendOp> (dstAlphaBlendFactor * oldColor.a);
	// } else {
	//		finalColor = newColor;
	// }
	//
	// finalColor = finalColor & colorWriteMask;
	// 
	// most common way:
	// finalColor.rgb = newAlpha * newColor + (1 - newAlpha) * oldColor;
	// finalColor.a = newAlpha.a;
	// code:
	// colorBlendAttachment.blendEnable = VK_TRUE;
	// colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	// colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	// colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	// colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	// colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	// colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	// 2, 
	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f; // Optional
	colorBlending.blendConstants[1] = 0.0f; // Optional
	colorBlending.blendConstants[2] = 0.0f; // Optional
	colorBlending.blendConstants[3] = 0.0f; // Optional

	// Dynamic state
	// A limited amount of the state that we've specified in the previous structs can actually be changed without recreating the pipeline.
	// 理解为大部分都是不能变的, 要变只能重新创建pipeline, 用这个就可以动态变
	//VkDynamicState dynamicStates[] = {
	//	VK_DYNAMIC_STATE_VIEWPORT,
	//	VK_DYNAMIC_STATE_LINE_WIDTH
	//};

	//VkPipelineDynamicStateCreateInfo dynamicState = {};
	//dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	//dynamicState.dynamicStateCount = 2;
	//dynamicState.pDynamicStates = dynamicStates;

	// Pipeline layout
	// specify uniform, push constants etc... here we use nothing, but still need to create one.
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 0; // Optional
	pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
	pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
	pipelineLayoutInfo.pPushConstantRanges = 0; // Optional

	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout!");
	}

	// final gfx pipeline.
	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr; // Optional
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = nullptr; // Optional
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0; // why it is 0?
	// Vulkan allows you to create a new graphics pipeline by deriving from an existing pipeline.
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineInfo.basePipelineIndex = -1; // Optional

	// can take multiple VkGraphicsPipelineCreateInfo objects and create multiple VkPipeline objects in a single call.
	// VK_NULL_HANDLE: A pipeline cache can be used to store and reuse data relevant to pipeline creation across multiple calls to 
	// vkCreateGraphicsPipelines and even across program executions if the cache is stored to a file. 
	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
		throw std::runtime_error("failed to create graphics pipeline!");
	}

	vkDestroyShaderModule(device, fragShaderModule, nullptr);
	vkDestroyShaderModule(device, vertShaderModule, nullptr);
}

void HelloTriangle::createFramebuffers() {
	swapChainFramebuffers.resize(swapChainImageViews.size());

	for (size_t i = 0; i < swapChainImageViews.size(); i++) {
		VkImageView attachments[] = {
			swapChainImageViews[i]
		};

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = swapChainExtent.width;
		framebufferInfo.height = swapChainExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create framebuffer!");
		}
	}
}

void HelloTriangle::createCommandPool() {
	// Each command pool can only allocate command buffers that are submitted on a single type of queue. 
	QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamilyIdx;
	// VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: Hint that command buffers are rerecorded with new commands very often (may change memory allocation behavior)
	// VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT: Allow command buffers to be rerecorded individually, without this flag they all have to be reset together
	poolInfo.flags = 0; // Optional

	if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create command pool!");
	}
}

// just record, not execute the cmd buffer.
void HelloTriangle::createCommandBuffers() {
	// Command buffers will be automatically freed when their command pool is destroyed
	commandBuffers.resize(swapChainFramebuffers.size());

	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	// specifies if the allocated command buffers are primary or secondary command buffers.
	//		VK_COMMAND_BUFFER_LEVEL_PRIMARY: Can be submitted to a queue for execution, but cannot be called from other command buffers.
	//		VK_COMMAND_BUFFER_LEVEL_SECONDARY: Cannot be submitted directly, but can be called from primary command buffers.
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

	if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate command buffers!");
	}

	// Starting command buffer recording
	for (size_t i = 0; i < commandBuffers.size(); i++) {
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		// specifies how we're going to use the command buffer
		//		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: The command buffer will be rerecorded right after executing it once.
		//		VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT: This is a secondary command buffer that will be entirely within a single render pass.
		//		VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT : The command buffer can be resubmitted while it is also already pending execution.
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		// only relevant for secondary command buffers.
		// specifies which state to inherit from the calling primary command buffers.
		beginInfo.pInheritanceInfo = nullptr; // Optional

		// If the command buffer was already recorded once, then a call to vkBeginCommandBuffer will implicitly reset it. 
		// It's not possible to append commands to a buffer at a later time.
		vkBeginCommandBuffer(commandBuffers[i], &beginInfo);

		// Starting a render pass
		VkRenderPassBeginInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.framebuffer = swapChainFramebuffers[i];
		// The render area defines where shader loads and stores will take place.
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = swapChainExtent;
		VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;

		// The final parameter controls how the drawing commands within the render pass will be provided.
		//		VK_SUBPASS_CONTENTS_INLINE: The render pass commands will be embedded in the primary 
		//			command buffer itself and no secondary command buffers will be executed.
		//		VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS: The render pass commands will be executed from secondary command buffers.
		vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		// Basic drawing commands
		vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		// vertexCount: Even though we don't have a vertex buffer, we technically still have 3 vertices to draw.
		// instanceCount: Used for instanced rendering, use 1 if you're not doing that.
		// firstVertex : Used as an offset into the vertex buffer, defines the lowest value of gl_VertexIndex.
		// firstInstance : Used as an offset for instanced rendering, defines the lowest value of gl_InstanceIndex.
		vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);

		// Finishing up
		vkCmdEndRenderPass(commandBuffers[i]);

		if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to record command buffer!");
		}
	}
}

void HelloTriangle::createSemaphores() {
	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
		vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS) {

		throw std::runtime_error("failed to create semaphores!");
	}
}
void HelloTriangle::updateAppState() {
	// do sth in CPU while the previous frame is being rendered. 
	// That way you keep both the GPU and CPU busy at all times.
}
// There are two ways of synchronizing swap chain events: fences and semaphores.
// Fences are mainly designed to synchronize your application itself with rendering operation, 
// whereas semaphores are used to synchronize operations within or across command queues. 
void HelloTriangle::drawFrame() {

	updateAppState();

	// waiting for presentation to finish before starting to draw the next frame,
	// otherwise, mem leak?
	vkQueueWaitIdle(presentQueue);

	// Acquire an image from the swap chain
	uint32_t imageIndex;
	// the swap chain from which we wish to acquire an image
	// specifies a timeout in nanoseconds for an image to become available. 
	vkAcquireNextImageKHR(device, swapChain, std::numeric_limits<uint64_t>::max(),
		imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

	// Execute the command buffer with that image as attachment in the framebuffer
	// Submitting the command buffer
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = { imageAvailableSemaphore };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	// specify which semaphores to wait on before execution begins
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	// in which stage(s) of the pipeline to wait.
	// That means that theoretically the implementation can already start executing 
	// our vertex shader and such while the image is not available yet.
	// Each entry in the waitStages array corresponds to the semaphore with the same index in pWaitSemaphores.
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

	// specify which semaphores to signal once the command buffer(s) have finished execution.
	VkSemaphore signalSemaphores[] = { renderFinishedSemaphore };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit draw command buffer!");
	}

	// Presentation
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	// specify which semaphores to wait on before presentation can happen
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapChains[] = { swapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr; // Optional

	vkQueuePresentKHR(presentQueue, &presentInfo);
}

VkShaderModule HelloTriangle::createShaderModule(const std::vector<char>& code) {
	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shader module!");
	}

	return shaderModule;
}

VkSurfaceFormatKHR HelloTriangle::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
	if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) {
		return{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	}

	for (const auto& availableFormat : availableFormats) {
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return availableFormat;
		}
	}

	return availableFormats[0];
}

VkPresentModeKHR HelloTriangle::chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes) {
	VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;

	for (const auto& availablePresentMode : availablePresentModes) {
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			return availablePresentMode;
		} else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
			bestMode = availablePresentMode;
		}
	}

	return bestMode;
}

VkExtent2D HelloTriangle::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	} else {
		// probably will not use this else
		VkExtent2D actualExtent = { WIDTH, HEIGHT };

		actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
		actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

		return actualExtent;
	}
}

SwapChainSupportDetails HelloTriangle::querySwapChainSupport(VkPhysicalDevice device, bool redoQuery) {
	if (!redoQuery && this->details.isComplete()) {
		return this->details;
	}

	// fill // this->details.
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
	// print the capa.
	// how many images int the chain queue.
	std::cout << "min/max ImageCount: (" << details.capabilities.minImageCount << ", " <<
		details.capabilities.maxImageCount << ")" << std::endl;
	std::cout << "minImageExtent: (" << details.capabilities.minImageExtent.width << ", " <<
		details.capabilities.minImageExtent.height << ")" << std::endl;
	std::cout << "maxImageExtent: (" << details.capabilities.maxImageExtent.width << ", " <<
		details.capabilities.maxImageExtent.height << ")" << std::endl;
	std::cout << "currentExtent: (" << details.capabilities.currentExtent.width << ", " <<
		details.capabilities.currentExtent.height << ")" << std::endl;
	// and other info...

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
	if (formatCount != 0) {
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
	}
	// print all the format to see
	for (const auto& availableFormat : details.formats) {
		std::cout << "format: " << availableFormat.format
			<< ", colorSpace: " << availableFormat.colorSpace << std::endl;
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
	if (presentModeCount != 0) {
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
	}
	// print all the mode to see
	for (const auto& mode : details.presentModes) {
		std::cout << "mode: " << mode << std::endl;
	}

	return this->details;
}

// currently, I have only one GPU supporting Vulkan. 
bool HelloTriangle::isDeviceSuitable(VkPhysicalDevice device) {
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
	// just print the props and features.
	{
		printf("Chose VkPhysicalDevice 0\n");
		printAllProperties(deviceProperties);
		printAllFeatures(deviceFeatures);
		fflush(stdout);
	}

	QueueFamilyIndices indices = findQueueFamilies(device);

	bool extensionsSupported = checkDeviceExtensionSupport(device);

	// to verify that swap chain support is adequate. 
	// Swap chain support is sufficient if there is at least one supported image format 
	// and one supported presentation mode given the window surface we have.
	bool swapChainAdequate = false;
	if (extensionsSupported) {
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
		swapChainAdequate = swapChainSupport.isComplete();
	}

	return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
		deviceFeatures.geometryShader &&
		indices.isComplete() && // here we require a device can represent the image.
		extensionsSupported && swapChainAdequate;  // VK_KHR_swapchain
}

bool HelloTriangle::checkDeviceExtensionSupport(VkPhysicalDevice device) {
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

	std::cout << "all the available DEVICE extensions:" << std::endl;
	for (const auto& extension : availableExtensions) {
		std::cout << "\t" << extension.extensionName << ", " << extension.specVersion << std::endl;
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

QueueFamilyIndices HelloTriangle::findQueueFamilies(VkPhysicalDevice device) {
	if (this->indices.isComplete()) {
		return this->indices;
	}

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	{
		std::cout << "queueFamilyCount: " << queueFamilyCount << std::endl;
		for (uint32_t i = 0; i < queueFamilyCount; ++i) {
			std::cout << "\t" << "queueFamilies[" << i << "].queueFlags: " << queueFamilies[i].queueFlags << std::endl;
			std::cout << "\t\t" << "VK_QUEUE_GRAPHICS_BIT: " << (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT ? "YES" : "NO") << std::endl;
			std::cout << "\t\t" << "VK_QUEUE_COMPUTE_BIT : " << (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT ? "YES" : "NO") << std::endl;
			std::cout << "\t\t" << "VK_QUEUE_TRANSFER_BIT : " << (queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT ? "YES" : "NO") << std::endl;
			std::cout << "\t\t" << "VK_QUEUE_SPARSE_BINDING_BIT : " << (queueFamilies[i].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT ? "YES" : "NO") << std::endl;

			std::cout << "\t" << "queueFamilies[" << i << "].queueCount: " << queueFamilies[i].queueCount << std::endl;
			std::cout << "\t" << "queueFamilies[" << i << "].timestampValidBits: " << queueFamilies[i].timestampValidBits << std::endl;
			std::cout << "\n" << std::endl;
		}
	}

	int i = 0;
	for (const auto& queueFamily : queueFamilies) {
		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			this->indices.graphicsFamilyIdx = i;
		}

		// look for a queue family that has the capability of presenting to our window surface.
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

		if (queueFamily.queueCount > 0 && presentSupport) {
			indices.presentFamilyIdx = i;
		}

		if (indices.isComplete()) {
			break;
		}

		i++;
	}

	// print which fimily we use.
	// for improved performance, it is better to use one if available.
	{
		std::cout << "we choose graphicsFamilyIdx: " << this->indices.graphicsFamilyIdx << std::endl;
		std::cout << "we choose presentFamilyIdx: " << this->indices.presentFamilyIdx << std::endl;
	}
	return this->indices;
}

// return the glfw ext + debug ext
std::vector<const char*> HelloTriangle::getRequiredExtensions() {
	std::vector<const char*> extensions;

	unsigned int glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	std::cout << "INSTANCE extensions required by glfwExtensions:" << std::endl;

	// The extensions specified by GLFW are always required
	for (unsigned int i = 0; i < glfwExtensionCount; i++) {
		std::cout << "\t" << glfwExtensions[i] << std::endl;
		extensions.push_back(glfwExtensions[i]);
	}

	if (enableValidationLayers) {
		extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	}

	return extensions;
}

// Each layer in the validationLayers should be supported.
bool HelloTriangle::checkValidationLayerSupport() {
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
	std::cout << "available INSTANCE Layer properties:" << std::endl;
	for (const auto& layerProperties : availableLayers) {
		std::cout << "\t" << layerProperties.layerName << std::endl;
	}

	for (const char* layerName : validationLayers) {
		bool layerFound = false;
		for (const auto& layerProperties : availableLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}
		if (!layerFound) {
			return false;
		}
	}
	return true;
}

std::vector<char> HelloTriangle::readFile(const std::string& filename) {
	// ate: Start reading at the end of the file
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		throw std::runtime_error("failed to open file!");
	}

	// we can use the read position to determine the size of the file and allocate a buffer
	// as reading at the end of the file
	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	// seek back to the beginning of the file and read all of the bytes at once
	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();

	std::cout << filename.c_str() << ", size: " << fileSize << std::endl;
	return buffer;
}

VKAPI_ATTR VkBool32 VKAPI_CALL HelloTriangle::debugCallback(
	VkDebugReportFlagsEXT flags,
	VkDebugReportObjectTypeEXT objType,  // eg. VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT
	uint64_t obj,  // eg. VkPhysicalDevice 
	size_t location,
	int32_t code,
	const char* layerPrefix,
	const char* msg,
	void* userData) {

	std::cerr << "validation layer: " << msg << std::endl;

	return VK_FALSE;
}

void HelloTriangle::printAllProperties(VkPhysicalDeviceProperties &deviceProperties) {
	printf("print the VkPhysicalDeviceProperties:\n");
	printf("	apiVersion: %u.%u.%u\n",
		VK_VERSION_MAJOR(deviceProperties.apiVersion),
		VK_VERSION_MINOR(deviceProperties.apiVersion),
		VK_VERSION_PATCH(deviceProperties.apiVersion));
	printf("	driverVersion: %u\n", deviceProperties.driverVersion);
	printf("	vendorID: 0x%x\n", deviceProperties.vendorID);
	printf("	deviceID: 0x%x\n", deviceProperties.deviceID);
	printf("	deviceName: %s\n", deviceProperties.deviceName);
	printf("	pipelineCacheUUID: %x%x%x%x-%x%x-%x%x-%x%x-%x%x%x%x%x%x\n",
		deviceProperties.pipelineCacheUUID[0],
		deviceProperties.pipelineCacheUUID[1],
		deviceProperties.pipelineCacheUUID[2],
		deviceProperties.pipelineCacheUUID[3],
		deviceProperties.pipelineCacheUUID[4],
		deviceProperties.pipelineCacheUUID[5],
		deviceProperties.pipelineCacheUUID[6],
		deviceProperties.pipelineCacheUUID[7],
		deviceProperties.pipelineCacheUUID[8],
		deviceProperties.pipelineCacheUUID[9],
		deviceProperties.pipelineCacheUUID[10],
		deviceProperties.pipelineCacheUUID[11],
		deviceProperties.pipelineCacheUUID[12],
		deviceProperties.pipelineCacheUUID[13],
		deviceProperties.pipelineCacheUUID[14],
		deviceProperties.pipelineCacheUUID[15]);
}

void HelloTriangle::printAllFeatures(const VkPhysicalDeviceFeatures& features) {
	printf("print the VkPhysicalDeviceFeatures:\n");
	printf("\trobustBufferAccess: %s\n", features.robustBufferAccess ? "YES" : "NO");
	printf("\tfullDrawIndexUint32: %s\n", features.fullDrawIndexUint32 ? "YES" : "NO");
	printf("\timageCubeArray: %s\n", features.imageCubeArray ? "YES" : "NO");
	printf("\tindependentBlend: %s\n", features.independentBlend ? "YES" : "NO");
	printf("\tgeometryShader: %s\n", features.geometryShader ? "YES" : "NO");
	printf("\ttessellationShader: %s\n", features.tessellationShader ? "YES" : "NO");
	printf("\tsampleRateShading: %s\n", features.sampleRateShading ? "YES" : "NO");
	printf("\tdualSrcBlend: %s\n", features.dualSrcBlend ? "YES" : "NO");
	printf("\tlogicOp: %s\n", features.logicOp ? "YES" : "NO");
	printf("\tmultiDrawIndirect: %s\n", features.multiDrawIndirect ? "YES" : "NO");
	printf("\tdrawIndirectFirstInstance: %s\n", features.drawIndirectFirstInstance ? "YES" : "NO");
	printf("\tdepthClamp: %s\n", features.depthClamp ? "YES" : "NO");
	printf("\tdepthBiasClamp: %s\n", features.depthBiasClamp ? "YES" : "NO");
	printf("\tfillModeNonSolid: %s\n", features.fillModeNonSolid ? "YES" : "NO");
	printf("\tdepthBounds: %s\n", features.depthBounds ? "YES" : "NO");
	printf("\twideLines: %s\n", features.wideLines ? "YES" : "NO");
	printf("\tlargePoints: %s\n", features.largePoints ? "YES" : "NO");
	printf("\talphaToOne: %s\n", features.alphaToOne ? "YES" : "NO");
	printf("\tmultiViewport: %s\n", features.multiViewport ? "YES" : "NO");
	printf("\tsamplerAnisotropy: %s\n", features.samplerAnisotropy ? "YES" : "NO");
	printf("\ttextureCompressionETC2: %s\n", features.textureCompressionETC2 ? "YES" : "NO");
	printf("\ttextureCompressionASTC_LDR: %s\n", features.textureCompressionASTC_LDR ? "YES" : "NO");
	printf("\ttextureCompressionBC: %s\n", features.textureCompressionBC ? "YES" : "NO");
	printf("\tocclusionQueryPrecise: %s\n", features.occlusionQueryPrecise ? "YES" : "NO");
	printf("\tpipelineStatisticsQuery: %s\n", features.pipelineStatisticsQuery ? "YES" : "NO");
	printf("\tvertexPipelineStoresAndAtomics: %s\n", features.vertexPipelineStoresAndAtomics ? "YES" : "NO");
	printf("\tfragmentStoresAndAtomics: %s\n", features.fragmentStoresAndAtomics ? "YES" : "NO");
	printf("\tshaderTessellationAndGeometryPointSize: %s\n", features.shaderTessellationAndGeometryPointSize ? "YES" : "NO");
	printf("\tshaderImageGatherExtended: %s\n", features.shaderImageGatherExtended ? "YES" : "NO");
	printf("\tshaderStorageImageExtendedFormats: %s\n", features.shaderStorageImageExtendedFormats ? "YES" : "NO");
	printf("\tshaderStorageImageMultisample: %s\n", features.shaderStorageImageMultisample ? "YES" : "NO");
	printf("\tshaderStorageImageReadWithoutFormat: %s\n", features.shaderStorageImageReadWithoutFormat ? "YES" : "NO");
	printf("\tshaderStorageImageWriteWithoutFormat: %s\n", features.shaderStorageImageWriteWithoutFormat ? "YES" : "NO");
	printf("\tshaderUniformBufferArrayDynamicIndexing: %s\n", features.shaderUniformBufferArrayDynamicIndexing ? "YES" : "NO");
	printf("\tshaderSampledImageArrayDynamicIndexing: %s\n", features.shaderSampledImageArrayDynamicIndexing ? "YES" : "NO");
	printf("\tshaderStorageBufferArrayDynamicIndexing: %s\n", features.shaderStorageBufferArrayDynamicIndexing ? "YES" : "NO");
	printf("\tshaderStorageImageArrayDynamicIndexing: %s\n", features.shaderStorageImageArrayDynamicIndexing ? "YES" : "NO");
	printf("\tshaderClipDistance: %s\n", features.shaderClipDistance ? "YES" : "NO");
	printf("\tshaderCullDistance: %s\n", features.shaderCullDistance ? "YES" : "NO");
	printf("\tshaderFloat64: %s\n", features.shaderFloat64 ? "YES" : "NO");
	printf("\tshaderInt64: %s\n", features.shaderInt64 ? "YES" : "NO");
	printf("\tshaderInt16: %s\n", features.shaderInt16 ? "YES" : "NO");
	printf("\tshaderResourceResidency: %s\n", features.shaderResourceResidency ? "YES" : "NO");
	printf("\tshaderResourceMinLod: %s\n", features.shaderResourceMinLod ? "YES" : "NO");
	printf("\tsparseBinding: %s\n", features.sparseBinding ? "YES" : "NO");
	printf("\tsparseResidencyBuffer: %s\n", features.sparseResidencyBuffer ? "YES" : "NO");
	printf("\tsparseResidencyImage2D: %s\n", features.sparseResidencyImage2D ? "YES" : "NO");
	printf("\tsparseResidencyImage3D: %s\n", features.sparseResidencyImage3D ? "YES" : "NO");
	printf("\tsparseResidency2Samples: %s\n", features.sparseResidency2Samples ? "YES" : "NO");
	printf("\tsparseResidency4Samples: %s\n", features.sparseResidency4Samples ? "YES" : "NO");
	printf("\tsparseResidency8Samples: %s\n", features.sparseResidency8Samples ? "YES" : "NO");
	printf("\tsparseResidency16Samples: %s\n", features.sparseResidency16Samples ? "YES" : "NO");
	printf("\tsparseResidencyAliased: %s\n", features.sparseResidencyAliased ? "YES" : "NO");
	printf("\tvariableMultisampleRate: %s\n", features.variableMultisampleRate ? "YES" : "NO");
	printf("\tinheritedQueries: %s\n", features.inheritedQueries ? "YES" : "NO");
	// clang-format on
}


//int main() {
//	HelloTriangle app;
//
//	try {
//		app.run();
//	} catch (const std::runtime_error& e) {
//		std::cerr << e.what() << std::endl;
//		//printf(e.what());
//		return EXIT_FAILURE;
//	}
//	return EXIT_SUCCESS;
//}