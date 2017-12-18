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

static int width, height;

// looks like there is no glGen function for the path obj
GLuint pathObject = 1;

static bool isSupported() {
    // make sure your query string is correct, like it is GL_NV_path_rendering, not NV_path_rendering
    int pr = isExtensionSupported("GL_NV_path_rendering");
    if (!pr) {
        printf("GL_NV_path_rendering is not suppported.\n");
        return false;
    }
    // another way to check extensions, using glew
    if (!GLEW_NV_path_rendering) {
        printf("glew say: GL_NV_path_rendering is not suppported.\n");
        return false;
    }
    return true;
}
static void initGraphics() {
    glViewport(0, 0, width, height);
}
static void doGraphics() {

    const char *svgPathString =
        // star
        "M100,180 L40,10 L190,120 L10,120 L160,10 z"
        // heart
        "M300 300 C 100 400,100 200,300 100,500 200,500 400,300 300Z";
    glPathStringNV(pathObject, GL_PATH_FORMAT_SVG_NV,
        (GLsizei)strlen(svgPathString), svgPathString);

    // Before rendering to a window with a stencil buffer,
    // clear the stencil buffer to zero and the color buffer to black
    glClearStencil(0);
    glClearColor(0, 0, 0, 0);
    glStencilMask(~0);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Use an orthographic path-to-clip-space transform to map the [0..500]x[0..400]
    // range of the star's path coordinates to the [-1..1] clip space cube
    glMatrixLoadIdentityEXT(GL_PROJECTION);
    glMatrixOrthoEXT(GL_PROJECTION, 0, 500, 0, 400, -1, 1);
    glMatrixLoadIdentityEXT(GL_MODELVIEW);

    // Stencil the path:
    glStencilFillPathNV(pathObject, GL_COUNT_UP_NV, 0x1F);


    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_NOTEQUAL, 0, 0x1F);
    glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
    glColor3f(1, 1, 0); // yellow
    glCoverFillPathNV(pathObject, GL_BOUNDING_BOX_NV);


    glPathParameteriNV(pathObject, GL_PATH_JOIN_STYLE_NV, GL_ROUND_NV);
    glPathParameterfNV(pathObject, GL_PATH_STROKE_WIDTH_NV, 6.5);

    glStencilStrokePathNV(pathObject, 0x1, ~0);

    glColor3f(1, 1, 1); // white
    glCoverStrokePathNV(pathObject, GL_CONVEX_HULL_NV);
}
static void exitGraphics() {
    // clean up and restore the status back
    glDeletePathsNV(pathObject, 1);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 0, ~0U);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    glClearColor(0, 0, 0, 0);
}

int path_rendering_ext_basic(void)
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

    // need this first before using the glFunction/extensions.
    if (glewInit() != GLEW_OK) {
        printf("Failed to initialize GLFW!\n");
        return -1;
    }

    glfwGetFramebufferSize(window, &width, &height);

    if (!isSupported()) {
        return -1;
    }

    initGraphics();

    while (!glfwWindowShouldClose(window)) {

        // dean next:
        // Getting Started with NV_path_rendering.pdf follow this in the nvogtest, and then port to VS.
        doGraphics();
        
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

    exitGraphics();

    glfwDestroyWindow(window);
    glfwTerminate();
    exit(EXIT_SUCCESS);
    return 0;
}
