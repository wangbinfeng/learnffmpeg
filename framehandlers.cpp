//
//  framehandlers.cpp
//  opencvdemo
//
//  Created by 王斌峰 on 2018/11/26.
//  Copyright © 2018年 Binfeng. All rights reserved.
//


#include "framehandlers.hpp"


using namespace cv;
using namespace cv::face;
using namespace std;
using namespace cv::face;

int CvFaceDetector::init(){
    faceDetector.load(lbpfilePath);
    
    FacemarkLBF::Params params;
    params.n_landmarks = 68; // 68个标注点
    params.initShape_n = 10;
    params.stages_n = 5; // 算法的5个强化步骤
    params.tree_n = 6; // 模型中每个标注点结构树 数目
    params.tree_depth = 5; // 决策树深度
    
    // Create an instance of Facemark
    facemark = FacemarkLBF::create(params);
    
    // Load landmark detector
    facemark->loadModel(lbfmodel);
    
    
    return 1;
}

void CvFaceDetector::process(cv::Mat& input, cv::Mat& output){
    // Find face
    vector<Rect> faces;
    Mat gray;
    // Convert frame to grayscale because
    // faceDetector requires grayscale image.
    cout << "input channels:"<< input.channels() << endl;
    if (input.channels() > 1)
        cvtColor(input, gray, COLOR_BGR2GRAY);
    else
        gray = input.clone();
    
    cout << "equalizeHist .." << endl;
    equalizeHist(gray, gray);
    
    //face_cascade.detectMultiScale(gray, faces, 1.1, 1, CASCADE_SCALE_IMAGE, Size(50, 50));
    //Mat(faces).copyTo(faces);
    cout << "detect multi scale " << endl;
    faceDetector.detectMultiScale(gray, faces);
    
    // Variable for landmarks.
    // Landmarks for one face is a vector of points
    // There can be more than one face in the image. Hence, we
    // use a vector of vector of points.
    vector< vector<Point2f> > landmarks;
    
    cout << "run landmark detector" << endl;
   
    bool success = facemark->fit(input,faces,landmarks);
    
    if(success)
    {
        cout << "detector sucess" << endl;
        Point eye_left; // 36th
        Point eye_right; // 45th
        
        for (unsigned long k = 0; k<landmarks.size(); k++)
            draw_landmarks(input, landmarks[k]);
        
        for (unsigned long i = 0; i<faces.size(); i++) {
            eye_left = landmarks[i][36];
            eye_right = landmarks[i][45];
            line(input, eye_left, eye_right, Scalar(255, 0, 0), 2, 8, 0);
            
            cout << "face alignment" << endl;
            //face_alignment(input, output, eye_left, eye_right, faces[i]);
            
            cout << "draw face rectangle" << endl;
            rectangle(input, faces[i], Scalar(255, 0, 0));
            
            cout << "draw 68 landmarks in face" << endl;
            for (unsigned long k = 0; k<landmarks[i].size(); k++){
                cv::circle(input, landmarks[i][k], 2, cv::Scalar(0, 0, 255), FILLED);
            }
           
        }
        
    }else
        cout << "detector fail" << endl;
    
    output = input;
}


void CvFaceDetector::draw_polyline
(
 Mat &im,
 const vector<Point2f> &landmarks,
 const int start,
 const int end,
 bool isClosed
 )
{
    // 收集开始和结束索引之间的所有点
    vector <Point> points;
    for (int i = start; i <= end; i++)
    {
        points.push_back(cv::Point(landmarks[i].x, landmarks[i].y));
    }
    
    // 绘制多边形曲线
    polylines(im, points, isClosed, cv::Scalar(255, 200,0), 2, 16);
    
}

