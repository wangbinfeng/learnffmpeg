//
//  opencvdemo.hpp
//  opencvdemo
//
//  Created by 王斌峰 on 2018/10/16.
//  Copyright © 2018年 Binfeng. All rights reserved.
//

#ifndef opencvdemo_hpp
#define opencvdemo_hpp

#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/stitching.hpp"
#include "opencv2/opencv.hpp"

using namespace cv;
using namespace std;

class ColorHistogram{
private:
    int histSize[3];
    float hranges[2];
    const float* ranges[3];
    int channels[3];
public:
    ColorHistogram(){
        histSize[0] = 256 ;
        histSize[1] = 256;
        histSize[2] = 256;
        hranges[0] = 0.0;
        hranges[1] = 256.0;
        
        ranges[0] = hranges;
        ranges[1] = hranges;
        ranges[2] = hranges;
        
        channels[0] = 0;
        channels[1] = 0;
        channels[2] = 0;
        
    }
    
    cv::Mat getHistogram(const cv::Mat& image){
        cv::Mat hist;
        hranges[0] = 0.0;
        hranges[1] = 256.0;
        channels[0] = 0;
        channels[1] = 1;
        channels[2] = 2;
        cv::calcHist(&image,1,channels,cv::Mat(),
                     hist,3,histSize,ranges);
        return hist;
        
    }
    
    cv::SparseMat getSparseHistogram(const cv::Mat& image){
        cv::SparseMat hist(3,histSize,CV_32F);
        
        hranges[0] = 0.0;
        hranges[1] = 256.0;
        channels[0] = 0;
        channels[1] = 1;
        channels[2] = 2;
        cv::calcHist(&image,1,channels,
                     cv::Mat(),//don't use mask
                     hist,//histogram
                     3,// 3 dims
                     histSize,
                     ranges//像素值的范围
                     );
        
        return hist;
        
    }
    void setSize(int size){
        histSize[0] = size ;
        histSize[1] = size;
        histSize[2] = size;
    }
    
};

class Histogram1D{
private:
    int histSize[1];
    float hranges[2];
    const float* ranges[1];
    int channels[1];
public:
    Histogram1D(){
        histSize[0] = 256;
        hranges[0] = 0.0;
        hranges[1] = 256.0;
        ranges[0] = hranges;
        channels[0] = 0;
    }
    
    cv::Mat getHistogram(const cv::Mat& image){
        cv::Mat hist;
        cv::calcHist(&image,1,channels,cv::Mat(),
                     hist,1,histSize,ranges);
        return hist;
        
    }
    
    cv::Mat getHistogramImage(const cv::Mat& image, int zoom = 1){
        cv::Mat hist = getHistogram(image);
        return getImageOfHistogram(hist, zoom);
    }
    
    cv::Mat getImageOfHistogram(const cv::Mat& hist, int zoom){
        double maxVal = 0.0;
        double minVal = 0.0;
        cv::minMaxLoc(hist,&minVal,&maxVal,0,0);
        
        int histSize = hist.rows;
        
        cv::Mat histImg(histSize * zoom, histSize * zoom, CV_8U,cv::Scalar(255));
        int hpt = static_cast<int>(0.9*histSize);
        
        for(int h =0 ;h <histSize;h++)
        {
            float binVal = hist.at<float>(h);
            if(binVal > 0){
                int intensity = static_cast<int>(binVal*hpt/maxVal);
                cv::line(histImg, cv::Point(h*zoom,histSize*zoom),cv::Point(h*zoom,(histSize - intensity)*zoom),
                         cv::Scalar(0),zoom);
            }
        }
        
        return histImg;
    }
    
    cv::Mat applyLookup(const Mat&image, const Mat& lookup){
        cv::Mat result;
        cv::LUT(image,lookup,result);
        return result;
    }
    
    cv::Mat applyLookup(const Mat& image){
        int dims(256);
        cv::Mat lut(1, &dims,CV_8U);
        for(int i=0; i<256; i++){
            lut.at<uchar>(i) = 255 - i;
        }
        
        return applyLookup(image,lut);
    }
    
