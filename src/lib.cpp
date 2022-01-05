#include "lib.h"
#include <fstream>

std::string g_data_dir = "./data";

void error(const std::string& message) {
    printf("%s\n", message.c_str());
    throw std::runtime_error(message);
}

fs::path get_data_directory() {
	return g_data_dir;
}

std::vector<uint8_t> read_binary_file(const std::string& file_name) {
    std::ifstream file(file_name, std::ios_base::in | std::ios_base::binary);
    if (!file)
    error("failed to open file: " + file_name);

    // get file size
    file.seekg(0, std::ios_base::end);
    std::streampos file_size = file.tellg();
    file.seekg(0, std::ios_base::beg);

    if (file_size == std::streampos(-1) || !file)
    error("failed to read file stats: " + file_name);

    // read file content
    std::vector<uint8_t> file_content(static_cast<size_t>(file_size));
    file.read(reinterpret_cast<char*>(file_content.data()), file_size);
    if (!file)
    error("failed to read file content: " + file_name);

    return file_content;
}

uint64_t elapsed_milliseconds(Timestamp timestamp) {
    auto duration = std::chrono::steady_clock::now() - timestamp.t;
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    return (uint64_t)milliseconds;
}

uint64_t elapsed_nanoseconds(Timestamp timestamp) {
    auto duration = std::chrono::steady_clock::now() - timestamp.t;
    auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    return (uint64_t)nanoseconds;
}
