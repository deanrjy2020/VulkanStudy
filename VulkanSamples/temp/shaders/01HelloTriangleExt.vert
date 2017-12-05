#version 450
// The GL_ARB_separate_shader_objects extension 
// is required for Vulkan shaders to work.
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

// pass to FS, as out
layout(location = 0) out vec3 fragColor;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
	// The built-in gl_VertexIndex variable contains the 
	// index of the current vertex. This is usually an index 
	// into the vertex buffer, but in our case it will be an 
	// index into a hardcoded array of vertex data.
    gl_Position = vec4(inPosition, 0.0, 1.0);
	fragColor = inColor;
}