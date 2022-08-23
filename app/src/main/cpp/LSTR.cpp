#include "LSTR.h"

bool LSTR::hasGPU = true;
bool LSTR::toUseGPU = true;
LSTR *LSTR::detector = nullptr;
std::vector<std::int16_t> culane_row_anchor = {121, 131, 141, 150, 160, 170, 180, 189, 199, 209, 219, 228, 238, 248, 258, 267, 277, 287};
std::vector<std::int16_t> tusimple_row_anchor = {128, 132, 136, 140, 144, 148, 152, 156, 160, 164, 168, 172, 176, 180, 184, 188, 192, 196, 200, 204, 208, 212, 216, 220, 224, 228, 232, 236, 240, 244, 248, 252, 256, 260, 264, 268, 272, 276, 280, 284};

LSTR::LSTR(AAssetManager *mgr, const char *param, const char *bin, bool useGPU) {
    hasGPU = ncnn::get_gpu_count() > 0;
    toUseGPU = hasGPU && useGPU;

    Net = new ncnn::Net();
    // opt 需要在加载前设置
    Net->opt.use_vulkan_compute = true;  // gpu
    Net->opt.use_fp16_arithmetic = true;  // fp16运算加速
    Net->load_param(mgr, param);
    Net->load_model(mgr, bin);
}

LSTR::~LSTR() {
    Net->clear();
    delete Net;
}

std::vector<BoxInfo> LSTR::detect(JNIEnv *env, jobject image, float threshold, float nms_threshold) {
    AndroidBitmapInfo img_size;
    AndroidBitmap_getInfo(env, image, &img_size);
    ncnn::Mat in_net = ncnn::Mat::from_android_bitmap_resize(env, image, ncnn::Mat::PIXEL_RGBA2RGB, input_size_w,
                                                             input_size_h);
    //__android_log_print(ANDROID_LOG_VERBOSE,"jun","size width : %u , height : %u",img_size.width, img_size.height);
    float mean[3] = {0.485f*255.f, 0.456f*255.f, 0.406f*255.f};//{1 / 255.f, 1 / 255.f, 1 / 255.f};
    float norm[3] = {1/0.229f/255.f, 1/0.224f/255.f, 1/0.225f/255.f};
    in_net.substract_mean_normalize(mean, norm);
    /*float test_color1 = in_net.channel(0).row(1)[1];
    float test_color2 = in_net.channel(1).row(1)[1];
    float test_color3 = in_net.channel(2).row(1)[1];
    float t=test_color1+test_color2+test_color3;
*/
    auto ex = Net->create_extractor();
    ex.set_light_mode(true);            //blob memory 할당후 해제되어 메모리 사용량 증가
    ex.set_num_threads(8);
    if (toUseGPU) {  // 消除提示
        ex.set_vulkan_compute(toUseGPU);
    }
    ex.input(0, in_net);
    std::vector<BoxInfo> result;
    ncnn::Mat out;
    ex.extract("200", out);

    auto boxes = decode_infer(out, {(int) img_size.width, (int) img_size.height}, input_size_w, num_class, threshold);
    result.insert(result.begin(), boxes.begin(), boxes.end());

/////////////////////////////    nms(result,nms_threshold);
    return result;
}

inline float fast_exp(float x) {
    union {
        uint32_t i;
        float f;
    } v{};
    v.i = (1 << 23) * (1.4426950409 * x + 126.93490512f);
    return v.f;
}

inline float sigmoid(float x) {
    return 1.0f / (1.0f + fast_exp(-x));
}