    cv::Mat stretch( const Mat& image, int minValue = 0){
        cv::Mat hist = getHistogram(image);
        
        int imin = 0;
        for( ; imin < histSize[0] ; imin++ ){
            if(hist.at<float>(imin) > minValue)
                break;
        }
        
        int imax = histSize[0] - 1;
        for( ; imax > 0 ; imax--){
            if(hist.at<float>(imax) > minValue)
                break;
        }
        cout << "imin:" << imin << ", imax:" << imax << endl;
        
        int dims(256);
        
        cv::Mat lut(1, &dims,CV_8U);
        for(int i=0; i<256; i++){
            if(i < imin)
                lut.at<uchar>(i) = 0;
            else if(i > imax)
                lut.at<uchar>(i) = 255;
            else
                lut.at<uchar>(i) = cvRound(255 * (i - imin)/(imax - imin));
        }
        
        cv::Mat result = applyLookup(image, lut);
        return result;
    }
    
    
    cv::Mat detect(cv::Mat& image, cv::Mat& imageRoi){
        cv::Mat result;
        cv::Mat histoRoi = getHistogram(imageRoi);
        cv::normalize(histoRoi,histoRoi,1.0);
        cv::calcBackProject(&image, 1, channels, histoRoi, result, ranges,255.0);
        return result;
    }
};

class ImageTool{
public:
    Mat rotate(Mat &inputImg, float angle)
    {
        Mat tempImg;
        
        float radian = (float) (angle /180.0 * CV_PI);
        
        //填充图像使其符合旋转要求
        int uniSize =(int) ( max(inputImg.cols, inputImg.rows)* 1.414 );
        int dx = (int) (uniSize - inputImg.cols)/2;
        int dy = (int) (uniSize - inputImg.rows)/2;
        
        copyMakeBorder(inputImg, tempImg, dy, dy, dx, dx, BORDER_CONSTANT);
        
        
        //旋轉中心
        Point2f center( (float)(tempImg.cols/2) , (float) (tempImg.rows/2));
        Mat affine_matrix = getRotationMatrix2D( center, angle, 1.0 );
        
        //旋轉
        warpAffine(tempImg, tempImg, affine_matrix, tempImg.size());
        
        
        //旋轉后的圖像大小
        float sinVal = fabs(sin(radian));
        float cosVal = fabs(cos(radian));
        
        
        Size targetSize( (int)(inputImg.cols * cosVal + inputImg.rows * sinVal),
                        (int)(inputImg.cols * sinVal + inputImg.rows * cosVal) );
        
        //剪掉四周边框
        int x = (tempImg.cols - targetSize.width) / 2;
        int y = (tempImg.rows - targetSize.height) / 2;
        
        Rect rect(x, y, targetSize.width, targetSize.height);
        
        return Mat(tempImg, rect);
        
    }
    static void circleCrop(const cv::Mat &src,  const cv::Mat &dst,cv::Mat &result) {
        Vec3f circ(src.cols/2,src.rows/2,800); // Some dummy values for now
        Mat mask(src.rows,src.cols, CV_8UC1,Scalar(0,0,0));
        circle(mask, Point(circ[0], circ[1]), circ[2], Scalar(255), FILLED);
        seamlessClone(src, dst, mask, Point(dst.cols/2,dst.rows/2), result, NORMAL_CLONE);
        circle(result, Point(dst.cols/2,dst.rows/2), circ[2], Scalar(255,0,0),50,LINE_8 );
    }
    
    static bool saveImage(const cv::Mat &dst, const char* dstImageName2) {
        bool result = false;
        vector<int> compression_params;
        compression_params.push_back(IMWRITE_JPEG_OPTIMIZE );
        compression_params.push_back(1);
        
        
        try
        {
            result = imwrite(dstImageName2, dst, compression_params);
        }
        catch (const cv::Exception& ex)
        {
            fprintf(stderr, "Exception converting image to JPG format: %s\n", ex.what());
        }
        
        
        if (result)
            printf("Saved \n");
        else
            printf("ERROR: Can't save file.\n");
        
        return result;
    }
    
    static int loadImage(cv::Mat &src,const char* imageName ) {
        src = imread( imageName, IMREAD_COLOR );
        if( src.empty()) {
            printf(" Error opening image\n");
            printf(" Program Arguments: [image_name -- default ../data/lena.jpg] \n");
            return -1;
        }
        return 1;
    }
    
    static void addBorder(Mat &src,Mat& dst){
        
        int top, bottom, left, right;
        int borderType = BORDER_CONSTANT;
        RNG rng(12345);
        
        top = (int) (0.02*src.rows); bottom = top;
        left = (int) (0.02*src.cols); right = left;
        cout << top << "," << bottom << "," << left << "," << right << endl;
        
        Scalar value( 125, 55, rng.uniform(0, 255) );
        copyMakeBorder( src, dst, top, bottom, left, right, borderType, value );
        
    }
    
