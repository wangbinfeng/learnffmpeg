//
//  framehandlers.hpp
//  opencvdemo
//
//  Created by 王斌峰 on 2018/11/26.
//  Copyright © 2018年 Binfeng. All rights reserved.
//

#ifndef framehandlers_hpp
#define framehandlers_hpp

#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/stitching.hpp"
#include "opencv2/opencv.hpp"

#include <opencv2/face.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <zbar.h>

class FrameProcessor{
public:
    virtual void process(cv::Mat& input, cv::Mat& output) = 0;
    virtual ~FrameProcessor(){};
};

class CvFaceDetector : public FrameProcessor{
private:
    const std::string lbpfilePath = "/Users/wangbinfeng/dev/opencv/data/haarcascades_cuda/haarcascade_frontalface_alt2.xml";
    const std::string lbfmodel = "/Users/wangbinfeng/dev/opencvdemo/lbfmodel.yaml";

    cv::CascadeClassifier faceDetector;
    cv::Ptr<cv::face::Facemark> facemark;
    
    int init();
    void draw_polyline(cv::Mat &im,const std::vector<cv::Point2f> &landmarks,const int start,const int end,bool isClosed = false);
    void draw_landmarks(cv::Mat &im, std::vector<cv::Point2f> &landmarks);
    void face_alignment(cv::Mat &face, cv::Mat &result, cv::Point left, cv::Point right, cv::Rect roi);
public:
    CvFaceDetector(){
        init();
    };
    void process(cv::Mat& input, cv::Mat& output);
    ~CvFaceDetector(){};
};



class QRcodeProcessor : public FrameProcessor{
private:
    cv::QRCodeDetector qrDecoder;
    int init();
public:
    QRcodeProcessor(){
        init();
    }
    void process(cv::Mat& input, cv::Mat& output);
    ~QRcodeProcessor(){};
};

class ZbarProcessor : public FrameProcessor{
private:
    zbar::ImageScanner scanner;
    
public:
    ZbarProcessor(){
        
    }
    ~ZbarProcessor(){
        
    }
    void process(cv::Mat& input, cv::Mat& output);
};

#endif /* framehandlers_hpp */
