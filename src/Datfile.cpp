#include "Datfile.hpp"

using namespace std;
namespace fs = std::filesystem;

#define add_e(x) x += (uintptr_t)datbuffer_e;
#define getImAtOffset(im_ptr, offset) (image *)(*(&im_ptr + offset) + (uintptr_t)(datbuffer_e));
#define SPACING_BETWEEN_POINTS 0x20
#define IMAGE_HEADER_CONST 0x10
#define VERBOSE_POINTS 1

const int LEN_TO_FIRST_POINT[] = {64, 76, 72, 68};

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

// i think our rgba is backwards
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
        color["red"] = imgs[i]->color.r;
        color["green"] = imgs[i]->color.g;
        color["blue"] = imgs[i]->color.b;
        color["alpha"] = imgs[i]->color.a;
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

        currImg["twiceDistBetPnts"] = imgs[i]->distFromLen2ToFirstPoint;

        uint32_t *pointsPtr = (uint32_t *)((uint64_t)(&imgs[i]->len2) + imgs[i]->distFromLen2ToFirstPoint);

        if (VERBOSE_POINTS)
        {
            Json::Value pointsJson(Json::arrayValue);

            uint32_t numPnts = (imgs[i]->len2 - imgs[i]->distFromLen2ToFirstPoint) / SPACING_BETWEEN_POINTS;

            for (uint32_t j = 0; j < numPnts; ++j)
            {
                Point *currPoint = (Point *)((uintptr_t)pointsPtr + j * SPACING_BETWEEN_POINTS);
                Json::Value currPntJson;
                currPntJson["index"] = j;
                currPntJson["x"] = currPoint->x;
                currPntJson["y"] = currPoint->y;
                pointsJson.append(currPntJson);
            }
            currImg["points"] = pointsJson;
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
    fs::path datfile(jsonfile.stem().string() + ".dat");
    
    ifstream imageJson(filename, ifstream::binary);
    Json::Value images;
    imageJson >> images;

    uint32_t numImgs = images["images"].size();

    Datfile::datfile *df = new Datfile::datfile;
    df->fileLen = 0; // may be able to calc up front based on num images or just wait until the end
    df->numImages = numImgs;
    df->zeros = 0;
    df->imageOffsets = 16 + ((numImgs -1) * 4);

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

        Json::Value jsonColor = img["Color"];
        Color rgba;
        rgba.r = jsonColor["red"].asInt();
        rgba.g = jsonColor["green"].asInt();
        rgba.b = jsonColor["blue"].asInt();
        rgba.a = jsonColor["alpha"].asInt();
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

        imgs[i]->numShapes = img["points"].size() / 3; // update this once they are grouped in 3's

        if (i == 0)
        {
            imgs[i]->distFromLen2ToFirstPoint = LEN_TO_FIRST_POINT[(df->numImages % 4) - 1];
        }
        else
        {
            imgs[i]->distFromLen2ToFirstPoint = 64;
        }

        // change to number of shapes * 3 when avail
        imgs[i]->len = (img["points"].size() * SPACING_BETWEEN_POINTS) + IMAGE_HEADER_CONST + imgs[i]->distFromLen2ToFirstPoint;
        imgs[i]->len2 = imgs[i]->len - 16;
    }

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