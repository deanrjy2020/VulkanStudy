#ifndef __01HELLOTRIANGLEEXT_H__
#define __01HELLOTRIANGLEEXT_H__

#include "01HelloTriangle.h"

class HelloTriangleExt : public HelloTriangle {
public:
	void run();
private:
	bool swapChainChanged = false;
	void initWindow();
	static void onWindowResized(GLFWwindow* window, int width, int height);
	void HelloTriangleExt::recreateSwapChain();
};

#endif