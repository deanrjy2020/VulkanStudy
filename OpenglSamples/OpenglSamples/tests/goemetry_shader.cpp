#include<iostream>
#include<cstdlib>
#include<GL/glew.h>
#include<GLFW/glfw3.h>

using namespace std;

// Window dimensions
static int width, height;
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
}

//=====================================================================================
static bool isSupported() {
    return 1;
}
static void initGraphics() {

}
static void doGraphics() {

}
static void exitGraphics() {

}

// return >=0, good.
// return <0, bad.
int geometry_shader() {
    // put the window related functions (glfw) and glew funxtions here
    // Init GLFW
    glfwInit();
    // Set all the required options for GLFW
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    // Create a GLFWwindow object
    GLFWwindow * window = glfwCreateWindow(640, 480, "Triangle", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    if (window == NULL) {
        cout << "Failed to create GLFW window!\n";
        glfwTerminate();
        return -1;
    }
    // Set the required callback functions
    glfwSetKeyCallback(window, key_callback);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        cout << "Failed to initialize GLFW!\n";
        return -1;
    }
    // Define the viewport dimensions
    glfwGetFramebufferSize(window, &width, &height);

    // Check this test is supported or not.
    if (!isSupported()) {
        return -1;
    }

    // init the test.
    initGraphics();

    while (!glfwWindowShouldClose(window))
    {
        // Check if any events have been activate
        glfwPollEvents();

        // rendering
        doGraphics();

        // Swap the screen buffers
        glfwSwapBuffers(window);
        glfwSetKeyCallback(window, key_callback);
    }

    // cleanup
    exitGraphics();

    // Terminate GLFW, clearing any resources allocated by GLFW.
    glfwDestroyWindow(window);
    glfwTerminate();
    exit(EXIT_SUCCESS);
    return 0;
}
