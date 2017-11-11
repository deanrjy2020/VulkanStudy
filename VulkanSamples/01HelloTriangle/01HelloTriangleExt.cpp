
#include "01HelloTriangleExt.h"

void HelloTriangleExt::run() {
	initWindow(); // son's intiWindow() must be called
	initVulkan();
	mainLoop();
	cleanup();
}

void HelloTriangleExt::initWindow() {
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

	glfwSetWindowUserPointer(window, this);
	glfwSetWindowSizeCallback(window, HelloTriangleExt::onWindowResized);
}

void HelloTriangleExt::onWindowResized(GLFWwindow* window, int width, int height) {
	if (width == 0 || height == 0) return;

	HelloTriangleExt *app = reinterpret_cast<HelloTriangleExt*>(glfwGetWindowUserPointer(window));
	app->recreateSwapChain();
}

void HelloTriangleExt::recreateSwapChain() {
	// we shouldn't touch resources that may still be in use.
	vkDeviceWaitIdle(device);

	this->swapChainChanged = true;

	cleanupSwapChain();
	createSwapChain(this->swapChainChanged);

	// The image views need to be recreated because they are based directly on the swap chain images.
	createImageViews();
	// The render pass needs to be recreated because it depends on the format of the swap chain images. 
	// It is rare for the swap chain image format to change during an operation like a window resize, 
	// but it should still be handled.
	createRenderPass();
	// Viewport and scissor rectangle size is specified during graphics pipeline creation, so the pipeline also needs to be rebuilt.
	createGraphicsPipeline();
	// the framebuffers and command buffers also directly depend on the swap chain images.
	createFramebuffers();
	createCommandBuffers();

	this->swapChainChanged = false;
}