//http://blog.csdn.net/u012552880/article/details/48040885
//http://blog.leanote.com/post/2732844039@qq.com/win10%E4%B8%8Bglfw-glew
//opengl32.lib是在你安装visual studio的时候就自带的.
#include<iostream>
#include<cstdlib>
#include<GL/glew.h>
#include<GLFW/glfw3.h>

using namespace std;
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
}

// Window dimensions
static int width, height;
static GLuint VBO, VAO;
static GLuint shaderProgram;
// Shaders
static const GLchar * vertexShaderSource = "#version 330 core\n"
"layout (location=0) in vec3 position;\n"
"void main()\n"
"{\n"
"gl_Position = vec4(position.x,position.y,position.z,1.0);\n"
"}\n";
static const GLchar* fragmentShaderSource = "#version 330 core\n"
"out vec4 color;\n"
"void main()\n"
"{\n"
"color = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n"
"}\n\0";

static bool isSupported() {
    return 1;
}
static void initGraphics() {
    glViewport(0, 0, width, height);

    // Build and compile shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    // Check for compile resault
    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        cout << "ERROR::SHADER::VERTEX:COMPILATION_FAILED\n" << infoLog << endl;
    }
    // fragmentShader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << endl;
    }
    //Link shaders
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    // Check for Linking resault
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << endl;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Set up vertex data and attribute pointers
    GLfloat vertices[] = {
        -0.5f,-0.5f,0.0f,   // Left
        0.5f,-0.5f,0.0f,    // Right
        0.0f,0.5f,0.0f      // Top
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    // Bind the VAO first, then bind and set vertex buffer(s) and attribute pointer(s)
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}
static void doGraphics() {
    // Render
    // Clear the colorbuffer
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    // Draw triangle
    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}
static void exitGraphics() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}

// return >=0, good.
// return <0, bad.
int shader_basic() {
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
