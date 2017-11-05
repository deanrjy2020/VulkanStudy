// need to use this define with glfw3.h together, then the glfw will vulkan api, otherwise will load gl api.
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>

#include <iostream>
//#include <stdexcept>
//#include <functional>
#include <vector>
#include <set>

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

// Vulkan does not have the concept of a "default framebuffer", hence it requires an infrastructure 
// that will own the buffers we will render to before we visualize them on the screen. 
// This infrastructure is known as the swap chain and must be created explicitly in Vulkan.
// 
// The swap chain is essentially a queue of images that are waiting to be presented to the screen.
// Our application will acquire such an image to draw to it, and then return it to the queue.
//
// Not all graphics cards are capable of presenting images directly to a screen for various reasons, 
// for example because they are designed for servers and don't have any display outputs. Secondly, 
// since image presentation is heavily tied into the window system and the surfaces associated with 
// windows, it is not actually part of the Vulkan core. You have to enable the VK_KHR_swapchain device 
// extension after querying for its support.
//
// 1, Checking for swap chain support
// 2, Enabling the extension in the logical device creation
// 3, Querying details of swap chain support, to make sure to be be compatible with our window surface.
//		see struct SwapChainSupportDetails
// 4, Choosing the right settings for the swap chain
// dean next
const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

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
};

class HelloTriangleApplication {
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
	}

	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
		SwapChainSupportDetails details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
		if (formatCount != 0) {
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
		if (presentModeCount != 0) {
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
		}

		return details;
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
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
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
	HelloTriangleApplication app;

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