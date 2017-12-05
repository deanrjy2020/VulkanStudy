#version 450
#extension GL_ARB_separate_shader_objects : enable

// 只有三角形cover到的pixel才会每个px执行一个FS
// there is no built-in variable to output a color for the current fragment.
// layout(location = 0) modifier specifies the index of the framebuffer.
layout(location = 0) out vec4 outColor;

// receive the input from VS, as in
// 通过location连接, 名字可以不一样.
layout(location = 0) in vec3 fragColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}