    static void addText(Mat &src,Point& org,const char* text){
        int lineType = 8;
        RNG rng(12345);
        Scalar color( 125, 55, rng.uniform(0, 255) );
        putText( src, text, org, 1,
                5, color, rng.uniform(1, 10), lineType);
    }
    
    static void stitch(vector<Mat>& srcs, Mat& dst, int row){
        int border = 10;
        if(row <= 1){
            
            int maxColSize = 0;
            int rowsSize = border;
            for(auto it = srcs.cbegin();it!=srcs.cend();++it){
                int cols = (*it).cols;
                if(maxColSize < cols)
                    maxColSize = cols;
                rowsSize += (*it).rows + border;
            }
            maxColSize += border;
            
            cout << rowsSize << ":" <<  maxColSize << endl;
            dst = Mat::zeros(rowsSize,maxColSize,CV_8UC3);
            int rows = border;
            for(auto it = srcs.cbegin();it!=srcs.cend();++it){
                Mat img = *it;
                cout << "copy to : "<<rows << "," << border << "," << img.rows << ":" <<  img.cols << endl;
                img.copyTo(dst(Rect(border,rows, img.cols,img.rows)));
                rows += img.rows + border;
            }
        }else if(row >= srcs.size()){
            int maxColSize = border;
            int maxRowSize = border;
            for(auto it = srcs.cbegin();it!=srcs.cend();++it){
                int cols = (*it).cols;
                int rows = (*it).rows;
                if(maxRowSize < rows)
                    maxRowSize = rows;
                maxColSize += cols + border;
            }
            maxRowSize += border+border;
            cout << maxRowSize << ":" <<  maxColSize << endl;
            
            dst = Mat::zeros(maxRowSize,maxColSize,CV_8UC3);
            int cols = border;
            for(auto it = srcs.cbegin();it!=srcs.cend();++it){
                Mat img = *it;
                cout << "copy to : "<<border << "," << cols << "," << img.rows << ":" <<  img.cols << endl;
                img.copyTo(dst(Rect(cols,border, img.cols, img.rows )));
                cols += img.cols + border;
            }
        }else{
            
            int maxColSize = border;
            int totalRowSize = 0;
            int i=0;
            for(i=0; i< srcs.size() ;  ){
                int colSize = 0;
                int maxRowSize = 0;
                for(int j=0; j<row && i < srcs.size(); j++)
                {
                    int cols = srcs[i].cols;
                    int rows = srcs[i].rows;
                    
                    if(maxRowSize < rows)
                        maxRowSize = rows;
                    
                    colSize += cols + border;
                    i+=1;
                }
                
                if(maxColSize < colSize)
                    maxColSize = colSize;
                
                totalRowSize += border + maxRowSize;
                
            }
            
            totalRowSize += border+border;
            
            cout << totalRowSize << ":" <<  maxColSize << endl;
            dst = Mat::zeros(totalRowSize,maxColSize,CV_8UC3);
            
            int r = border;
            i=0;
            for(i=0; i< srcs.size() ;){
                int c = border;
                int maxRows = 0;
                cout<<"i:" << i << endl;
                for(int j=0; j< row && i < srcs.size(); j++)
                {
                    cout << "copy to : "<<c << "," << r << "," << srcs[i].rows << ":" <<  srcs[i].cols << endl;
                    srcs[i].copyTo(dst(Rect(c,r,srcs[i].cols,srcs[i].rows)));
                    c += srcs[i].cols + border;
                    if(maxRows < srcs[i].rows)
                        maxRows = srcs[i].rows;
                    i += 1;
                }
                r += maxRows + border;
            }
        }
        
    }
    
