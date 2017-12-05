:: just double click this file and it will generates the spir-v for you.
%VULKAN_SDK%/Bin/glslangValidator.exe -V 01HelloTriangle.vert -o 01HelloTriangleVert.spv
%VULKAN_SDK%/Bin/glslangValidator.exe -V 01HelloTriangle.frag -o 01HelloTriangleFrag.spv

%VULKAN_SDK%/Bin/glslangValidator.exe -V 01HelloTriangleExt.vert -o 01HelloTriangleExtVert.spv
%VULKAN_SDK%/Bin/glslangValidator.exe -V 01HelloTriangleExt.frag -o 01HelloTriangleExtFrag.spv
pause