#include "Datfile.hpp"
#include <stdio.h>
#include <string.h>

using namespace std;
namespace fs = std::filesystem;

#define add_e(x) x += (uintptr_t)datbuffer_e;
#define getImAtOffset(im_ptr, offset) (image *)(*(&im_ptr + offset) + (uintptr_t)(datbuffer_e));
#define SPACING_BETWEEN_POINTS 0x20
#define IMAGE_HEADER_CONST 0x10
#define VERBOSE_POINTS 1

const int LEN_TO_FIRST_POINT[] = {0x40, 0x4C, 0x48, 0x44};

bool Datfile::convert(string filename)
{
    string ext = filename.substr(filename.find_last_of("."));

    if (ext == ".json")
        return JsonToDat(filename);
    else if (ext == ".dat")
        return DatToJson(filename);
    else
    {
        cout << "Please give either an .json or a .dat file to convert" << endl;
        return false;
    }
}

bool Datfile::DatToJson(string filename)
{
    ifstream inf;
    inf.open(filename);

    if (!inf)
    {
        cout << "Rip that file doesnt exist bro" << endl;
        return false;
    }

    uint32_t filesize = fs::file_size(filename);
    char *datbuffer = new char[filesize];
    uint32_t *datbuffer_e = new uint32_t[filesize / 4];
    readBytes(inf, datbuffer, filesize);
    swapBytes(datbuffer, datbuffer_e, filesize);

    // cast as a datfile with multiple images
    datfile *df = (datfile *)datbuffer_e;

    image **imgs = new image *[df->numImages];
    for (uint32_t i = 0; i < df->numImages; ++i)
    {
        imgs[i] = getImAtOffset(df->imageOffsets, i);
    }

    Json::Value root;
    Json::Value imgsJson(Json::arrayValue);

    for (uint32_t i = 0; i < df->numImages; ++i)
    {
        Json::Value currImg;
        currImg["index"] = i;

        Json::Value color;
        color["alpha"] = imgs[i]->color.a;
        color["red"] = imgs[i]->color.r;
        color["green"] = imgs[i]->color.g;
        color["blue"] = imgs[i]->color.b;
        currImg["color"] = color;

        currImg["charId"] = imgs[i]->charId;

        Json::Value scale;
        scale["rotM00"] = imgs[i]->rotAndScale.m00;
        scale["rotM01"] = imgs[i]->rotAndScale.m01;
        scale["rotM10"] = imgs[i]->rotAndScale.m10;
        scale["rotM11"] = imgs[i]->rotAndScale.m11;
        currImg["rotAndScale"] = scale;

        Json::Value offset;
        offset["x"] = imgs[i]->initialOffset.x;
        offset["y"] = imgs[i]->initialOffset.y;
        currImg["initialOffset"] = offset;

        uint32_t *pointsPtr = (uint32_t *)((uint64_t)(&imgs[i]->len2) + imgs[i]->distFromLen2ToFirstPoint);

        if (VERBOSE_POINTS)
        {
            Json::Value pointsJson(Json::arrayValue);

            uint32_t numPnts = (imgs[i]->len2 - imgs[i]->distFromLen2ToFirstPoint) / SPACING_BETWEEN_POINTS;
            uint32_t numShapes = numPnts / 3;
            
            for (uint32_t s = 0; s < numShapes; ++s)
            {
                Json::Value currShape;
                Json::Value currShapePts(Json::arrayValue);

                currShape["index"] = s;

                for (uint32_t j = 0; j < 3; ++j)
                {
                    uint32_t ptIndx = (s * 3) + j;
                    Point *currPoint = (Point *)((uintptr_t)pointsPtr + ptIndx * SPACING_BETWEEN_POINTS);
                    
                    Json::Value currPntJson;
                    currPntJson["index"] = j;
                    currPntJson["x"] = currPoint->x;
                    currPntJson["y"] = currPoint->y;
                    currShapePts.append(currPntJson);
                }

                currShape["points"] = currShapePts;
                pointsJson.append(currShape);
            }
            currImg["shapes"] = pointsJson;
        }
        else
        {
            Json::Value points;
            Point *topLeft = (Point *)((uintptr_t)pointsPtr + 1 * SPACING_BETWEEN_POINTS);
            Point *bottomRight = (Point *)((uintptr_t)pointsPtr + 4 * SPACING_BETWEEN_POINTS);
            points["x1"] = topLeft->x;
            points["y1"] = topLeft->y;
            points["x2"] = bottomRight->x;
            points["y2"] = bottomRight->y;
            currImg["points"] = points;
        }

        imgsJson.append(currImg);
    }

    root["images"] = imgsJson;

    string outfname = filename.substr(0, filename.find_last_of(".")) + ".json";
    ofstream outf(outfname);
    outf << root.toStyledString();
    outf.close();

    delete[] datbuffer;
    delete[] datbuffer_e;

    return true;
}