    static void wave(const Mat& image, const Mat& result)
    {
        Mat srcX(image.rows,image.cols,CV_32F);
        Mat srcY(image.rows,image.cols,CV_32F);
        
        for(int i=0 ; i<image.rows; i++)
            for(int j=0; j<image.cols; j++){
                srcX.at<float>(i,j) = j;
                srcY.at<float>(i,j) = i+5*sin(j/10.0);
            }
        cv::remap(image,result,srcX,srcY, cv::INTER_LINEAR);
    }
    
    
    static void detectHScolor(Mat& image,double minHue,double maxHue,
                              double minSat,double maxSat, Mat& mask){
        Mat hsv;
        cvtColor(image,hsv,COLOR_BGR2HSV);
        vector<Mat> channels;
        split(hsv,channels);
        // channels[ 0] 是 色调
        // channels[ 1] 是 饱和度
        // channels[ 2] 是 亮度
        
        Mat mask1;
        threshold(channels[0],mask1,maxHue,255,THRESH_BINARY_INV);
        
        Mat mask2;
        threshold(channels[0],mask2,minHue,255,THRESH_BINARY);
        
        Mat hueMask;
        if(minHue < maxHue)
            hueMask = mask1 & mask2;
        else
            hueMask = mask1 | mask2;
        
        threshold(channels[1],mask1,maxSat,255,THRESH_BINARY_INV);
        threshold(channels[1],mask2,minSat,255,THRESH_BINARY);
        
        Mat satMask;
        satMask = mask1 & mask2;
        
        mask = hueMask&satMask;
        
    }
    
    static void colorReduce(Mat& image,int div)
    {
        //image.at<uchar>(i,j)：取出灰度图像中i行j列的点。
        //image.at<Vec3b>(i,j)[k]：取出彩色图像中i行j列第k通道的颜色点
        
        for(int i=0;i<image.rows;i++)
        {
            for(int j=0;j<image.cols;j++)
            {
                image.at<Vec3b>(i,j)[0]=image.at<Vec3b>(i,j)[0]/div*div+div/2;
                image.at<Vec3b>(i,j)[1]=image.at<Vec3b>(i,j)[1]/div*div+div/2;
                image.at<Vec3b>(i,j)[2]=image.at<Vec3b>(i,j)[2]/div*div+div/2;
             }
        }
    }
    
    static void colorReduce(const Mat& image,Mat& outImage,int div)
    {
        int nr=image.rows;
        int nc=image.cols;
        
        outImage.create(image.size(),image.type());
        if(image.isContinuous()&&outImage.isContinuous())
        {
                nr=1;
                nc=nc*image.rows*image.channels();
        }
        for(int i=0;i<nr;i++)
        {
            const uchar* inData=image.ptr<uchar>(i);
            uchar* outData=outImage.ptr<uchar>(i);
            for(int j=0;j<nc;j++)
            {
                 *outData++=*inData++/div*div+div/2;
             }
        }
     }
    
};


class ContentFinder{
private:
    float hranges[2];
    const float* ranges[3];
    int channels[3];
    float threshold;
    cv::Mat histogram;
public:
    ContentFinder():threshold(1.0f){
        ranges[0] = hranges;
        ranges[1] = hranges;
        ranges[2] = hranges;
    }
    
    void setThreshold(float val){
        this->threshold = val;
    }
    
    float getThreshold(){
        return this->threshold;
    }
    
    void setHistogram(const cv::Mat& h){
        histogram = h;
        cv::normalize(histogram,histogram,1.0);
    }
    
    cv::Mat find(const cv::Mat& image){
        cv::Mat result;
        hranges[0] = 0.0;
        hranges[1] = 256.0;
        
        channels[0] = 0;
        channels[1] = 1;
        channels[2] = 2;
        
        return find(image,hranges[0],hranges[1],channels);
    }
    
    cv::Mat find(const cv::Mat& image, float minValue, float maxValue, int* channels){
        cv::Mat result;
        hranges[0] = minValue;
        hranges[1] = maxValue;
        
        for(int i=0 ; i < histogram.dims; i++){
            this->channels[i] = channels[i];
        }
        
        cv::calcBackProject(&image, 1, channels, histogram, result, ranges,255.0);
        if(threshold > 0.0)
            cv::threshold(result, result, 255*threshold, 255.0, cv::THRESH_BINARY);
        
        return result;
    }
};


class ImageComparator{
private:
    cv::Mat refH;
    cv::Mat inputH;
    ColorHistogram hist;
    int nBins;
    
public:
    ImageComparator():nBins(8){
        
    }
    void setNumberOfBins(int value){
        this->nBins = value;
    }
    
    void setReferenceImage(const cv::Mat& image){
        hist.setSize(nBins);
        refH = hist.getHistogram(image);
    }
    
    double compare(const cv::Mat& image,int type = HISTCMP_INTERSECT ){
        inputH = hist.getHistogram(image);
        //cv::EMD
        
        return cv::compareHist(refH,inputH,type);
    }
    
    
};


