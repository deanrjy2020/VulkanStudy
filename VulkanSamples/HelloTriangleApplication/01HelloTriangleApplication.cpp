#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <iostream>
//#include <stdexcept>
//#include <functional>
#include <vector>

const int WIDTH = 640;
const int HEIGHT = 480;

// NDEBUG macro is part of the C++ standard 
#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif
const std::vector<const char*> validationLayers = {
	"VK_LAYER_LUNARG_standard_validation"
};

class HelloTriangleApplication {
public:
	void run() {
		initVulkan();
		mainLoop();
		cleanup();
	}

private:
	GLFWwindow* window;
	VkInstance instance1;
	VkInstance instance2;

	void initVulkan() {
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "01Vulkan", nullptr, nullptr);

		createInstance();
	}

	void mainLoop() {
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
		}
	}

	void cleanup() {
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

		// Enumerate all the extension
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
		std::cout << "all available extensions:" << std::endl;
		for (const auto& extension : extensions) {
			std::cout << "\t" << extension.extensionName << ", specVersion" << extension.specVersion << std::endl;
		}

		// create instance2 by vkEnumerateInstanceExtensionProperties
		std::vector<char*> extensionsName(extensionCount);
		for (int i = 0; i < extensionCount; ++i) {
			extensionsName[i] = extensions[i].extensionName;
		}

		createInfo.enabledExtensionCount = extensionCount;
		createInfo.ppEnabledExtensionNames = extensionsName.data();
		if (vkCreateInstance(&createInfo, nullptr, &instance2) != VK_SUCCESS) {
			throw std::runtime_error("failed to create instance2!");
		}

		//Dean, next
		//	https ://vulkan-tutorial.com/Drawing_a_triangle/Setup/Validation_layers
		//search: Now let's see what a callback function looks like
	}

	// Each layer in the validationLayers should be supported.
	bool checkValidationLayerSupport() {
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
		std::cout << "available Layers:" << std::endl;
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
		std::cout << "glfwExtensions:" << std::endl;

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