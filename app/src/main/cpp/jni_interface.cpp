#include <jni.h>
#include <string>
#include <ncnn/gpu.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include "YoloV5.h"
#include "YoloV4.h"
#include "SimplePose.h"
#include "Yolact.h"
#include "ENet.h"
#include "FaceLandmark.h"
#include "DBFace.h"
#include "MbnFCN.h"
#include "MobileNetV3Seg.h"
#include "YoloV5CustomLayer.h"
#include "NanoDet.h"
#include "LSTR.h"
#include "yolox.h"



static int draw_unsupported(cv::Mat& rgb)
{
    const char text[] = "unsupported";

    int baseLine = 0;
    cv::Size label_size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 1.0, 1, &baseLine);

    int y = (rgb.rows - label_size.height) / 2;
    int x = (rgb.cols - label_size.width) / 2;

    cv::rectangle(rgb, cv::Rect(cv::Point(x, y), cv::Size(label_size.width, label_size.height + baseLine)),
                  cv::Scalar(255, 255, 255), -1);

    cv::putText(rgb, text, cv::Point(x, y + label_size.height),
                cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0));

    return 0;
}


JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    ncnn::create_gpu_instance();
    if (ncnn::get_gpu_count() > 0) {
        YoloV5::hasGPU = true;
        YoloV4::hasGPU = true;
        LSTR::hasGPU = true;
        SimplePose::hasGPU = true;
        Yolact::hasGPU = true;
        ENet::hasGPU = true;
        FaceLandmark::hasGPU = true;
        DBFace::hasGPU = true;
        MbnFCN::hasGPU = true;
        MBNV3Seg::hasGPU = true;
    }
//    LOGD("jni onload");
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved) {
    ncnn::destroy_gpu_instance();
    delete YoloV5::detector;
    delete YoloV4::detector;
    delete LSTR::detector;
    delete SimplePose::detector;
    delete Yolact::detector;
    delete ENet::detector;
    delete FaceLandmark::detector;
    delete DBFace::detector;
    delete MbnFCN::detector;
    delete MBNV3Seg::detector;



    
//    LOGD("jni onunload");
}