class ColorDetect{
private:
    int minDist; //minium acceptable distance
    Vec3b target;//target color;
    Mat result; //the result
public:
    ColorDetect():minDist(100),target(0,0,0){}
    ColorDetect(uchar blue,uchar green, uchar red, int _minDist):minDist(_minDist){
        SetTargetColor(red,green,blue);
    }
    cv::Mat operator()(const cv::Mat& image){
        return process(image);
    }
    
    void SetMinDistance(int dist){
        if(dist < 0)
            dist = 0;
        minDist = dist;
    }
    void SetTargetColor(uchar red,uchar green,uchar blue){
        target = cv::Vec3b(blue,green,red);
    }
    void SetTargetColor(Vec3b color){ //set the target color
        target = color;
    }
    Vec3b getTargetColor(){
        return target;
    }
    Mat process(const Mat& image){
        Mat ImageLab=image.clone();
        result.create(image.rows,image.cols,CV_8U);
        
        //将image转换为Lab格式存储在ImageLab中
        cvtColor(image,ImageLab,COLOR_BGR2Lab);
        //将目标颜色由BGR转换为Lab
        Mat temp(1,1,CV_8UC3);
        temp.at<Vec3b>(0,0)=target;//创建了一张1*1的临时图像并用目标颜色填充
        cvtColor(temp,temp,COLOR_BGR2Lab);
        target=temp.at<Vec3b>(0,0);//再从临时图像的Lab格式中取出目标颜色
        
        // 创建处理用的迭代器
        Mat_<Vec3b>::iterator it=ImageLab.begin<Vec3b>();
        Mat_<Vec3b>::iterator itend=ImageLab.end<Vec3b>();
        Mat_<uchar>::iterator itout=result.begin<uchar>();
        while(it!=itend)
        {
            //两个颜色值之间距离的计算
            int dist=static_cast<int>(norm<int,3>(Vec3i((*it)[0]-target[0],
                                                        (*it)[1]-target[1],(*it)[2]-target[2])));
            if(dist<minDist)
                (*itout)=255;
            else
                (*itout)=0;
            it++;
            itout++;
        }
        return result;
    }
    
    Mat process2(const Mat& image){
        Mat output;
        
        cv::absdiff(image, cv::Scalar(target), output);
        std::vector<cv::Mat> images;
        cv::split(output,images);
        output = images[0] + images[1] +images[2];
        cv::threshold(output,output,minDist,255,cv::THRESH_BINARY_INV);
        
        return output;
    }
        
};

class MorphoFeatures{
private:
    int threshold;
    cv::Mat_<uchar> cross;
    cv::Mat_<uchar> diamond;
    cv::Mat_<uchar> square;
    cv::Mat_<uchar> x;
public:
    MorphoFeatures():threshold(-1),cross(5,5),diamond(5,5),square(5,5),x(5,5){
        cross <<
            0,0,1,0,0,
            0,0,1,0,0,
            1,1,1,1,1,
            0,0,1,0,0,
            0,0,1,0,0;
        
        diamond <<
        0,0,1,0,0,
        0,1,0,1,0,
        1,0,0,0,1,
        0,1,0,1,0,
        0,0,1,0,0;
        
        square <<
        1,1,1,1,1,
        1,0,0,0,1,
        1,0,0,0,1,
        1,0,0,0,1,
        1,1,1,1,1;
        
        x <<
        1,0,0,0,1,
        0,1,0,1,0,
        0,0,1,0,0,
        0,1,0,1,0,
        1,0,0,0,1;
        
    }
    
    cv::Mat getCorners(const cv::Mat& image){
        cv::Mat result,result2;
        
        cv::dilate(image,result,cross);
        cv::erode(result,result,diamond);
        cv::dilate(image,result2,x);
        cv::erode(result2,result2,square);
        cv::absdiff(result2,result,result);
        //applyThreshold(result);
        cv::threshold(result,result,threshold,255,THRESH_BINARY);
        
        return result;
    }
    
};

class CvFrameProcessor{
public:
    virtual void process(cv::Mat& input, cv::Mat& output) = 0;
    virtual ~CvFrameProcessor(){};
};

class BGFGSegmentor: public CvFrameProcessor{
    
private:
    cv::Mat gray;
    cv::Mat background;
    cv::Mat backImage;
    cv::Mat foreground;
    double learningRate;
    int threshold;
public:
    BGFGSegmentor():threshold(25){}
    
