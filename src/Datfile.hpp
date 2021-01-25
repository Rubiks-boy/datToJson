#ifndef DATFILE
#define DATFILE

#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include "dist/json/json.h"

class Datfile
{
public:
    static bool convert(std::string filename);

private:
    static bool DatToJson(std::string filename);
    static bool JsonToDat(std::string filename);

    static void readBytes(std::ifstream &inf, char *fbuf, uint32_t filesize);
    static void swapBytes(char *fbuf, uint32_t *fbuf_e, uint32_t filesize);
    static void swapBytesChar(unsigned char *fbuf, uint32_t filesize);

    struct datfile
    {
        uint32_t fileLen;
        uint32_t numImages;
        uint32_t zeros;
        uint32_t imageOffsets;
    };

    struct Ignore_2
    {
        uint32_t const0;
        uint32_t const1;
    };

    struct Ignore_3
    {
        uint32_t const0;
        uint32_t const1;
        uint32_t const2;
    };

    struct Ignore6
    {
        uint32_t i0 = 0;
        uint32_t i1 = 0;
        uint32_t i2 = 0;
        uint32_t i3 = 0;
        uint32_t i4 = 0;
        uint32_t i5 = 0;
    };

    struct RotAndScale
    {
        float m00;
        float m01;
        float m10;
        float m11;
    };

    struct Color
    {
        uint8_t a;
        uint8_t b;
        uint8_t g;
        uint8_t r;
    };

    struct Point
    {
        float x;
        float y;
    };

    struct Shape
    {
        Point pt_a;
        Ignore6 ignore_a;
        Point pt_b;
        Ignore6 ignore_b;
        Point pt_c;
        Ignore6 ignore_c;
    };

    struct image
    {
        uint32_t len;
        Ignore_3 consts_1_0_10;
        uint32_t len2;
        Ignore_3 consts0_0; // first value in this can be 0x0 or 0x02 -- appears to be 0x02 when charId = 0x0
        Color color;
        uint32_t charId;
        Ignore_2 consts0_1;
        RotAndScale rotAndScale;
        Point initialOffset;
        uint32_t numShapes;
        uint32_t distFromLen2ToFirstPoint;
    };
};

#endif