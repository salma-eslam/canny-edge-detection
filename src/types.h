#pragma once
#include <vector>
#include <cstdint>

struct Image
{
    int width;
    int height;
    std::vector<uint8_t> data;
};

struct Image16
{
    int width;
    int height;
    std::vector<int16_t> data;
};