    void setThreshold(int v){
        threshold = v;
    }
    void process(cv::Mat& frame, cv::Mat& output) override{
        cv::cvtColor(frame,gray,COLOR_BGR2GRAY);
        
        if(background.empty())
            gray.convertTo(background, CV_32F);
        background.convertTo(backImage, CV_8U);
        absdiff(backImage,gray,foreground);
        
        
        cv::threshold(foreground, output, threshold, 255,THRESH_BINARY_INV);
        accumulateWeighted(gray, background,learningRate,output);
        
    }
    
};


class MyFrameProcessor: public CvFrameProcessor{
    
public:
    MyFrameProcessor(){}
    ~MyFrameProcessor(){}
    void process(cv::Mat& frame, cv::Mat& output) override{
        
        int lineType = 8;
        RNG rng(12345);
        Scalar color( 125, 55, rng.uniform(0, 255) );
        Point org(50,50);
        putText( frame, "Tiantian is reading", org, 1,
                5, color, rng.uniform(1, 10), lineType);
        
        //resize(frame,output,cv::Size(frame.cols/2,frame.rows/2));
        Mat gray;
        if(frame.channels() == 3)
            cvtColor(frame,gray,COLOR_BGR2GRAY);
        else gray = frame;
        
        cv::Canny(gray,output,100,200);
        
        //cv::Mat gray;
        //cout << "MyFrameProcessor, channels:"<< frame.channels() << endl;
        //resize(frame,output,cv::Size(frame.cols/2,frame.rows/2));
        /*
        if(frame.channels() == 3)
            cvtColor(frame,gray,CV_BGR2GRAY);
        else gray = frame;
        
        cv::Canny(gray,output,100,200);
        */
       /*
        if(background.empty())
            gray.convertTo(background, CV_32F);
        background.convertTo(backImage, CV_8U);
        absdiff(backImage,gray,foreground);
        
        
        cv::threshold(foreground, output, 128, 255,THRESH_BINARY_INV);
        accumulateWeighted(gray, background,0.2,output);
         */
        
    }
};


class VideoProcessor{
    
private:
    cv::VideoWriter writer;
    cv::VideoCapture capture;
    void (*process)(cv::Mat&, cv::Mat&);
    CvFrameProcessor *frameProcessor;
    bool callIt;
    std::string  windowNameInput;
    std::string windowNameOutput;
    int delay;
    long fnumber;
    long frameToStop;
    bool stop;
    std::vector<std::string> images;
    std::vector<std::string>::const_iterator itImg;
    
    std::string outputFile;
    int currentIndex;
    int digits;
    std::string extension;
    
public:
    VideoProcessor():frameProcessor(nullptr),process(nullptr){}
    ~VideoProcessor(){
        capture.release();
        if(frameProcessor)
            delete frameProcessor;
    }
    
    void setFrameProcessorCB(void (*frameProcessorCB) (cv::Mat& ,cv::Mat&)){
        process = frameProcessorCB;
        frameProcessor = nullptr;
        callProcess();
    }
    
    void setFrameProcessor(CvFrameProcessor* ptr){
        process = nullptr;
        frameProcessor = ptr;
        callProcess();
    }
    
    bool setInput(int camera=0){
        fnumber = 0;
        capture.release();
        capture.open(camera);
        return capture.isOpened();
    }
    
    bool setInput(const std::vector<std::string>& imgs){
        fnumber = 0;
        capture.release();
        images = imgs;
        itImg = images.begin();
        return true;
    }
    
    bool setInput(std::string filename){
        fnumber = 0;
        capture.release();
        capture.open(filename);
        if(capture.isOpened())
        {
            int frame_num = capture.get(cv::CAP_PROP_FRAME_COUNT);
            int width = capture.get(CAP_PROP_FRAME_WIDTH);
            int height = capture.get(CAP_PROP_FRAME_HEIGHT);
            int fps = capture.get(CAP_PROP_FPS);
            cout << "Total fms:" << frame_num
            << " width:" << width << " height:" << height << " fps:" << fps << endl;
        }
        
        return capture.isOpened();
    }
    
    void displayInput(std::string wn){
        windowNameInput = wn;
        cv::namedWindow(windowNameInput);
    }
    
    void displayOutput(std::string wn){
        windowNameOutput = wn;
        cv::namedWindow(windowNameOutput);
    }
    
