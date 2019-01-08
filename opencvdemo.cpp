
using namespace std;

int maincv()
{
    //const char *fileName = "/Users/wangbinfeng/dev/opencvdemo/tiantian_480p.mp4";
    const char *fileName = "/Users/wangbinfeng/dev/opencvdemo/tiantian.mov";
    //FFmpegUtil* ffmpeg = new FFmpegUtil(fileName);
    //FFmpegUtil* ffmpeg = new FFmpegUtil("");
    //ffmpeg->setCvFrameProcessor(new BGFGSegmentor());
    //ffmpeg->setFilter("scale=320:-1");
    const char *mirrorfilter = "crop=iw/2:ih:0:0,split[left][tmp];[tmp]hflip[right];[left][right] hstack";
    const char *wavefilter = "format=gbrp,split=4[a][b][c][d],[d]histogram=display_mode=0:level_height=244[dd],[a]waveform=m=1:d=0:r=0:c=7[aa],[b]waveform=m=0:d=0:r=0:c=7[bb],[c][aa]vstack[V],[bb][dd]vstack[V2],[V][V2]hstack";
    const char *flter2="split=4[a][b][c][d];[b]lutrgb=g=0:b=0[x];[c]lutrgb=r=0:b=0[y];[d]lutrgb=r=0:g=0[z];[a][x][y][z]hstack=4";
    const char *waterf =   "movie=/Users/wangbinfeng/dev/opencvdemo/star.jpg[wm];[in][wm]overlay=5:5[out]";
    const char *blackwhite = "lutyuv='u=128:v=128'";
    
    //const char *filters_descr = "hflip";
    //const char *filters_descr = "hue='h=60:s=-3'";
    //const char *filters_descr = "crop=2/3*in_w:2/3*in_h";
    //const char *drawbox = "drawbox=x=200:y=200:w=300:h=300:color=pink@0.5";
    const char *drawgrid="scale=320:-1,drawgrid=width=100:height=100:thickness=4:color=pink@0.9";
    /*This filtergraph splits the input stream in two streams, then sends one stream through the crop filter and the vflip filter, before merging it back with the other stream by overlaying it on top. You can use the following command to achieve this:
    */
    const char *f1="split [main][tmp]; [tmp] crop=iw:ih/2:0:0, vflip [flip]; [main][flip] overlay=0:H/2";
    //ffmpeg->setFilter(drawgrid);
    //ffmpeg->setOutputFile("/Users/wangbinfeng/dev/opencvdemo/tiantian_480p_3.mp4");
    //ffmpeg->setExtractFrame(true);
    //ffmpeg->setSavedPath("/Users/wangbinfeng/dev/opencvdemo");
    //ffmpeg->run();
    //delete ffmpeg;

    return 0;

}

