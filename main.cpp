
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/stitching.hpp"
#include "opencv2/opencv.hpp"

#include "Media.h"
#include "RtspClient.hpp"

using namespace std;

int main(int argv, char* argc[])
{
	//const char *filename = "/Users/wangbinfeng/dev/opencvdemo/xiyouji.mp3";
    //const char *filename = "/Users/wangbinfeng/dev/opencvdemo/tiantian.mov";
    //const char *filename = "/Users/wangbinfeng/dev/opencvdemo/avitest.avi";
    
    const char *rtsp = "rtsp://192.168.1.57/para.m4e";
    const char *rtmp = "rtmp://192.168.1.57:1935/vod/small.mp4";
    const char *rtmp1 = "rtmp://192.168.1.57:1935/live/binfeng";
    const char *drawgrid="scale=640:-1,drawgrid=width=100:height=100:thickness=4:color=pink@0.9";
	
//    MediaState media(filename);
//    media.set_filter_desc(drawgrid);
//    //media.set_audio_background_filename("/Users/wangbinfeng/dev/opencvdemo/frame_4.jpg");
//    media.start();
//    //media.play();
    
//    FFmpegTools ffmpeg;
//    const char *filename = "/Users/wangbinfeng/dev/opencvdemo/cap.mp4";
//    ffmpeg.cv_capture_camera_to_file(filename,800,600,25);
  
    RtspClient client("192.168.1.134",554,"rtsp://192.168.1.134/test.aac");
    client.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(30000));
    client.close();
    
    
	return 0;
}