bool Datfile::JsonToDat(string filename)
{
    fs::path jsonfile(filename);
    fs::path datfile(jsonfile.stem().string() + "-output.dat");
    
    ifstream imageJson(filename, ifstream::binary);
    Json::Value images;
    imageJson >> images;

    uint32_t numImgs = images["images"].size();

    Datfile::datfile *df = new Datfile::datfile;
    df->fileLen = 0;
    df->numImages = numImgs;
    df->zeros = 0;
    df->imageOffsets = 16 + ((numImgs -1) * 4); // num bytes from 0x0 to length of first image

    uint32_t totalNumShapes = 0;
    vector<Shape> shapesAllImgs;
    image **imgs = new image *[df->numImages];
    for (uint32_t i = 0; i < df->numImages; ++i)
    {
        Json::Value img = images["images"][i];

        imgs[i] = new image;

        Ignore_3 consts1;
        consts1.const0 = 1;
        consts1.const1 = 0;
        consts1.const2 = 16;
        imgs[i]->consts_1_0_10 = consts1;

        Ignore_3 consts2;
        consts2.const0 = images["images"][i]["charId"].asInt() != 0 ? 0 : 2;
        consts2.const1 = 0;
        consts2.const2 = 0;
        imgs[i]->consts0_0 = consts2;

        Json::Value jsonColor = img["color"];
        Color rgba;
        rgba.a = jsonColor["alpha"].asInt();
        rgba.r = jsonColor["red"].asInt();
        rgba.g = jsonColor["green"].asInt();
        rgba.b = jsonColor["blue"].asInt();
        imgs[i]->color = rgba;

        imgs[i]->charId = img["charId"].asInt();
        imgs[i]->consts0_1 = Ignore_2 { 0, 0 };

        Json::Value jsonScale = img["rotAndScale"];
        RotAndScale scale;
        scale.m00 = jsonScale["rotM00"].asFloat();
        scale.m01 = jsonScale["rotM01"].asFloat();
        scale.m10 = jsonScale["rotM10"].asFloat();
        scale.m11 = jsonScale["rotM11"].asFloat();
        imgs[i]->rotAndScale = scale;

        Point initOffset;
        initOffset.x = img["initialOffset"]["x"].asFloat();
        initOffset.y = img["initialOffset"]["y"].asFloat();
        imgs[i]->initialOffset = initOffset;

        uint32_t numShapes = img["shapes"].size();
        totalNumShapes += numShapes;
        imgs[i]->numShapes = numShapes;

        Shape *shapes = new Shape [numShapes];
        for (uint32_t j = 0; j < numShapes; ++j)
        {
            Json::Value pnts = img["shapes"][j]["points"];
            
            if(pnts.size() != 3) {
                cout << "Shapes must have 3 points. Fix or gtfo." << endl;
                exit(42069);
            }
                
            shapes[j].pt_a.x = pnts[0]["x"].asFloat();
            shapes[j].pt_a.y = pnts[0]["y"].asFloat();
            shapes[j].pt_b.x = pnts[1]["x"].asFloat();
            shapes[j].pt_b.y = pnts[1]["y"].asFloat();
            shapes[j].pt_c.x = pnts[2]["x"].asFloat();
            shapes[j].pt_c.y = pnts[2]["y"].asFloat();

            shapes[j].ignore_a = {0}, shapes[j].ignore_b = {0}, shapes[j].ignore_c = {0};
            
            shapesAllImgs.push_back(shapes[j]);
        }

        imgs[i]->distFromLen2ToFirstPoint = i == 0 ? LEN_TO_FIRST_POINT[(df->numImages % 4) - 1] : imgs[i]->distFromLen2ToFirstPoint = 64;

        imgs[i]->len = (img["shapes"].size() * 3 * SPACING_BETWEEN_POINTS) + IMAGE_HEADER_CONST + imgs[i]->distFromLen2ToFirstPoint;
        imgs[i]->len2 = imgs[i]->len - 16;
    }

    shapesAllImgs.resize(totalNumShapes);

    df->fileLen = df->imageOffsets;
    uint32_t *offsets = new uint32_t [df->numImages];
    offsets[0] = df->imageOffsets;
    for (uint32_t k = 0; k < df->numImages; ++k)
    {
        df->fileLen += imgs[k]->len;

        if (k != 0) {
            offsets[k] = offsets[k-1] + imgs[k-1]->len;
        }
    }

    unsigned char *datfiledata = new unsigned char [df->fileLen];
    memset(datfiledata, 0, df->fileLen);
    unsigned char *dat = datfiledata;
    *(unsigned int *)dat = df->fileLen;
    dat += 4;
    *(unsigned int *)dat = df->numImages;
    dat += 8; // skip ahead 8 to include zeroes
    
    for (uint32_t l = 0; l < df->numImages; l++)
    {
        *(unsigned int *)dat = offsets[l];
        dat += 4;
    }

    uint32_t shapeIdx = 0;
    for (uint32_t m = 0; m < df->numImages; m++)
    {
        memcpy(dat, imgs[m], sizeof(*imgs[m]));
        dat += sizeof(*imgs[m]) + imgs[m]->distFromLen2ToFirstPoint - 64; // 64 is min dist from len 2 to first pt

        for (uint32_t s = 0; s < imgs[m]->numShapes; s++)
        {
            memcpy(dat, &shapesAllImgs[shapeIdx], sizeof(shapesAllImgs[shapeIdx]));
            dat += sizeof(shapesAllImgs[shapeIdx]);
            shapeIdx ++;
        }
    }

    swapBytesChar(datfiledata, df->fileLen);

    // string outfname = filename.substr(0, filename.find_last_of(".")) + ".dat";
    FILE *f = fopen(datfile.c_str(), "wb");

    if (!f)
    {
        return false;
    }

    fwrite(datfiledata, df->fileLen, 1, f);
    fclose(f);

    return true;
}

void Datfile::readBytes(ifstream &inf, char *fbuf, uint32_t filesize)
{
    inf.seekg(0, ios::beg);
    inf.read(fbuf, filesize);
    inf.close();
}

void Datfile::swapBytes(char *fbuf, uint32_t *fbuf_e, uint32_t filesize)
{
    for (uint32_t i = 0; 4 * i < filesize; ++i)
    {
        uint32_t ui = *(uint32_t *)(fbuf + i * 4);
        fbuf_e[i] = (ui >> 24) |
                    ((ui << 8) & 0x00FF0000) |
                    ((ui >> 8) & 0x0000FF00) |
                    (ui << 24);
    }
}

void Datfile::swapBytesChar(unsigned char *fbuf, uint32_t filesize)
{
    for (uint32_t i = 0; i + 3 < filesize; i += 4)
	{
		uint32_t *ui = (uint32_t *)(fbuf + i);
		*ui = (*ui >> 24) |
			  ((*ui << 8) & 0x00FF0000) |
			  ((*ui >> 8) & 0x0000FF00) |
			  (*ui << 24);
	}
}