void CvFaceDetector::draw_landmarks(Mat &im, vector<Point2f> &landmarks)
{
    // 在脸上绘制68点及轮廓（点的顺序是特定的，有属性的）
    if (landmarks.size() == 68)
    {
        draw_polyline(im, landmarks, 0, 16);           // Jaw line
        draw_polyline(im, landmarks, 17, 21);          // Left eyebrow
        draw_polyline(im, landmarks, 22, 26);          // Right eyebrow
        draw_polyline(im, landmarks, 27, 30);          // Nose bridge
        draw_polyline(im, landmarks, 30, 35, true);    // Lower nose
        draw_polyline(im, landmarks, 36, 41, true);    // Left eye
        draw_polyline(im, landmarks, 42, 47, true);    // Right Eye
        draw_polyline(im, landmarks, 48, 59, true);    // Outer lip
        draw_polyline(im, landmarks, 60, 67, true);    // Inner lip
    }
    else
    {
        // 如果人脸关键点数不是68，则我们不知道哪些点对应于哪些面部特征。所以，我们为每个landamrk画一个圆圈。
        for(int i = 0; i < landmarks.size(); i++)
        {
            circle(im,landmarks[i],3,cv::Scalar(255, 200,0), FILLED);
        }
    }
    
}


void CvFaceDetector::face_alignment(Mat &face, Mat& result,Point left, Point right, Rect roi) {
    int offsetx = roi.x;
    int offsety = roi.y;
    
    // 计算中心位置
    int cx = roi.width / 2;
    int cy = roi.height / 2;
    
    // 计算角度
    int dx = right.x - left.x;
    int dy = right.y - left.y;
    double degree = 180 * ((atan2(dy, dx)) / CV_PI);
    
    // 旋转矩阵计算
    Mat M = getRotationMatrix2D(Point2f(cx, cy), degree, 1.0);
    Point2f center(cx, cy);
    Rect bbox = RotatedRect(center, face.size(), degree).boundingRect();
    M.at<double>(0, 2) += (bbox.width / 2.0 - center.x);
    M.at<double>(1, 2) += (bbox.height / 2.0 - center.y);
   
    warpAffine(face, result, M, bbox.size());
    
}


int QRcodeProcessor::init(){
    
    return 1;
}


void QRcodeProcessor::process(cv::Mat& input, cv::Mat& output){
    using namespace cv;
    using namespace std;
    
    cv::Mat bbox;
    std::string data = qrDecoder.detectAndDecode(input, bbox, output);
    if(data.length()>0)
    {
        cout << "Decoded Data : " << data << endl;
        int n = bbox.rows;
        for(int i = 0 ; i < n ; i++)
        {
            line(input, Point2i(bbox.at<float>(i,0),bbox.at<float>(i,1)), Point2i(bbox.at<float>((i+1) % n,0), bbox.at<float>((i+1) % n,1)), Scalar(255,0,0), 3);
        }
        output.convertTo(output, CV_8UC3);
        
        int lineType = 1;
        RNG rng(12345);
        Scalar color( 125, 55, rng.uniform(0, 255) );
        
        putText( input, data, Point2i(0,bbox.at<float>(0,1)), 1,
                2, color, rng.uniform(1, 10), lineType);
       
    }
    else
        cout << "QR Code not detected" << endl;
}

void ZbarProcessor::process(cv::Mat &im, cv::Mat &output){
    using namespace zbar;
   
    // Configure scanner
    scanner.set_config(ZBAR_NONE, ZBAR_CFG_ENABLE, 1);
    
    // Convert image to grayscale
    Mat imGray;
    cvtColor(im, imGray,COLOR_BGR2GRAY);
    
    // Wrap image data in a zbar image
    Image image(im.cols, im.rows, "Y800", (uchar *)imGray.data, im.cols * im.rows);
    
    // Scan the image for barcodes and QRCodes
    int n = scanner.scan(image);
    
    // Print results
    for(Image::SymbolIterator symbol = image.symbol_begin(); symbol != image.symbol_end(); ++symbol)
    {
        // Print type and data
        cout << "Type : " << symbol->get_type_name() << endl;
        cout << "Data : " << symbol->get_data() << endl << endl;
        
        // Obtain location
        std::vector<Point> points;
        for(int i = 0; i< symbol->get_location_size(); i++)
        {
            points.push_back(Point(symbol->get_location_x(i),symbol->get_location_y(i)));
        }
        
        vector<Point> hull;
        
        // If the points do not form a quad, find convex hull
        if(points.size() > 4)
            convexHull(points, hull);
        else
            hull = points;
        
        // Number of points in the convex hull
        int n = hull.size();
        
        for(int j = 0; j < n; j++)
        {
            line(im, hull[j], hull[ (j+1) % n], Scalar(255,0,0), 3);
        }
    }
    
    image.set_data(NULL,0);
}
