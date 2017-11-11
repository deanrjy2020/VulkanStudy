#include "01HelloTriangle.h"
#include "01HelloTriangleExt.h"

#include <iostream>

int main() {
	//HelloTriangle app;
	HelloTriangleExt app;

	try {
		app.run();
	} catch (const std::runtime_error& e) {
		std::cerr << e.what() << std::endl;
		//printf(e.what());
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}