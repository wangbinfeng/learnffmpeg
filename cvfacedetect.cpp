#include <opencv2/opencv.hpp>
#include <zbar.h>

using namespace cv;
using namespace std;
using namespace zbar;

typedef struct
{
    string type;
    string data;
    vector <Point> location;
} decodedObject;

// Find and decode barcodes and QR codes
void decode(Mat &im, vector<decodedObject>&decodedObjects)
{
    
    // Create zbar scanner
    ImageScanner scanner;
    
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
        decodedObject obj;
        
        obj.type = symbol->get_type_name();
        obj.data = symbol->get_data();
        
        // Print type and data
        cout << "Type : " << obj.type << endl;
        cout << "Data : " << obj.data << endl << endl;
        
        // Obtain location
        for(int i = 0; i< symbol->get_location_size(); i++)
        {
            obj.location.push_back(Point(symbol->get_location_x(i),symbol->get_location_y(i)));
        }
        
        decodedObjects.push_back(obj);
    }
}

// Display barcode and QR code location
void display(Mat &im, vector<decodedObject>&decodedObjects)
{
    // Loop over all decoded objects
    namedWindow("Results");
    for(int i = 0; i < decodedObjects.size(); i++)
    {
        vector<Point> points = decodedObjects[i].location;
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
    
    // Display results
    imshow("Results", im);
    waitKey(0);
    
}

int main_cv(int argc, char* argv[])
{
    // Read image
    Mat image = imread("/Users/wangbinfeng/dev/opencvdemo/testbar.jpg");
    //2. 转化为灰度图
    Mat imageGray,imageGuussian;
    Mat imageSobelX,imageSobelY,imageSobelOut;

    cvtColor(image,imageGray,COLOR_RGB2GRAY);
//
//    namedWindow("2");
//    imshow("2",imageGray);
//
//    //3. 高斯平滑滤波
//    GaussianBlur(imageGray,imageGuussian,Size(3,3),0);
//    namedWindow("3");
//    imshow("3",imageGuussian);
//
//    //4.求得水平和垂直方向灰度图像的梯度差,使用Sobel算子
//    Mat imageX16S,imageY16S;
//    Sobel(imageGuussian,imageX16S,CV_16S,1,0,3,1,0,4);
//    Sobel(imageGuussian,imageY16S,CV_16S,0,1,3,1,0,4);
//    convertScaleAbs(imageX16S,imageSobelX,1,0);
//    convertScaleAbs(imageY16S,imageSobelY,1,0);
//    imageSobelOut=imageSobelX-imageSobelY;
//    namedWindow("4");
//    imshow("4.X",imageSobelX);
//    namedWindow("4.Y");
//    imshow("4.Y",imageSobelY);
//    namedWindow("4.XY");
//    imshow("4.XY",imageSobelOut);
//
//    //5.均值滤波，消除高频噪声
//    blur(imageSobelOut,imageSobelOut,Size(3,3));
//    imshow("5.",imageSobelOut);
//
//    //6.二值化
//    Mat imageSobleOutThreshold;
//    threshold(imageSobelOut,imageSobleOutThreshold,180,255,THRESH_BINARY);
//    imshow("6.",imageSobleOutThreshold);
//
//    //7.闭运算，填充条形码间隙
//    Mat  element=getStructuringElement(0,Size(7,7));
//    morphologyEx(imageSobleOutThreshold,imageSobleOutThreshold,MORPH_CLOSE,element);
//    imshow("7.",imageSobleOutThreshold);
//
//    //8. 腐蚀，去除孤立的点
//    erode(imageSobleOutThreshold,imageSobleOutThreshold,element);
//    imshow("8.",imageSobleOutThreshold);
//
//    //9. 膨胀，填充条形码间空隙，根据核的大小，有可能需要2~3次膨胀操作
//    dilate(imageSobleOutThreshold,imageSobleOutThreshold,element);
//    dilate(imageSobleOutThreshold,imageSobleOutThreshold,element);
//    dilate(imageSobleOutThreshold,imageSobleOutThreshold,element);
//    imshow("9.",imageSobleOutThreshold);
//    vector<vector<Point>> contours;
//    vector<Vec4i> hiera;
//
//    //10.通过findContours找到条形码区域的矩形边界
//    findContours(imageSobleOutThreshold,contours,hiera,RETR_EXTERNAL,CHAIN_APPROX_NONE);
//    for(int i=0;i<contours.size();i++)
//    {
//        Rect rect=boundingRect((Mat)contours[i]);
//        rectangle(image,rect,Scalar(255),2);
//    }
//    imshow("10.",image);
    
//    Mat grayImage,blurImage,thresholdImage,gradientXImage,gradientYImage,gradientImage,morphImage;
//
//    if(srcImage.channels() == 3)
//    {
//        cvtColor(srcImage,grayImage,COLOR_RGB2GRAY);
//    }
//    else
//    {
//        grayImage = srcImage.clone();
//    }
//
//    Scharr(grayImage,gradientXImage,CV_32F,1,0);
//    Scharr(grayImage,gradientYImage,CV_32F,0,1);
//    //因为我们需要的条形码在需要X方向水平,所以更多的关注X方向的梯度幅值,而省略掉Y方向的梯度幅值
//    subtract(gradientXImage,gradientYImage,gradientImage);
//    //归一化为八位图像
//    convertScaleAbs(gradientImage,gradientImage);
//    //看看得到的梯度图像是什么样子
//    //imshow(windowNameString,gradientImage);
//    //对图片进行相应的模糊化,使一些噪点消除
//    blur(gradientImage,blurImage,Size(9,9));
//    //模糊化以后进行阈值化,得到到对应的黑白二值化图像,二值化的阈值可以根据实际情况调整
//    threshold(blurImage,thresholdImage,210,255,THRESH_BINARY);
//    //看看二值化图像
//    //imshow(windowNameString,thresholdImage);
//    //二值化以后的图像,条形码之间的黑白没有连接起来,就要进行形态学运算,消除缝隙,相当于小型的黑洞,选择闭运算
//    //因为是长条之间的缝隙,所以需要选择宽度大于长度
//    Mat kernel = getStructuringElement(MORPH_RECT,Size(21,7));
//    morphologyEx(thresholdImage,morphImage,MORPH_CLOSE,kernel);
//    //看看形态学操作以后的图像
//    //imshow(windowNameString,morphImage);
//    //现在要让条形码区域连接在一起,所以选择膨胀腐蚀,而且为了保持图形大小基本不变,应该使用相同次数的膨胀腐蚀
//    //先腐蚀,让其他区域的亮的地方变少最好是消除,然后膨胀回来,消除干扰,迭代次数根据实际情况选择
//    erode(morphImage, morphImage, getStructuringElement(MORPH_RECT, Size(3,3)),Point(-1,-1),4);
//    dilate(morphImage, morphImage, getStructuringElement(MORPH_RECT, Size(3,3)),Point(-1,-1),4);
//    //看看形态学操作以后的图像
//    imshow("zbar",morphImage);
//    vector<vector<Point2i>>contours;
//    vector<float>contourArea;
//    //接下来对目标轮廓进行查找,目标是为了计算图像面积
//    findContours(morphImage,contours,RETR_EXTERNAL,CHAIN_APPROX_SIMPLE);
//    //计算轮廓的面积并且存放
//    for(int i = 0; i < contours.size();i++)
//    {
//        contourArea.push_back(cv::contourArea(contours[i]));
//    }
//    //找出面积最大的轮廓
//    double maxValue;Point maxLoc;
//    minMaxLoc(contourArea, NULL,&maxValue,NULL,&maxLoc);
//    //计算面积最大的轮廓的最小的外包矩形
//    RotatedRect minRect = minAreaRect(contours[maxLoc.x]);
//    //为了防止找错,要检查这个矩形的偏斜角度不能超标
//    //如果超标,那就是没找到
//
//    if(minRect.angle<2.0)
//    {
//        //找到了矩形的角度,但是这是一个旋转矩形,所以还要重新获得一个外包最小矩形
//        Rect myRect = boundingRect(contours[maxLoc.x]);
//        //把这个矩形在源图像中画出来
//        rectangle(srcImage,myRect,Scalar(0,255,255),3,LINE_AA);
//        //看看显示效果,找的对不对
//        imshow("zbar",srcImage);
//        //将扫描的图像裁剪下来,并保存为相应的结果,保留一些X方向的边界,所以对rect进行一定的扩张
//        myRect.x= myRect.x - (myRect.width/20);
//        myRect.width = myRect.width*1.1;
//
//        cv::rectangle(srcImage, myRect, Scalar(255,0,0));
//    }
   
//    vector<decodedObject> decodedObjects;
//    decode(image, decodedObjects);
//    display(image, decodedObjects);

    
    
    waitKey();
    
    return EXIT_SUCCESS;
}
