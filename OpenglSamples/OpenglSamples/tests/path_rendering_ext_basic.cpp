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

///////////////////////////////////////////////////////////////////////////////////////////////
// http://developer.download.nvidia.com/opengl/specs/GL_NV_path_rendering.txt
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
    // 0 means all bits are 0; ~0 means all bits are 1 (writeing to sbuffer, all pass.), 
    // do not to specify the bit number.
    glStencilMask(~0); 
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Use an orthographic path-to-clip-space transform to map the [0..500]x[0..400]
    // range of the star's path coordinates to the [-1..1] clip space cube
    glMatrixLoadIdentityEXT(GL_PROJECTION);
    glMatrixOrthoEXT(GL_PROJECTION, 0, 500, 0, 400, -1, 1);
    glMatrixLoadIdentityEXT(GL_MODELVIEW);

    // Stencil the path:
    // 这句话就是把内容写到Sbuffer里面. 参数什么意思?
    glStencilFillPathNV(pathObject, GL_COUNT_UP_NV, 0x1F); //fill是吧整个obj填满

    glEnable(GL_STENCIL_TEST);
    // (sbuffer & 0x1F) 和 (0 & 0x1F) 比较, 不相等通过, 因为sbuf里面的内容(heart和star)不是0, 背景是0
    glStencilFunc(GL_NOTEQUAL, 0, 0x1F);
    // keep: action to take if the stencil test fails.
    // keep: action to take if the stencil test passes, but the depth test fails.
    // zero: action to take if both the stencil and the depth test pass.
    // 最后一个zero就是说这次做完了就把sbuf清掉, 下次做其他的时候不用在clear了.
    glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
    glColor3f(1, 1, 0); // yellow
    glCoverFillPathNV(pathObject, GL_BOUNDING_BOX_NV); //画黄色的两个obj, fill是填满

    // 画两个obj外面的白色的线
    glPathParameteriNV(pathObject, GL_PATH_JOIN_STYLE_NV, GL_ROUND_NV); 
    glPathParameterfNV(pathObject, GL_PATH_STROKE_WIDTH_NV, 6.5); //线的宽度

    glStencilStrokePathNV(pathObject, 0x1, ~0); //和上面的fill对比, stroke 是把outline写进sbuf.

    glColor3f(1, 1, 1); // white
    glCoverStrokePathNV(pathObject, GL_CONVEX_HULL_NV);//还是和上面fill对比.

    // 画OpenGL字
    const char *word = "OpenGLXY";
    const GLsizei wordLen = (GLsizei)strlen(word);
    GLuint glyphBase = glGenPathsNV(wordLen);
    const GLfloat emScale = 2048; // match TrueType convention
    GLuint templatePathObject = ~0; // Non-existant path object
    // 在字库里面找字
    glPathGlyphsNV(glyphBase,
        GL_SYSTEM_FONT_NAME_NV, "Helvetica", GL_BOLD_BIT_NV,
        wordLen, GL_UNSIGNED_BYTE, word,
        GL_SKIP_MISSING_GLYPH_NV, templatePathObject, emScale);
    glPathGlyphsNV(glyphBase,
        GL_SYSTEM_FONT_NAME_NV, "Arial", GL_BOLD_BIT_NV,
        wordLen, GL_UNSIGNED_BYTE, word,
        GL_SKIP_MISSING_GLYPH_NV, templatePathObject, emScale);
    glPathGlyphsNV(glyphBase,
        GL_STANDARD_FONT_NAME_NV, "Sans", GL_BOLD_BIT_NV,
        wordLen, GL_UNSIGNED_BYTE, word,
        GL_USE_MISSING_GLYPH_NV, templatePathObject, emScale);

    // GLfloat xtranslate[5 + 1]; // wordLen+1
    GLfloat *xtranslate = (GLfloat*) malloc((wordLen + 1) * sizeof(GLfloat));
    if (!xtranslate) {
        printf("ERROR: xtranslate.\n");
        return;
    }

    xtranslate[0] = 0; // Initial glyph offset is zero
    glGetPathSpacingNV(GL_ACCUM_ADJACENT_PAIRS_NV,
        wordLen + 1, GL_UNSIGNED_BYTE,
        //多了, 和字符数对应就行.
        "\000\001\002\003\004\005\006\007\008\009\009", // repeat last
                                        // letter twice
        glyphBase,
        1.0f, 1.0f,
        GL_TRANSLATE_X_NV,
        xtranslate + 1);

// bug, VS下找不到这两个macro
#define GL_FONT_Y_MIN_BOUNDS_NV                             0x00020000
#define GL_FONT_Y_MAX_BOUNDS_NV                             0x00080000

    GLfloat yMinMax[2];
    glGetPathMetricRangeNV(GL_FONT_Y_MIN_BOUNDS_NV | GL_FONT_Y_MAX_BOUNDS_NV,
        glyphBase, /*count*/1,
        2 * sizeof(GLfloat),
        yMinMax);

    glMatrixLoadIdentityEXT(GL_PROJECTION);
    glMatrixOrthoEXT(GL_PROJECTION,
        0, xtranslate[wordLen], yMinMax[0], yMinMax[1],
        -1, 1);
    glMatrixLoadIdentityEXT(GL_MODELVIEW);

    glStencilFillPathInstancedNV(wordLen, GL_UNSIGNED_BYTE,
        "\000\001\002\003\004\005\006\007\008\009",
        glyphBase,
        GL_PATH_FILL_MODE_NV, 0xFF,
        GL_TRANSLATE_X_NV, xtranslate);

    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_NOTEQUAL, 0, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
    glColor3f(0.5, 0.5, 0.5); // 50% gray
    //// a linear gradient color, bug??
    //const GLfloat rgbGen[3][3] = {
    //    { 0, 0, 0 }, // red = constant zero
    //    { 0, 1, 0 }, // green = varies with y from bottom (0) to top (1)
    //    { 0, -1, 1 } // blue = varies with y from bottom (1) to top (0)
    //};
    //glPathColorGenNV(GL_PRIMARY_COLOR, GL_PATH_OBJECT_BOUNDING_BOX_NV,
    //    GL_RGB, &rgbGen[0][0]);

    glCoverFillPathInstancedNV(wordLen, GL_UNSIGNED_BYTE,
        "\000\001\002\003\004\005\006\007\008\009",
        glyphBase,
        GL_BOUNDING_BOX_OF_BOUNDING_BOXES_NV,
        GL_TRANSLATE_X_NV, xtranslate);



    if (xtranslate) {
        free(xtranslate);
        xtranslate = NULL;
    }
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
