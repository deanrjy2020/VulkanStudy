#ifndef __01HELLOTRIANGLE_H__
#define __01HELLOTRIANGLE_H__

// need to use this define with glfw3.h together, then the glfw will vulkan
// api, otherwise will load gl api.
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

//#include <vulkan/vulkan.h>

#include <vector>

const int WIDTH = 640;
const int HEIGHT = 480;

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

// NDEBUG macro is part of the C++ standard 
#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

struct Vertex {
	glm::vec2 pos;
	glm::vec3 color;

	// A vertex binding describes at which rate to load data from memory 
	// throughout the vertices. It specifies the number of bytes between 
	// data entries and whether to move to the next data entry after each 
	// vertex or after each instance.
	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription = {};
		// The binding parameter specifies the index of the binding in the array of bindings.
		bindingDescription.binding = 0;
		// specifies the number of bytes from one entry to the next.
		bindingDescription.stride = sizeof(Vertex);
		// VK_VERTEX_INPUT_RATE_VERTEX: Move to the next data entry after each vertex
		// VK_VERTEX_INPUT_RATE_INSTANCE: Move to the next data entry after each instance
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}
};

const std::vector<Vertex> vertices = {
	{ { 0.0f, -0.5f },{ 1.0f, 0.0f, 0.0f } },
	{ { 0.5f, 0.5f },{ 0.0f, 1.0f, 0.0f } },
	{ { -0.5f, 0.5f },{ 0.0f, 0.0f, 1.0f } }
};

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
	void run();

	// to be used in the son.
protected:
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

	// one FB for each image in the swap chain.
	std::vector<VkFramebuffer> swapChainFramebuffers;

	VkRenderPass renderPass;
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;

	VkCommandPool commandPool;
	// allocates and records the commands for each swap chain image.
	std::vector<VkCommandBuffer> commandBuffers;

	// signal that an image has been acquired and is ready for rendering
	VkSemaphore imageAvailableSemaphore;
	// signal that rendering has finished and presentation can happen
	VkSemaphore renderFinishedSemaphore;

	void initWindow();
	void initVulkan();
	void mainLoop();

	void cleanupSwapChain();
	void cleanup();
	void createInstance();
	void setupDebugCallback();
	void createSurface();
	void pickPhysicalDevice();
	void createLogicalDevice();
	void createSwapChain(bool redoQuery = false);
	void createImageViews();
	void createRenderPass();
	void createGraphicsPipeline();
	void createFramebuffers();
	void createCommandPool();
	void createCommandBuffers();
	void createSemaphores();
	void updateAppState();
	void drawFrame();
	VkShaderModule createShaderModule(const std::vector<char>& code);
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, bool redoQuery = false);
	bool isDeviceSuitable(VkPhysicalDevice device);
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
	std::vector<const char*> getRequiredExtensions();
	bool checkValidationLayerSupport();
	static std::vector<char> readFile(const std::string& filename);
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugReportFlagsEXT flags,
		VkDebugReportObjectTypeEXT objType,  // eg. VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT
		uint64_t obj,  // eg. VkPhysicalDevice 
		size_t location,
		int32_t code,
		const char* layerPrefix,
		const char* msg,
		void* userData);
	void printAllProperties(VkPhysicalDeviceProperties &deviceProperties);
	void printAllFeatures(const VkPhysicalDeviceFeatures& features);
};
#endif