static Yolox* g_yolox = 0;
static ncnn::Mutex lock1;
// public native boolean loadModel(AssetManager mgr, int modelid, int cpugpu);
extern "C" {
JNIEXPORT jboolean JNICALL Java_com_jun_golf_NcnnYolox_loadModel(JNIEnv* env, jobject thiz, jobject assetManager, jint modelid, jint cpugpu)
{
    if (modelid < 0 || modelid > 6 || cpugpu < 0 || cpugpu > 1)
    {
        return JNI_FALSE;
    }
    AAssetManager* mgr = AAssetManager_fromJava(env, assetManager);
    const char* modeltypes[] =
            {
//                    "yolox-nano",
                    //"yolox-tiny-ori",
                    "2022-04-24-sim-opt", //최신 학습 파일
//                        "second_1",
//                    "yolox-nano-hau-ori",
                    "yolox-tiny",
            };

    const int target_sizes[] =
            {
                    416,
                    416,
            };
    const float mean_vals[][3] =
            {
                    {255.f * 0.485f, 255.f * 0.456, 255.f * 0.406f},
                    {255.f * 0.485f, 255.f * 0.456, 255.f * 0.406f},
            };
    const float norm_vals[][3] =
            {
                    {1 / (255.f * 0.229f), 1 / (255.f * 0.224f), 1 / (255.f * 0.225f)},
                    {1 / (255.f * 0.229f), 1 / (255.f * 0.224f), 1 / (255.f * 0.225f)},
            };
    const char* modeltype = modeltypes[(int)modelid];
    int target_size = target_sizes[(int)modelid];

    bool use_gpu = (int)cpugpu == 1;


    // reload
    {
        ncnn::MutexLockGuard g(lock1);

        if (use_gpu && ncnn::get_gpu_count() == 0)
        {
            // no gpu
            delete g_yolox;
            g_yolox = 0;

        }
        else
        {
            if (!g_yolox)
                g_yolox = new Yolox;
            g_yolox->load(mgr, modeltype, target_size, mean_vals[(int)modelid], norm_vals[(int)modelid], use_gpu);

        }
    }

    return JNI_TRUE;
}
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_jun_golf_NcnnYolox_detect(JNIEnv *env, jclass, jobject image, jdouble threshold, jdouble nms_threshold) {
    std::vector<Object1> objects;
    //cv::Mat rgb(0, 0, CV_8UC3);

    auto result = g_yolox->detect(env,image,objects,threshold,nms_threshold);
//    __android_log_print(ANDROID_LOG_VERBOSE,"jun", "쓰레솔드 : %d", threshold);
//
//    __android_log_print(ANDROID_LOG_VERBOSE,"jun", "%d",nms_threshold);

    auto box_cls = env->FindClass("com/jun/golf/Box");
    auto cid = env->GetMethodID(box_cls, "<init>", "(FFFFIF)V");
    jobjectArray ret = env->NewObjectArray(result.size(), box_cls, nullptr);
    int i = 0;
    for (auto &box:result) {
        env->PushLocalFrame(1);
        jobject obj = env->NewObject(box_cls, cid, box.x1, box.y1, box.x2, box.y2, box.label, box.score);
        obj = env->PopLocalFrame(obj);
        env->SetObjectArrayElement(ret, i++, obj);
    }
    return ret;
}
/*********************************************************************************************
                                         Yolov5
 ********************************************************************************************/
extern "C" JNIEXPORT void JNICALL
Java_com_jun_golf_YOLOv5_init(JNIEnv *env, jclass, jobject assetManager, jboolean useGPU) {
    if (YoloV5::detector != nullptr) {
        delete YoloV5::detector;
        YoloV5::detector = nullptr;
    }
    if (YoloV5::detector == nullptr) {
        AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
        YoloV5::detector = new YoloV5(mgr, "yolov5.param", "yolov5.bin", useGPU);
    }
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_jun_golf_YOLOv5_detect(JNIEnv *env, jclass, jobject image, jdouble threshold, jdouble nms_threshold) {
    auto result = YoloV5::detector->detect(env, image, threshold, nms_threshold);

    auto box_cls = env->FindClass("com/jun/golf/Box");
    auto cid = env->GetMethodID(box_cls, "<init>", "(FFFFIF)V");
    jobjectArray ret = env->NewObjectArray(result.size(), box_cls, nullptr);
    int i = 0;
    for (auto &box:result) {
        env->PushLocalFrame(1);
        jobject obj = env->NewObject(box_cls, cid, box.x1, box.y1, box.x2, box.y2, box.label, box.score);
        obj = env->PopLocalFrame(obj);
        env->SetObjectArrayElement(ret, i++, obj);
    }
    return ret;
}

// ***************************************[ Yolov5 Custom Layer ]****************************************
extern "C" JNIEXPORT void JNICALL
Java_com_jun_golf_YOLOv5_initCustomLayer(JNIEnv *env, jclass, jobject assetManager, jboolean useGPU) {
    if (YoloV5CustomLayer::detector != nullptr) {
        delete YoloV5CustomLayer::detector;
        YoloV5CustomLayer::detector = nullptr;
    }
    if (YoloV5CustomLayer::detector == nullptr) {
        AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
        YoloV5CustomLayer::detector = new YoloV5CustomLayer(mgr, "yolov5s_customlayer.param", "yolov5s_customlayer.bin", useGPU);
    }
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_jun_golf_YOLOv5_detectCustomLayer(JNIEnv *env, jclass, jobject image, jdouble threshold, jdouble nms_threshold) {
    auto result = YoloV5CustomLayer::detector->detect(env, image, threshold, nms_threshold);

    auto box_cls = env->FindClass("com/jun/golf/Box");
    auto cid = env->GetMethodID(box_cls, "<init>", "(FFFFIF)V");
    jobjectArray ret = env->NewObjectArray(result.size(), box_cls, nullptr);
    int i = 0;
    for (auto &box:result) {
        env->PushLocalFrame(1);
        jobject obj = env->NewObject(box_cls, cid, box.x1, box.y1, box.x2, box.y2, box.label, box.score);
        obj = env->PopLocalFrame(obj);
        env->SetObjectArrayElement(ret, i++, obj);
    }
    return ret;
}

/*********************************************************************************************
                                         YOLOv4-tiny
 yolov4官方ncnn模型下载地址
 darknet2ncnn:https://drive.google.com/drive/folders/1YzILvh0SKQPS_lrb33dmGNq7aVTKPWS0
 ********************************************************************************************/

// 20200813 增加 MobileNetV2-YOLOv3-Nano-coco
// 20201124 增加 yolo-fastest-xl

extern "C" JNIEXPORT void JNICALL
Java_com_jun_golf_YOLOv4_init(JNIEnv *env, jclass, jobject assetManager, jint yoloType, jboolean useGPU) {
    if (YoloV4::detector != nullptr) {
        delete YoloV4::detector;
        YoloV4::detector = nullptr;
    }
    if (YoloV4::detector == nullptr) {
        AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
        if (yoloType == 0) {
            YoloV4::detector = new YoloV4(mgr, "yolov4-tiny-opt.param", "yolov4-tiny-opt.bin", useGPU);
        } else if (yoloType == 2) {
            YoloV4::detector = new YoloV4(mgr, "MobileNetV2-YOLOv3-Nano-coco.param",
                                          "MobileNetV2-YOLOv3-Nano-coco.bin", useGPU);
        } else if (yoloType == 1) {
            YoloV4::detector = new YoloV4(mgr, "yolo-fastest-opt.param", "yolo-fastest-opt.bin", useGPU);
        }
    }
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_jun_golf_YOLOv4_detect(JNIEnv *env, jclass, jobject image, jdouble threshold, jdouble nms_threshold) {
    auto result = YoloV4::detector->detect(env, image, threshold, nms_threshold);

    auto box_cls = env->FindClass("com/jun/golf/Box");
    auto cid = env->GetMethodID(box_cls, "<init>", "(FFFFIF)V");
    jobjectArray ret = env->NewObjectArray(result.size(), box_cls, nullptr);
    int i = 0;
    for (auto &box:result) {
        env->PushLocalFrame(1);
        jobject obj = env->NewObject(box_cls, cid, box.x1, box.y1, box.x2, box.y2, box.label, box.score);
        obj = env->PopLocalFrame(obj);
        env->SetObjectArrayElement(ret, i++, obj);
    }
    return ret;
}


extern "C" JNIEXPORT void JNICALL
Java_com_jun_golf_LSTR_init(JNIEnv *env, jclass, jobject assetManager, jint yoloType, jboolean useGPU) {
    if (LSTR::detector != nullptr) {
        delete LSTR::detector;
        LSTR::detector = nullptr;
    }
    if (LSTR::detector == nullptr) {
        AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
        //LSTR::detector = new LSTR(mgr, "ufast_lane_det-sim-opt-fp16.param", "ufast_lane_det-sim-opt-fp16.bin", useGPU);
        LSTR::detector = new LSTR(mgr, "ufast_lane_det-sim-opt-fp16_12000_150epo.param", "ufast_lane_det-sim-opt-fp16_12000_150epo.bin", useGPU);
        //LSTR::detector = new LSTR(mgr, "LSTR-sim-opt.param", "LSTR-sim-opt.bin", false);
    }
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_jun_golf_LSTR_detect(JNIEnv *env, jclass, jobject image, jdouble threshold, jdouble nms_threshold) {
    auto result = LSTR::detector->detect(env, image, threshold, nms_threshold);

    auto box_cls = env->FindClass("com/jun/golf/Box");
    auto cid = env->GetMethodID(box_cls, "<init>", "(FFFFIF)V");
    jobjectArray ret = env->NewObjectArray(result.size(), box_cls, nullptr);
    int i = 0;
    for (auto &box:result) {
        env->PushLocalFrame(1);
        jobject obj = env->NewObject(box_cls, cid, box.x1, box.y1, box.x2, box.y2, box.label, box.score);
        obj = env->PopLocalFrame(obj);
        env->SetObjectArrayElement(ret, i++, obj);
    }
    return ret;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_jun_golf_LSTR_advancedKeyCheck(JNIEnv *env, jclass, jlong advanced_key ) {
    return true;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_jun_golf_LSTR_getAdvancedKey(JNIEnv *env, jclass, jlong idx ) {

    return 0;
}

extern "C" JNIEXPORT void JNICALL
Java_com_jun_golf_LSTR_setFastExp(JNIEnv *env, jclass, jboolean is_use_fast_exp ) {
    if(LSTR::detector!=0){
        //LSTR::detector->set_fast_exp(is_use_fast_exp);
    }
}

/*********************************************************************************************
                                         NanoDet
 ********************************************************************************************/
extern "C" JNIEXPORT void JNICALL
Java_com_jun_golf_NanoDet_init(JNIEnv *env, jclass, jobject assetManager, jboolean useGPU) {
    if (NanoDet::detector != nullptr) {
        delete NanoDet::detector;
        NanoDet::detector = nullptr;
    }
    if (NanoDet::detector == nullptr) {
        AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
        NanoDet::detector = new NanoDet(mgr, "nanodet_m.param", "nanodet_m.bin", useGPU);
    }
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_jun_golf_NanoDet_detect(JNIEnv *env, jclass, jobject image, jdouble threshold, jdouble nms_threshold) {
    auto result = NanoDet::detector->detect(env, image, threshold, nms_threshold);

    auto box_cls = env->FindClass("com/jun/golf/Box");
    auto cid = env->GetMethodID(box_cls, "<init>", "(FFFFIF)V");
    jobjectArray ret = env->NewObjectArray(result.size(), box_cls, nullptr);
    int i = 0;
    for (auto &box:result) {
        env->PushLocalFrame(1);
        jobject obj = env->NewObject(box_cls, cid, box.x1, box.y1, box.x2, box.y2, box.label, box.score);
        obj = env->PopLocalFrame(obj);
        env->SetObjectArrayElement(ret, i++, obj);
    }
    return ret;
}


/*********************************************************************************************
                                         SimplePose
 ********************************************************************************************/

extern "C" JNIEXPORT void JNICALL
Java_com_jun_golf_SimplePose_init(JNIEnv *env, jclass clazz, jobject assetManager, jboolean useGPU) {
    if (SimplePose::detector != nullptr) {
        delete SimplePose::detector;
        SimplePose::detector = nullptr;
    }
    if (SimplePose::detector == nullptr) {
        AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
        SimplePose::detector = new SimplePose(mgr, useGPU);
    }
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_jun_golf_SimplePose_detect(JNIEnv *env, jclass clazz, jobject image) {
    auto result = SimplePose::detector->detect(env, image);

    auto box_cls = env->FindClass("com/jun/golf/KeyPoint");
    auto cid = env->GetMethodID(box_cls, "<init>", "([F[FFFFFF)V");
    jobjectArray ret = env->NewObjectArray(result.size(), box_cls, nullptr);
    int i = 0;
    int KEY_NUM = 17;
    for (auto &keypoint : result) {
        env->PushLocalFrame(1);
        float x[KEY_NUM];
        float y[KEY_NUM];
        for (int j = 0; j < KEY_NUM; j++) {
            x[j] = keypoint.keyPoints[j].p.x;
            y[j] = keypoint.keyPoints[j].p.y;
        }
        jfloatArray xs = env->NewFloatArray(KEY_NUM);
        env->SetFloatArrayRegion(xs, 0, KEY_NUM, x);
        jfloatArray ys = env->NewFloatArray(KEY_NUM);
        env->SetFloatArrayRegion(ys, 0, KEY_NUM, y);

        jobject obj = env->NewObject(box_cls, cid, xs, ys,
                keypoint.boxInfos.x1, keypoint.boxInfos.y1, keypoint.boxInfos.x2, keypoint.boxInfos.y2,
                keypoint.boxInfos.score);
        obj = env->PopLocalFrame(obj);
        env->SetObjectArrayElement(ret, i++, obj);
    }
    return ret;

}

/*********************************************************************************************
                                         Yolact
 ********************************************************************************************/
jintArray matToBitmapIntArray(JNIEnv *env, const cv::Mat &image) {
    jintArray resultImage = env->NewIntArray(image.total());
    auto *_data = new jint[image.total()];
    for (int i = 0; i < image.total(); i++) {  // =========== 注意这里再确认下要不要除3
        char r = image.data[3 * i + 2];
        char g = image.data[3 * i + 1];
        char b = image.data[3 * i + 0];
        char a = (char) 255;
        _data[i] = (((jint) a << 24) & 0xFF000000) + (((jint) r << 16) & 0x00FF0000) +
                   (((jint) g << 8) & 0x0000FF00) + ((jint) b & 0x000000FF);
    }
    env->SetIntArrayRegion(resultImage, 0, image.total(), _data);
    delete[] _data;
    return resultImage;
}

jcharArray matToBitmapCharArray(JNIEnv *env, const cv::Mat &image) {
    jcharArray resultImage = env->NewCharArray(image.total());
    auto *_data = new jchar[image.total()];
    for (int i = 0; i < image.total(); i++) {
        char m = image.data[i];
        _data[i] = (m & 0xFF);
    }
    env->SetCharArrayRegion(resultImage, 0, image.total(), _data);
    delete[] _data;
    return resultImage;
}

extern "C" JNIEXPORT void JNICALL
Java_com_jun_golf_Yolact_init(JNIEnv *env, jclass clazz, jobject assetManager, jboolean useGPU) {
    if (Yolact::detector != nullptr) {
        delete Yolact::detector;
        Yolact::detector = nullptr;
    }
    if (Yolact::detector == nullptr) {
        AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
        Yolact::detector = new Yolact(mgr, useGPU);
    }
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_jun_golf_Yolact_detect(JNIEnv *env, jclass clazz, jobject image) {
    auto result = Yolact::detector->detect_yolact(env, image);

    auto yolact_mask = env->FindClass("com/jun/golf/YolactMask");
//    auto cid = env->GetMethodID(yolact_mask, "<init>", "(FFFFIF[F[I)V");
    auto cid = env->GetMethodID(yolact_mask, "<init>", "(FFFFIF[F[C)V");
    jobjectArray ret = env->NewObjectArray(result.size(), yolact_mask, nullptr);
    int i = 0;
    for (auto &mask : result) {
//        LOGD("jni yolact mask rect x:%f y:%f", mask.rect.x, mask.rect.y);
//        LOGD("jni yolact maskdata size:%d", mask.maskdata.size());
//        LOGD("jni yolact mask size:%d", mask.mask.cols * mask.mask.rows);
//        jintArray jintmask = matToBitmapIntArray(env, mask.mask);
        jcharArray jcharmask = matToBitmapCharArray(env, mask.mask);

        env->PushLocalFrame(1);
        jfloatArray maskdata = env->NewFloatArray(mask.maskdata.size());
        auto *jnum = new jfloat[mask.maskdata.size()];
        for (int j = 0; j < mask.maskdata.size(); ++j) {
            *(jnum + j) = mask.maskdata[j];
        }
        env->SetFloatArrayRegion(maskdata, 0, mask.maskdata.size(), jnum);
        delete[] jnum;

        jobject obj = env->NewObject(yolact_mask, cid,
                                     mask.rect.x, mask.rect.y, mask.rect.x + mask.rect.width,
                                     mask.rect.y + mask.rect.height,
                                     mask.label, mask.prob, maskdata, jcharmask);
        obj = env->PopLocalFrame(obj);
        env->SetObjectArrayElement(ret, i++, obj);
    }
    return ret;
}


/*********************************************************************************************
                                         chineseocr-lite
 ********************************************************************************************/
jstring str2jstring(JNIEnv *env, const char *pat) {
    //定义java String类 strClass
    jclass strClass = (env)->FindClass("java/lang/String");
    //获取String(byte[],String)的构造器,用于将本地byte[]数组转换为一个新String
    jmethodID ctorID = (env)->GetMethodID(strClass, "<init>", "([BLjava/lang/String;)V");
    //建立byte数组
    jbyteArray bytes = (env)->NewByteArray(strlen(pat));
    //将char* 转换为byte数组
    (env)->SetByteArrayRegion(bytes, 0, strlen(pat), (jbyte *) pat);
    // 设置String, 保存语言类型,用于byte数组转换至String时的参数
    jstring encoding = (env)->NewStringUTF("UTF-8");
    //将byte数组转换为java String,并输出
    return (jstring) (env)->NewObject(strClass, ctorID, bytes, encoding);
}

std::string jstring2str(JNIEnv *env, jstring jstr) {
    char *rtn = NULL;
    jclass clsstring = env->FindClass("java/lang/String");
    jstring strencode = env->NewStringUTF("UTF-8");
    jmethodID mid = env->GetMethodID(clsstring, "getBytes", "(Ljava/lang/String;)[B");
    jbyteArray barr = (jbyteArray) env->CallObjectMethod(jstr, mid, strencode);
    jsize alen = env->GetArrayLength(barr);
    jbyte *ba = env->GetByteArrayElements(barr, JNI_FALSE);
    if (alen > 0) {
        rtn = (char *) malloc(alen + 1);
        memcpy(rtn, ba, alen);
        rtn[alen] = 0;
    }
    env->ReleaseByteArrayElements(barr, ba, 0);
    std::string stemp(rtn);
    free(rtn);
    return stemp;
}


/*********************************************************************************************
                                            ENet
 ********************************************************************************************/
extern "C" JNIEXPORT void JNICALL
Java_com_jun_golf_ENet_init(JNIEnv *env, jclass, jobject assetManager, jboolean useGPU) {
    if (ENet::detector != nullptr) {
        delete ENet::detector;
        ENet::detector = nullptr;
    }
    if (ENet::detector == nullptr) {
        AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
        ENet::detector = new ENet(mgr, useGPU);
    }
}

extern "C" JNIEXPORT jfloatArray JNICALL
Java_com_jun_golf_ENet_detect(JNIEnv *env, jclass, jobject image) {
    auto result = ENet::detector->detect_enet(env, image);

    int output_w = result.w;
    int output_h = result.h;
//    LOGD("jni enet output w:%d h:%d", output_w, output_h);
    auto *output = new jfloat[output_w * output_h];
    for (int h = 0; h < output_h; h++) {
        for (int w = 0; w < output_w; w++) {
            output[h * output_w + w] = result.row(h)[w];
        }
    }
    jfloatArray jfloats = env->NewFloatArray(output_w * output_h);
    if (jfloats == nullptr) {
        return nullptr;
    }
    env->SetFloatArrayRegion(jfloats, 0, output_w * output_h, output);
    delete[] output;
    return jfloats;
}

/*********************************************************************************************
                                        MobileNetv3_Seg
 ********************************************************************************************/
extern "C" JNIEXPORT void JNICALL
Java_com_jun_golf_MbnSeg_init(JNIEnv *env, jclass, jobject assetManager, jboolean useGPU) {
    if (MBNV3Seg::detector != nullptr) {
        delete MBNV3Seg::detector;
        MBNV3Seg::detector = nullptr;
    }
    if (MBNV3Seg::detector == nullptr) {
        AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
        MBNV3Seg::detector = new MBNV3Seg(mgr, useGPU);
    }
}

extern "C" JNIEXPORT jfloatArray JNICALL
Java_com_jun_golf_MbnSeg_detect(JNIEnv *env, jclass, jobject image) {
    auto result = MBNV3Seg::detector->detect_mbnseg(env, image);

    int output_w = result.w;
    int output_h = result.h;
//    LOGD("jni mbnv3seg output w:%d h:%d", output_w, output_h);
    auto *output = new jfloat[output_w * output_h];
    for (int h = 0; h < output_h; h++) {
        for (int w = 0; w < output_w; w++) {
            output[h * output_w + w] = result.row(h)[w];
        }
    }
    jfloatArray jfloats = env->NewFloatArray(output_w * output_h);
    if (jfloats == nullptr) {
        return nullptr;
    }
    env->SetFloatArrayRegion(jfloats, 0, output_w * output_h, output);
    delete[] output;
    return jfloats;
}

/*********************************************************************************************
                                        MobileNetv2_FCN
 ********************************************************************************************/
extern "C" JNIEXPORT void JNICALL
Java_com_jun_golf_MbnFCN_init(JNIEnv *env, jclass, jobject assetManager, jboolean useGPU) {
    if (MbnFCN::detector != nullptr) {
        delete MbnFCN::detector;
        MbnFCN::detector = nullptr;
    }
    if (MbnFCN::detector == nullptr) {
        AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
        MbnFCN::detector = new MbnFCN(mgr, useGPU);
    }
}

extern "C" JNIEXPORT jfloatArray JNICALL
Java_com_jun_golf_MbnFCN_detect(JNIEnv *env, jclass, jobject image) {
    auto result = MbnFCN::detector->detect_mbnfcn(env, image);

    int output_w = result.w;
    int output_h = result.h;
//    LOGD("jni mbnfcn output w:%d h:%d", output_w, output_h);
    auto *output = new jfloat[output_w * output_h];
    for (int h = 0; h < output_h; h++) {
        for (int w = 0; w < output_w; w++) {
            output[h * output_w + w] = result.row(h)[w];
        }
    }
    jfloatArray jfloats = env->NewFloatArray(output_w * output_h);
    if (jfloats == nullptr) {
        return nullptr;
    }
    env->SetFloatArrayRegion(jfloats, 0, output_w * output_h, output);
    delete[] output;
    return jfloats;
}

/*********************************************************************************************
                                         Face_Landmark
 ********************************************************************************************/

extern "C" JNIEXPORT void JNICALL
Java_com_jun_golf_FaceLandmark_init(JNIEnv *env, jclass clazz, jobject assetManager, jboolean useGPU) {
    if (FaceLandmark::detector != nullptr) {
        delete FaceLandmark::detector;
        FaceLandmark::detector = nullptr;
    }
    if (FaceLandmark::detector == nullptr) {
        AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
        FaceLandmark::detector = new FaceLandmark(mgr, useGPU);
    }
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_jun_golf_FaceLandmark_detect(JNIEnv *env, jclass clazz, jobject image) {
    auto result = FaceLandmark::detector->detect(env, image);

    auto box_cls = env->FindClass("com/jun/golf/FaceKeyPoint");
    auto cid = env->GetMethodID(box_cls, "<init>", "(FF)V");
    jobjectArray ret = env->NewObjectArray(result.size(), box_cls, nullptr);
    int i = 0;
    for (auto &keypoint : result) {
        env->PushLocalFrame(1);
        jobject obj = env->NewObject(box_cls, cid, keypoint.p.x, keypoint.p.y);
        obj = env->PopLocalFrame(obj);
        env->SetObjectArrayElement(ret, i++, obj);
    }
    return ret;

}

/*********************************************************************************************
                                            DBFace
 ********************************************************************************************/
extern "C" JNIEXPORT void JNICALL
Java_com_jun_golf_DBFace_init(JNIEnv *env, jclass clazz, jobject assetManager, jboolean useGPU) {
    if (DBFace::detector != nullptr) {
        delete DBFace::detector;
        DBFace::detector = nullptr;
    }
    if (DBFace::detector == nullptr) {
        AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
        DBFace::detector = new DBFace(mgr, useGPU);
    }
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_jun_golf_DBFace_detect(JNIEnv *env, jclass clazz, jobject image, jdouble threshold, jdouble nms_threshold) {
    auto result = DBFace::detector->detect(env, image, threshold, nms_threshold);
//    LOGD("jni dbface size:%d %f %f", result.size(), threshold, nms_threshold);

    auto box_cls = env->FindClass("com/jun/golf/KeyPoint");
    auto cid = env->GetMethodID(box_cls, "<init>", "([F[FFFFFF)V");
    jobjectArray ret = env->NewObjectArray(result.size(), box_cls, nullptr);
    int i = 0;
    int KEY_NUM = 5;
    for (auto &keypoint : result) {
        env->PushLocalFrame(1);
        float x[KEY_NUM];
        float y[KEY_NUM];
        for (int j = 0; j < KEY_NUM; j++) {
            x[j] = keypoint.landmark.x[j];
            y[j] = keypoint.landmark.y[j];
        }
        jfloatArray xs = env->NewFloatArray(KEY_NUM);
        env->SetFloatArrayRegion(xs, 0, KEY_NUM, x);
        jfloatArray ys = env->NewFloatArray(KEY_NUM);
        env->SetFloatArrayRegion(ys, 0, KEY_NUM, y);

        jobject obj = env->NewObject(box_cls, cid, xs, ys,
                                     keypoint.box.x, keypoint.box.y, keypoint.box.r, keypoint.box.b,
                                     (float) keypoint.score);
        obj = env->PopLocalFrame(obj);
        env->SetObjectArrayElement(ret, i++, obj);
    }
    return ret;

}
