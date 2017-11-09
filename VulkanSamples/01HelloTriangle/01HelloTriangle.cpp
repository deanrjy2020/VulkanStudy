// need to use this define with glfw3.h together, then the glfw will vulkan
// api, otherwise will load gl api.
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>

#include <iostream>
//#include <stdexcept>
//#include <functional>
#include <vector>
#include <set>
#include <algorithm>
#include <fstream>

const int WIDTH = 640;
const int HEIGHT = 480;

// NDEBUG macro is part of the C++ standard 
#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif
// more info, see VK_SDK/Config/xx.txt
// this willbe used in create instance and logival device
const std::vector<const char*> validationLayers = {
	"VK_LAYER_LUNARG_standard_validation"
};

// Vulkan does not have the concept of a "default framebuffer", hence it 
// requires an infrastructure that will own the buffers we will render to 
// before we visualize them on the screen. This infrastructure is known as 
// the swap chain and must be created explicitly in Vulkan.
// 
// The swap chain is essentially a queue of images that are waiting to be 
// presented to the screen. Our application will acquire such an image to 
// draw to it, and then return it to the queue.
//
// Not all graphics cards are capable of presenting images directly to a 
// screen for various reasons, for example because they are designed for 
// servers and don't have any display outputs. Secondly, since image 
// presentation is heavily tied into the window system and the surfaces 
// associated with windows, it is not actually part of the Vulkan core. 
// You have to enable the VK_KHR_swapchain device extension after querying 
// for its support.
//
// 1, Checking for swap chain support
// 2, Enabling the extension in the logical device creation
// 3, Querying details of swap chain support, to make sure to be be compatible
// with our window surface.	see struct SwapChainSupportDetails
// 4, Choosing the right settings for the swap chain
//		4.1, Surface format (color depth)
//		4.2, Presentation mode(conditions for "swapping" images to the screen)
//			4.2.1, VK_PRESENT_MODE_IMMEDIATE_KHR: Images submitted by your 
//			application are transferred to the screen right away, which may 
//			result in tearing.
//			4.2.2, VK_PRESENT_MODE_FIFO_KHR: The swap chain is a queue where 
//			the display takes an image from the front of the queue when the 
//			display is refreshed and the program inserts rendered images at the
//			back of the queue. If the queue is full then the program has to 
//			wait. This is most similar to vertical sync as found in modern 
//			games. The moment that the display is refreshed is known as 
//			"vertical blank". This mode is guaranteed to be available.
//			4.2.3, VK_PRESENT_MODE_FIFO_RELAXED_KHR: This mode only differs 
//			from the previous one if the application is late and the queue was
//			empty at the last vertical blank. Instead of waiting for the next 
//			vertical blank, the image is transferred right away when it finally
//			arrives. This may result in visible tearing.
//			4.2.4 VK_PRESENT_MODE_MAILBOX_KHR: This is another variation of the
//			second mode. Instead of blocking the application when the queue is 
//			full, the images that are already queued are simply replaced with 
//			the newer ones. This mode can be used to implement triple 
//			buffering, which allows you to avoid tearing with significantly 
//			less latency issues than standard vertical sync that uses double 
//			buffering.
//		4.3, Swap extent(resolution of images in swap chain)
const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// Unfortunately, because the debugCallback function is an extension function, it is not automatically loaded. We have to look up its address ourselves.
VkResult CreateDebugReportCallbackEXT(VkInstance instance,
	const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback) {
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

static std::vector<char> readFile(const std::string& filename) {
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

// It's actually possible that the queue families supporting drawing commands and the ones supporting presentation do not overlap.
struct QueueFamilyIndices {
	int graphicsFamilyIdx = -1;
	int presentFamilyIdx = -1;

	bool isComplete() {
		return graphicsFamilyIdx >= 0 && presentFamilyIdx >= 0;
	}
};

struct SwapChainSupportDetails {
	// Basic surface capabilities (min/max number of images in swap chain, min/max width and height of images)
	VkSurfaceCapabilitiesKHR capabilities;
	// Surface formats (pixel format, color space)
	std::vector<VkSurfaceFormatKHR> formats;
	// Available presentation modes
	std::vector<VkPresentModeKHR> presentModes;

	bool isComplete() {
		return !formats.empty() && !presentModes.empty();
	}
};

class HelloTriangle {
public:
	void run() {
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

private:
	GLFWwindow* window;
	VkInstance instance1;
	VkInstance instance2; // not use this one
	VkDebugReportCallbackEXT callback;
	// The window surface needs to be created right after the instance creation, 
	// because it can actually influence the physical device selection. 
	VkSurfaceKHR surface;
	// This object will be implicitly destroyed when the VkInstance is destroyed, 
	// so we won't need to do anything new in the cleanup function
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	QueueFamilyIndices indices; //prefer to do once.
	VkDevice device;
	// Device queues are implicitly cleaned up when the device is destroyed.
	// In case the queue families are the same, the two handles will most likely have the same value now.
	VkQueue graphicsQueue;
	VkQueue presentQueue;
	SwapChainSupportDetails details; //prefer to do once.
	VkSwapchainKHR swapChain;
	std::vector<VkImage> swapChainImages; // refer to the images in the swapChain, no need to cleanup.
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;

	std::vector<VkImageView> swapChainImageViews;

	void initWindow() {
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
	}
	void initVulkan() {
		createInstance();
		setupDebugCallback();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();

		createSwapChain();
		createImageViews();

		createGraphicsPipeline();
	}

	void createGraphicsPipeline() {
		// setup shaders.
		auto vertShaderCode = readFile("shaders/vert.spv");
		auto fragShaderCode = readFile("shaders/frag.spv");

		VkShaderModule vertShaderModule;
		VkShaderModule fragShaderModule;

		vertShaderModule = createShaderModule(vertShaderCode);
		fragShaderModule = createShaderModule(fragShaderCode);

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

		// Color blending, two ways
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

		// 2, ��һ�ַ���Ч��һ��, ��
		// VkPipelineColorBlendStateCreateInfo

		// Dynamic state
		// dean next



		vkDestroyShaderModule(device, fragShaderModule, nullptr);
		vkDestroyShaderModule(device, vertShaderModule, nullptr);
	}

	VkShaderModule createShaderModule(const std::vector<char>& code) {
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

	void createImageViews() {
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

	void createSwapChain() {
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
		VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

		// save them for future use.
		this->swapChainImageFormat = surfaceFormat.format;
		this->swapChainExtent = extent;

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
	}

	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
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

	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
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

	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes) {
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

	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
		if (this->details.isComplete()) {
			return this->details;
		}

		// fill // this->details.
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
		// print the capa.
		// how many images int the chain queue.
		std::cout << "min/max ImageCount: (" << details.capabilities.minImageCount << ", " <<
			details.capabilities.maxImageCount << ")" << std::endl;
		std::cout<< "minImageExtent: (" << details.capabilities.minImageExtent.width << ", " <<
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

	// need the QueueFamilyIndices to create the logical device.
	void createLogicalDevice() {
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

	void createSurface() {
		if (glfwCreateWindowSurface(instance1, window, nullptr, &surface) != VK_SUCCESS) {
			throw std::runtime_error("failed to create window surface!");
		}
	}

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
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

	void pickPhysicalDevice() {
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

	// currently, I have only one GPU supporting Vulkan. 
	bool isDeviceSuitable(VkPhysicalDevice device) {
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

	bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

		std::cout << "all the available DEVICE extensions:" << std::endl;
		for (const auto& extension : availableExtensions) {
			std::cout << "\t" <<extension.extensionName << ", " << extension.specVersion<< std::endl;
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}

	void printAllProperties(VkPhysicalDeviceProperties &deviceProperties) {
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

	void printAllFeatures(const VkPhysicalDeviceFeatures& features) {
		printf("print the VkPhysicalDeviceFeatures:\n");
		printf("\trobustBufferAccess: %s\n",features.robustBufferAccess ? "YES" : "NO");
		printf("\tfullDrawIndexUint32: %s\n",features.fullDrawIndexUint32 ? "YES" : "NO");
		printf("\timageCubeArray: %s\n",features.imageCubeArray ? "YES" : "NO");
		printf("\tindependentBlend: %s\n",features.independentBlend ? "YES" : "NO");
		printf("\tgeometryShader: %s\n",features.geometryShader ? "YES" : "NO");
		printf("\ttessellationShader: %s\n",features.tessellationShader ? "YES" : "NO");
		printf("\tsampleRateShading: %s\n",features.sampleRateShading ? "YES" : "NO");
		printf("\tdualSrcBlend: %s\n",features.dualSrcBlend ? "YES" : "NO");
		printf("\tlogicOp: %s\n",features.logicOp ? "YES" : "NO");
		printf("\tmultiDrawIndirect: %s\n",features.multiDrawIndirect ? "YES" : "NO");
		printf("\tdrawIndirectFirstInstance: %s\n",features.drawIndirectFirstInstance ? "YES" : "NO");
		printf("\tdepthClamp: %s\n",features.depthClamp ? "YES" : "NO");
		printf("\tdepthBiasClamp: %s\n",features.depthBiasClamp ? "YES" : "NO");
		printf("\tfillModeNonSolid: %s\n",features.fillModeNonSolid ? "YES" : "NO");
		printf("\tdepthBounds: %s\n",features.depthBounds ? "YES" : "NO");
		printf("\twideLines: %s\n",features.wideLines ? "YES" : "NO");
		printf("\tlargePoints: %s\n",features.largePoints ? "YES" : "NO");
		printf("\talphaToOne: %s\n",features.alphaToOne ? "YES" : "NO");
		printf("\tmultiViewport: %s\n",features.multiViewport ? "YES" : "NO");
		printf("\tsamplerAnisotropy: %s\n",features.samplerAnisotropy ? "YES" : "NO");
		printf("\ttextureCompressionETC2: %s\n",features.textureCompressionETC2 ? "YES" : "NO");
		printf("\ttextureCompressionASTC_LDR: %s\n",features.textureCompressionASTC_LDR ? "YES" : "NO");
		printf("\ttextureCompressionBC: %s\n",features.textureCompressionBC ? "YES" : "NO");
		printf("\tocclusionQueryPrecise: %s\n",features.occlusionQueryPrecise ? "YES" : "NO");
		printf("\tpipelineStatisticsQuery: %s\n",features.pipelineStatisticsQuery ? "YES" : "NO");
		printf("\tvertexPipelineStoresAndAtomics: %s\n",features.vertexPipelineStoresAndAtomics ? "YES" : "NO");
		printf("\tfragmentStoresAndAtomics: %s\n",features.fragmentStoresAndAtomics ? "YES" : "NO");
		printf("\tshaderTessellationAndGeometryPointSize: %s\n",features.shaderTessellationAndGeometryPointSize ? "YES" : "NO");
		printf("\tshaderImageGatherExtended: %s\n",features.shaderImageGatherExtended ? "YES" : "NO");
		printf("\tshaderStorageImageExtendedFormats: %s\n",features.shaderStorageImageExtendedFormats ? "YES" : "NO");
		printf("\tshaderStorageImageMultisample: %s\n",features.shaderStorageImageMultisample ? "YES" : "NO");
		printf("\tshaderStorageImageReadWithoutFormat: %s\n",features.shaderStorageImageReadWithoutFormat ? "YES" : "NO");
		printf("\tshaderStorageImageWriteWithoutFormat: %s\n",features.shaderStorageImageWriteWithoutFormat ? "YES" : "NO");
		printf("\tshaderUniformBufferArrayDynamicIndexing: %s\n",features.shaderUniformBufferArrayDynamicIndexing ? "YES" : "NO");
		printf("\tshaderSampledImageArrayDynamicIndexing: %s\n",features.shaderSampledImageArrayDynamicIndexing ? "YES" : "NO");
		printf("\tshaderStorageBufferArrayDynamicIndexing: %s\n",features.shaderStorageBufferArrayDynamicIndexing ? "YES" : "NO");
		printf("\tshaderStorageImageArrayDynamicIndexing: %s\n",features.shaderStorageImageArrayDynamicIndexing ? "YES" : "NO");
		printf("\tshaderClipDistance: %s\n",features.shaderClipDistance ? "YES" : "NO");
		printf("\tshaderCullDistance: %s\n",features.shaderCullDistance ? "YES" : "NO");
		printf("\tshaderFloat64: %s\n",features.shaderFloat64 ? "YES" : "NO");
		printf("\tshaderInt64: %s\n",features.shaderInt64 ? "YES" : "NO");
		printf("\tshaderInt16: %s\n",features.shaderInt16 ? "YES" : "NO");
		printf("\tshaderResourceResidency: %s\n",features.shaderResourceResidency ? "YES" : "NO");
		printf("\tshaderResourceMinLod: %s\n",features.shaderResourceMinLod ? "YES" : "NO");
		printf("\tsparseBinding: %s\n",features.sparseBinding ? "YES" : "NO");
		printf("\tsparseResidencyBuffer: %s\n",features.sparseResidencyBuffer ? "YES" : "NO");
		printf("\tsparseResidencyImage2D: %s\n",features.sparseResidencyImage2D ? "YES" : "NO");
		printf("\tsparseResidencyImage3D: %s\n",features.sparseResidencyImage3D ? "YES" : "NO");
		printf("\tsparseResidency2Samples: %s\n",features.sparseResidency2Samples ? "YES" : "NO");
		printf("\tsparseResidency4Samples: %s\n",features.sparseResidency4Samples ? "YES" : "NO");
		printf("\tsparseResidency8Samples: %s\n",features.sparseResidency8Samples ? "YES" : "NO");
		printf("\tsparseResidency16Samples: %s\n",features.sparseResidency16Samples ? "YES" : "NO");
		printf("\tsparseResidencyAliased: %s\n",features.sparseResidencyAliased ? "YES" : "NO");
		printf("\tvariableMultisampleRate: %s\n",features.variableMultisampleRate ? "YES" : "NO");
		printf("\tinheritedQueries: %s\n",features.inheritedQueries ? "YES" : "NO");
		// clang-format on
	}

	void mainLoop() {
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
		}
	}

	void cleanup() {
		for (size_t i = 0; i < swapChainImageViews.size(); i++) {
			vkDestroyImageView(device, swapChainImageViews[i], nullptr);
		}
		vkDestroySwapchainKHR(device, swapChain, nullptr);
		vkDestroyDevice(device, nullptr);
		DestroyDebugReportCallbackEXT(instance1, callback, nullptr);
		vkDestroySurfaceKHR(instance1, surface, nullptr);
		vkDestroyInstance(instance1, nullptr);
		vkDestroyInstance(instance2, nullptr);

		glfwDestroyWindow(window);

		glfwTerminate();
	}
	
	void createInstance() {
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

	// Each layer in the validationLayers should be supported.
	bool checkValidationLayerSupport() {
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

	// return the glfw ext + debug ext
	std::vector<const char*> getRequiredExtensions() {
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

	void setupDebugCallback() {
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

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
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
};

int main() {
	HelloTriangle app;

	try {
		app.run();
	}
	catch (const std::runtime_error& e) {
		std::cerr << e.what() << std::endl;
		//printf(e.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}