    void run(){
        cv::Mat frame;
        cv::Mat output;
        
        if(!isOpened())
            return;
        
        stop = false;
        while(!isStopped()){
            if(!readNextFrame(frame))
                break;
            
            if(windowNameInput.length() != 0)
                cv::imshow(windowNameInput,frame);
            
            if(callIt){
                if(process)
                    process(frame,output);
                else if(frameProcessor)
                    frameProcessor->process(frame, output);
                
                fnumber++;
            }else
                output = frame;
            
            if(outputFile.length() != 0)
                writeNextFrame(output);
            
            if(windowNameOutput.length() != 0)
                cv::imshow(windowNameOutput,output);
            
            if(delay > 0 && cv::waitKey(delay) > 0 )
                stopIt();
            
            if(frameToStop > 0 && getFrameNumber() == frameToStop)
                stopIt();
                
        }
    }
    
    void stopIt(){
        stop = true;
    }
    bool isStopped(){
        return stop;
    }
    bool isOpened(){
        return (capture.isOpened() || !images.empty());
    }
    
    void setDelay(int value){
        delay = value;
    }
    
    bool readNextFrame(cv::Mat& frame){
        if(images.size() == 0)
            return capture.read(frame);
        else {
            if(itImg != images.end()){
                frame = cv::imread(*itImg);
                itImg++;
                return frame.data != 0;
            }else
                return false;
        }
        return true;
    }
    
    void callProcess(){
        callIt = true;
    }
    
    void dontCallProcess(){
        callIt = false;
    }
    
    void stopAtFrameNo(long frame){
        frameToStop = frame;
    }
    
    long getFrameNumber(){
        long fnumber = static_cast<long>(capture.get(CAP_PROP_POS_FRAMES));
        return fnumber;
    }
    
    int getCodec(char codec[4]){
        if(images.size() != 0) return -1;
        union{
            int value;
            char code[4];
        }ret;
        
        ret.value = static_cast<int>(capture.get(CAP_PROP_FOURCC));
        if(ret.value != 0)
        {
            codec[0] = ret.code[0];
            codec[1] = ret.code[1];
            codec[2] = ret.code[2];
            codec[3] = ret.code[3];
        }else{
            codec[0] = 'X';
            codec[1] = 'V';
            codec[2] = 'I';
            codec[3] = 'D';
        }
        
        cout << " Codec: " <<ret.value << ", "<< codec[0] << codec[1] << codec[2] << codec[3] << endl;
        
        return ret.value;
    }
    
    bool setOutput(const string& filename, const string& ext, int numberOfDigits = 3, int startIndex = 0){
        if(numberOfDigits < 0)
            return false;
        outputFile = filename;
        extension = ext;
        digits = numberOfDigits;
        currentIndex = startIndex;
        return true;
    }
    
    double getFrameRate(){
        return capture.get(CAP_PROP_FPS);
    }
    
    cv::Size getFrameSize(){
        return cv::Size(capture.get(CAP_PROP_FRAME_WIDTH),capture.get(CAP_PROP_FRAME_HEIGHT));
    }
    
    bool setOutput(const string& filename,int codec = 0, double framerate=0.0, bool isColor = true){
        outputFile = filename;
        extension.clear();
        if(framerate == 0.0)
            framerate = getFrameRate();
        
        char c[4];
        if(codec == 0)
            codec = getCodec(c);
        if(codec == 0){
            //codec = CV_FOURCC('D', 'I','V','X');
            //codec = CV_FOURCC('H', '2','6','4');
            //codec = FOURCC('M', 'J','P','G');
        }
        return writer.open(outputFile, codec, framerate, getFrameSize(), isColor);
    }
    
    void writeNextFrame(cv::Mat& frame){
        if(extension.length()){
            std::stringstream ss;
            ss << outputFile << std::setfill('0') << std::setw(digits) << currentIndex++ <<  extension;
            cv::imwrite(ss.str(),frame);
        }else{
            writer.write(frame);
        }
    }
};


class FaceDetector{
private:
    
