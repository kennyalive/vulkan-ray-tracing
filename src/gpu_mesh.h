#pragma once 

#include "vk.h"

struct GPU_Mesh {
    Vk_Buffer vertex_buffer;
    Vk_Buffer index_buffer;
    uint32_t vertex_count = 0;
    uint32_t index_count = 0;

    void destroy() {
        vertex_buffer.destroy();
        index_buffer.destroy();
        vertex_count = 0;
        index_count = 0;
    }
};