std::vector<BoxInfo>
LSTR::decode_infer(ncnn::Mat &data, const yolocv::YoloSize &frame_size, int net_size, int num_classes, float threshold) {
    std::vector<BoxInfo> result;
    //data 201,18,4  200 x轴 ，18 Y轴，4车道
    float col_sample_w = 8.070707071;
    //out_j = out_j[:, ::-1, :]
    //kjk 20211006 아래 3줄 추가
    const int griding_size = 100;
    const int row_num = 40;
    const int lane_cls_num = 2;
    const int LD_y_axis = 15; //lookahead distance y축 좌표        //LD 사용자 입력
    //kjk 20211006 위 3줄 추가
    float out[griding_size+1][row_num][lane_cls_num];
    float out1[row_num][lane_cls_num];
    float out2[row_num][lane_cls_num];

    //kjk 20211006
    //float out[201][18][4];
    //float out1[18][4];
    //float out2[18][4];
    //kjk 20211006

    //#第二个纬度 倒序
    //print("out_j.shape 1",out_j.shape)
    //沿着Z 轴 进行softmax ，每个数 乘以 【1~200]  代表着 图像X 定位的位置。
    //比如 下标 1 ，数值0.9 ，乘以 1 = X分割区域点 1 的位置概率是 0.9
    //下标100 ，数值 0.8，乘以 100 = 分割区域点 100 处，出现概率是 0.8
    //车道最终预测结果取最大，类似一个长的山峰，沿着最高点，选择高处的连线
    //prob = scipy.special.softmax(out_j[:-1, :, :], axis=0)
    //idx = np.arange(200) + 1
    //idx = idx.reshape(-1, 1, 1)
    //loc = np.sum(prob * idx, axis=0)
    float horizon_max = 0;

    int horizon_idx = griding_size;

    //kjk 20211006
    //int horizon_idx = 200;
    //kjk 20211006

    //float sum_exp[200];
    int channel = data.c;

    //kjk 20211006
//    for (int y = 0; y < 18; y++) {
//        for (int l = 0; l < 4; l++) {
//            for(int x =0;x < 201 ;x ++) {
    //kjk 20211006
    for (int y = 0; y < row_num; y++) {
        for (int l = 0; l < lane_cls_num; l++) {
            for(int x =0;x < griding_size+1 ;x ++) {

                ncnn::Mat c_data = data.channel(x);
                const float xdata = c_data.row(y)[l] ;//c_data[ 4 * y + l];

                //float xdata = data.channel(x).row(y)[l];
                //__android_log_print(ANDROID_LOG_VERBOSE,"jun","original_data[%d][%d][%d] dims : %f",x,y,l,xdata);;
                //__android_log_print(ANDROID_LOG_VERBOSE,"jun","my_data[%d][%d][%d] dims : %f",x,y,l,xx_data);;

                //int idx = l+ 18*y + 201 * x;
                //float expp = expf(xdata);
                float expp = expf(xdata);
                out[x][y][l] = expp;
                if (x!=0) {
                    out1[y][l] += expp;
                }
                else if (x == griding_size){

                }
                else {//==0
                    out1[y][l] = expp;
                }
            }
        }
    }

//kjk1020 input 찍어보기
//    for(int x =0;x < griding_size+1 ;x ++) {
//        __android_log_print(ANDROID_LOG_VERBOSE,"jun","original_data[%d][0][0] value : %f",x,out[x][0][0]);
//    }

//    for (int y =0;y < 18 ;y ++){
//        //const float *row_data= data.row(y);
//        for (int l =0;l < 4 ;l ++){
    for (int y =0;y < row_num ;y ++){
        //const float *row_data= data.row(y);
        for (int l =0;l < lane_cls_num ;l ++){
            float horizon_sum = 0;
            float horizon_max = 0;
            int horizon_max_idx = 0;
            for(int x =0;x < griding_size+1 ;x ++) {
                if (out1[y][l]!=0){
                    float o = out[x][y][l];
                    o /= out1[y][l] ;
                    if(o>horizon_max){
                        horizon_max = o;
                        horizon_max_idx = x;
                    }
                    //out2 = np.sum(prob * idx, axis=0)
                    o *=(float)x;
                    //out[x][y][l] = o;
                    /*
                     out[x][y][l] /= sum_exp ;
                    out[x][y][l] *=(float)x;
                     */
                    if (x!=0) {
                        out2[y][l] += o;
                    }
                    else if (x == griding_size){

                    }
                    else {//==0
                        out2[y][l] = o;
                    }
                    //horizon_sum +=o;
                }
            }
            //kjk 20211006
//            if(horizon_max_idx==200){
//                out2[y][l] = 0;
//            }
            //kjk 20211006

            if(horizon_max_idx==griding_size){
                out2[y][l] = 0;
            }

            /*sum_exp[x] = horizon_sum;
            if(horizon_sum > horizon_max){
                horizon_idx = x;
                horizon_max = horizon_sum;
            }*/
        }
    };
    //out_j = np.argmax(out_j, axis=0)

    //loc[out_j == cfg.griding_num] = 0
    if (horizon_idx == 2011){
        //no result

    }else{
        //#out_j (18,4) ,4 条车道，存储x 的位置[0~1]，18 是Y 的序号
        //for i in range(out_j.shape[1]):

//kjk 20211006
//        for (int l =0;l < 4 ;l ++) {
//            //#10% 左侧区域开始
//            //if np.sum(out_j[:, i] != 0) > 1:
//            float sum = 0;
//            for (int y = 0; y < 18; y++) {
//                //const float *row_data = data.row(y);
//                sum += out2[ y][ l];
//            }
//            if (sum > 2) {
//
//                //for k in range(out_j.shape[0]):
//                //if out_j[k, i] > 0:
//                for (int y = 0; y < 18; y++) {

//kjk 20211006
        for (int l =0;l < lane_cls_num ;l ++) {
            //#10% 左侧区域开始
            //if np.sum(out_j[:, i] != 0) > 1:
            float sum = 0;
            for (int y = 0; y < row_num; y++) {
                //const float *row_data = data.row(y);
                sum += out2[ y][ l];
            }
            if (sum > 2) {
                //for k in range(out_j.shape[0]):
                //if out_j[k, i] > 0:
                for (int y = 0; y < row_num; y++) {
                    if (out2[y][l] > 0) {
                        if (y > 6) {
                            BoxInfo box;
                            //ppp = (int(out_j[k, i] * col_sample_w ) - 1,
                            //int((row_anchor[cls_num_per_lane-1-k])) - 1 )
                            if (y==LD_y_axis)
                                box.label = 1002 + l;
                            else if (y == row_num - 1)
                                box.label = 1004 + l;
                            else
                                box.label = 1000 + l;
                            box.score = out2[y][l];
                            float xx = (out2[y][l] * col_sample_w);
                            //kjk 20211006
                            //float yy = culane_row_anchor[y];
                            //kjk 20211006
                            float yy = tusimple_row_anchor[y];
                            box.x1 = xx;
                            box.y1 = yy;
                            box.x2 = xx + 1;
                            box.y2 = yy + 1;
                            result.push_back(box);
                        }
                    }
                }
            }
        }
    }

    return result;
}