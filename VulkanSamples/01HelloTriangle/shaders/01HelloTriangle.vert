#version 450
// The GL_ARB_separate_shader_objects extension 
// is required for Vulkan shaders to work.
#extension GL_ARB_separate_shader_objects : enable

// pass to FS, as out
layout(location = 0) out vec3 fragColor;

out gl_PerVertex {
    vec4 gl_Position;
};

vec2 positions[3] = vec2[](
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

void main() {
	// The built-in gl_VertexIndex variable contains the 
	// index of the current vertex. This is usually an index 
	// into the vertex buffer, but in our case it will be an 
	// index into a hardcoded array of vertex data.
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
	fragColor = colors[gl_VertexIndex];
}