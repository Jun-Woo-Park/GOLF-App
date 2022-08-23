#ifndef LSTR_H
#define LSTR_H

#include "ncnn/net.h"
#include "YoloV5.h"


class LSTR {
public:
    LSTR(AAssetManager *mgr, const char *param, const char *bin, bool useGPU);

    ~LSTR();

    std::vector<BoxInfo> detect(JNIEnv *env, jobject image, float threshold, float nms_threshold);
    std::vector<std::string> labels{"golf-cart", "transover", "person", "tree", "t-box", "wastebasket", "stairs",
                                    "stele", "drain", "bunker", "flag", "a-piece-of-wood", "lake", "sign", "tool",
                                    "left", "right", "go-straight", "boat", "plastic-cylinder"};

private:
    static std::vector<BoxInfo>
    decode_infer(ncnn::Mat &data, const yolocv::YoloSize &frame_size, int net_size, int num_classes, float threshold);

//    static void nms(std::vector<BoxInfo>& result,float nms_threshold);
    ncnn::Net *Net;
    int input_size_w = 800 ;
    int input_size_h = 288 ;
    int num_class = 80;
public:
    static LSTR *detector;
    static bool hasGPU;
    static bool toUseGPU;
};


#endif //LSTR_H
