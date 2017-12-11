#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void error_callback(int error, const char* description)
{
	fputs(description, stderr);
}
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);
}

// this is the old/original way to query, but if you want to use the extension function, 
// there is a lot of work (like, function point.)
// see here for the function point. https://www.opengl.org/archives/resources/features/OGLextensions/
// use glew: http://glew.sourceforge.net/basic.html
int isExtensionSupported(const char *extension)
{
    const GLubyte *extensions = NULL;
    const GLubyte *start;
    GLubyte *where, *terminator;

    /* Extension names should not have spaces. */
    where = (GLubyte *)strchr(extension, ' ');
    if (where || *extension == '\0')
        return 0;
    extensions = glGetString(GL_EXTENSIONS);
    // print all the extensions
    //printf("%s\n", Extensions);

    /* It takes a bit of care to be fool-proof about parsing the
    OpenGL extensions string. Don't be fooled by sub-strings,
    etc. */
    start = extensions;
    for (;;) {
        where = (GLubyte *)strstr((const char *)start, extension);
        if (!where)
            break;
        terminator = where + strlen(extension);
        if (where == start || *(where - 1) == ' ')
            if (*terminator == ' ' || *terminator == '\0')
                return 1;
        start = terminator;
    }
    return 0;
}

void main3(void)
{
	GLFWwindow* window;
	glfwSetErrorCallback(error_callback);
	if (!glfwInit())
		exit(EXIT_FAILURE);
	window = glfwCreateWindow(640, 480, "Simple example", NULL, NULL);
	if (!window)
	{
		glfwTerminate();
		exit(EXIT_FAILURE);
	}
	glfwMakeContextCurrent(window);
	glfwSetKeyCallback(window, key_callback);
	if (!glfwWindowShouldClose(window))
	{
		float ratio;
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		ratio = width / (float)height;
		glViewport(0, 0, width, height);

        // make sure your query string is correct, like it is GL_NV_path_rendering, not NV_path_rendering
        int pr = isExtensionSupported("GL_NV_path_rendering");
        if (!pr) {
            printf("GL_NV_path_rendering is not suppported.\n");
            return;
        }
        // ????
        // another way to check extensions, using glew
        if (!GLEW_NV_path_rendering) {
            printf("glew say: GL_NV_path_rendering is not suppported.\n");
            return;
        }
        
        // todo next, why there is runtime error: glPathStringNV
        // why glew say there is no NV_pr?
        // Getting Started with NV_path_rendering.pdf follow this in the nvogtest, and then port to VS.

        GLuint pathObj = 42;
        const char *svgPathString =
            // star
            "M100,180 L40,10 L190,120 L10,120 L160,10 z"
            // heart
            "M300 300 C 100 400,100 200,300 100,500 200,500 400,300 300Z";
        glPathStringNV(pathObj, GL_PATH_FORMAT_SVG_NV,
            (GLsizei)strlen(svgPathString), svgPathString);



		//glClear(GL_COLOR_BUFFER_BIT);
		//glMatrixMode(GL_PROJECTION);
		//glLoadIdentity();
		//glOrtho(-ratio, ratio, -1.f, 1.f, 1.f, -1.f);
		//glMatrixMode(GL_MODELVIEW);
		//glLoadIdentity();
		//glRotatef((float)glfwGetTime() * 50.f, 0.f, 0.f, 1.f);
		//glBegin(GL_TRIANGLES);
		//glColor3f(1.f, 0.f, 0.f);
		//glVertex3f(-0.6f, -0.4f, 0.f);
		//glColor3f(0.f, 1.f, 0.f);
		//glVertex3f(0.6f, -0.4f, 0.f);
		//glColor3f(0.f, 0.f, 1.f);
		//glVertex3f(0.f, 0.6f, 0.f);
		//glEnd();
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	glfwDestroyWindow(window);
	glfwTerminate();
	exit(EXIT_SUCCESS);
}