    const char* cascadeName = "/Users/wangbinfeng/dev/opencvdemo/opencv/data/haarcascades/haarcascade_frontalface_alt2.xml";//人脸的训练数据
    //String nestedCascadeName = "./haarcascade_eye_tree_eyeglasses.xml";//人眼的训练数据
    const char* nestedCascadeName = "/Users/wangbinfeng/dev/opencvdemo/opencv/data/haarcascades/haarcascade_eye.xml";//人眼的训练数据
public:
    void detect( const char* filename)
    {
       
        CascadeClassifier cascade, nestedCascade;//创建级联分类器对象
        double scale = 1.3;
        
        cv::Mat img = imread(filename,1);
        if( img.empty() )
        {
            cout << "Couldn'g open image " << filename << ". Usage: watershed <image_name>\n";
            return ;
        }
        
        namedWindow( "result", 1 );
        
        if( !cascade.load( cascadeName ) )
        {
            cerr << "ERROR: Could not load classifier cascade" << endl;
            return ;
        }
        
        if( !nestedCascade.load( nestedCascadeName ) )
        {
            cerr << "WARNING: Could not load classifier cascade for nested objects" << endl;
            return ;
        }
        
        int i = 0;
        double t = 0;
        vector<Rect> faces;
        const static Scalar colors[] =  { CV_RGB(0,0,255),
            CV_RGB(0,128,255),
            CV_RGB(0,255,255),
            CV_RGB(0,255,0),
            CV_RGB(255,128,0),
            CV_RGB(255,255,0),
            CV_RGB(255,0,0),
            CV_RGB(255,0,255)} ;//用不同的颜色表示不同的人脸
        
        Mat gray, smallImg( cvRound (img.rows/scale), cvRound(img.cols/scale), CV_8UC1 );//将图片缩小，加快检测速度
        
        cvtColor( img, gray, COLOR_BGR2GRAY );//因为用的是类haar特征，所以都是基于灰度图像的，这里要转换成灰度图像
        resize( gray, smallImg, smallImg.size(), 0, 0, INTER_LINEAR );//将尺寸缩小到1/scale,用线性插值
        equalizeHist( smallImg, smallImg );//直方图均衡
        
        t = (double)getTickCount();//用来计算算法执行时间
        
        
        //检测人脸
        //detectMultiScale函数中smallImg表示的是要检测的输入图像为smallImg，faces表示检测到的人脸目标序列，1.1表示
        //每次图像尺寸减小的比例为1.1，2表示每一个目标至少要被检测到3次才算是真的目标(因为周围的像素和不同的窗口大
        //小都可以检测到人脸),CV_HAAR_SCALE_IMAGE表示不是缩放分类器来检测，而是缩放图像，Size(30, 30)为目标的
        //最小最大尺寸
        cascade.detectMultiScale( smallImg, faces,
                                 1.1, 2, 0
                                 //|CV_HAAR_FIND_BIGGEST_OBJECT
                                 //|CV_HAAR_DO_ROUGH_SEARCH
                                 |CASCADE_SCALE_IMAGE
                                 ,
                                 Size(30, 30) );
        
        t = (double)getTickCount() - t;//相减为算法执行的时间
        printf( "detection time = %g ms\n", t/((double)getTickFrequency()*1000.) );
        for( vector<Rect>::const_iterator r = faces.begin(); r != faces.end(); r++, i++ )
        {
            Mat smallImgROI;
            vector<Rect> nestedObjects;
            Point center;
            Scalar color = colors[i%8];
            int radius;
            center.x = cvRound((r->x + r->width*0.5)*scale);//还原成原来的大小
            center.y = cvRound((r->y + r->height*0.5)*scale);
            radius = cvRound((r->width + r->height)*0.25*scale);
            circle( img, center, radius, color, 3, 8, 0 );
            
            //检测人眼，在每幅人脸图上画出人眼
            if( nestedCascade.empty() )
                continue;
            smallImgROI = smallImg(*r);
            
            //和上面的函数功能一样
            nestedCascade.detectMultiScale( smallImgROI, nestedObjects,
                                           1.1, 2, 0
                                           //|CV_HAAR_FIND_BIGGEST_OBJECT
                                           //|CV_HAAR_DO_ROUGH_SEARCH
                                           //|CV_HAAR_DO_CANNY_PRUNING
                                           |CASCADE_SCALE_IMAGE
                                           ,
                                           Size(30, 30) );
            for( vector<Rect>::const_iterator nr = nestedObjects.begin(); nr != nestedObjects.end(); nr++ )
            {
                center.x = cvRound((r->x + nr->x + nr->width*0.5)*scale);
                center.y = cvRound((r->y + nr->y + nr->height*0.5)*scale);
                radius = cvRound((nr->width + nr->height)*0.25*scale);
                circle( img, center, radius, color, 3, 8, 0 );//将眼睛也画出来，和对应人脸的图形是一样的
            }
        }
        cv::imshow( "result", img );
    }
};

#endif /* opencvdemo_hpp */
