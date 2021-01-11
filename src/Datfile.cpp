#include "Datfile.hpp"

using namespace std;
namespace fs = std::filesystem;

#define add_e(x) x += (uintptr_t)datbuffer_e;
#define getImAtOffset(im_ptr, offset) (image *)(*(&im_ptr + offset) + (uintptr_t)(datbuffer_e));
#define SPACING_BETWEEN_POINTS 0x20

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
        currImg["colorR"] = imgs[i]->color.r;
        currImg["colorG"] = imgs[i]->color.g;
        currImg["colorB"] = imgs[i]->color.b;
        currImg["colorA"] = imgs[i]->color.a;

        currImg["charId"] = imgs[i]->charId;

        currImg["rotM00"] = imgs[i]->rotAndScale.m00;
        currImg["rotM01"] = imgs[i]->rotAndScale.m01;
        currImg["rotM10"] = imgs[i]->rotAndScale.m10;
        currImg["rotM11"] = imgs[i]->rotAndScale.m11;

        currImg["twiceDistBetPnts"] = imgs[i]->distFromLen2ToFirstPoint;

        Json::Value pointsJson(Json::arrayValue);

        uint32_t numPnts = (imgs[i]->len2 - imgs[i]->distFromLen2ToFirstPoint) / SPACING_BETWEEN_POINTS;
        uint32_t *pointsPtr = (uint32_t *)((uint64_t)(&imgs[i]->len2) + imgs[i]->distFromLen2ToFirstPoint);

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
    return false;
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