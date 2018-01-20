
#include "common.h"

using namespace std;

std::vector<char> readFile(const std::string& filename) {
    // ate: Start reading at the end of the file
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    // we can use the read position to determine the size of the file and allocate a buffer
    // as reading at the end of the file
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    // seek back to the beginning of the file and read all of the bytes at once
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    std::cout << filename.c_str() << ", size: " << fileSize << std::endl;
    return buffer;
}