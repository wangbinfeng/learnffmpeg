//
//  openglexample.cpp
//  opencvdemo
//
//  Created by 王斌峰 on 2018/11/15.
//  Copyright © 2018年 Binfeng. All rights reserved.
//
#include <stdio.h>
#include <math.h>
#include <setjmp.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "jpeglib.h"

#include <stdio.h>
#include <string.h>

#include "SOIL.h"
#include "stb_image.h"

#define PI 3.1415926

//画矩形，传入的是左下角XY坐标和右上角XY坐标
static void glRect(int leftX,int leftY,int rightX,int rightY){
    //画封闭曲线
    glBegin(GL_LINE_LOOP);
    //左下角
    glVertex2d(leftX,leftY);
    //右下角
    glVertex2d(rightX,leftY);
    //右上角
    glVertex2d(rightX,rightY);
    //左上角
    glVertex2d(leftX,rightY);
    //结束画线
    glEnd();
}

//画圆角矩形，传入矩形宽高，角半径，矩形中心点坐标
static void glRoundRec(int centerX,int centerY,int width,int height,float cirR){
    //二分之PI，一个象限的角度
    float PI_HALF = PI/2;
    //划分程度,值越大画得越精细
    float divide=20.0;
    //圆角矩形的坐标
    float tx,ty;
    //画封闭曲线
    glBegin(GL_LINE_LOOP);
    //四个象限不同的操作符
    int opX[4]={1,-1,-1,1};
    int opY[4]={1,1,-1,-1};
    //用来计数，从第一象限到第四象限
    float x=0;
    //x自增时加的值
    float part=1/divide;
    //计算内矩形宽高一半的数值
    int w=width/2-cirR;
    int h=height/2-cirR;
    //循环画线
    for(x=0;x<4;x+=part){
        //求出弧度
        float rad = PI_HALF*x;
        //计算坐标值
        tx=cirR*cos(rad)+opX[(int)x]*w+centerX;
        ty=cirR*sin(rad)+opY[(int)x]*h+centerY;
        //传入坐标画线
        glVertex2f(tx,ty);
    }
    //结束画线
    glEnd();
}

//画弧线，相对偏移量XY，开始的弧度，结束的弧度，半径
static void glArc(double x,double y,double start_angle,double end_angle,double radius)
{
    //开始绘制曲线
    glBegin(GL_LINE_STRIP);
    //每次画增加的弧度
    double delta_angle=PI/180;
    //画圆弧
    for (double i=start_angle;i<=end_angle;i+=delta_angle)
    {
        //绝对定位加三角函数值
        double vx=x+radius * cos(i);
        double vy=y+radius*sin(i);
        glVertex2d(vx,vy);
    }
    //结束绘画
    glEnd();
}


//画圆
static void glCircle(double x, double y, double radius)
{
    //画全圆
    glArc(x,y,0,2*PI,radius);
}

//画三角形，传入三个点的坐标
static void glTri(int x1,int y1,int x2,int y2,int x3,int y3){
    //画封闭线
    glBegin(GL_LINE_LOOP);
    //一点
    glVertex2d(x1,y1);
    //二点
    glVertex2d(x2,y2);
    //三点
    glVertex2d(x3,y3);
    //结束画线
    glEnd();
}

//画线，传入两点坐标
static void glLine(int x1,int y1,int x2,int y2){
    //画封闭线
    glBegin(GL_LINE_STRIP);
    //一点
    glVertex2d(x1,y1);
    //二点
    glVertex2d(x2,y2);
    //结束画线
    glEnd();
}

//窗口大小变化时调用的函数
static void ChangeSize(GLsizei w,GLsizei h)
{
    //避免高度为0
    if(h==0) {
        h=1;
    }
    printf("w:%d,h:%d\n",w,h);
    //定义视口大小，宽高一致
    glViewport(0,0,w,h);
    int half = 400;
    //重置坐标系统，使投影变换复位
    glMatrixMode(GL_PROJECTION);
    //将当前的用户坐标系的原点移到了屏幕中心
    glLoadIdentity();
    //定义正交视域体
    glOrtho(-w/2, w/2, -h/2, h/2,-100, 100);
//    if(w<h) {
//        //如果高度大于宽度，则将高度视角扩大，图形显示居中
//        glOrtho(-half,half,-half*h/w,half*h/w,-half,half);
//    } else {
//        //如果宽度大于高度，则将宽度视角扩大，图形显示居中
//        glOrtho(-half*w/h,half*w/h,-half,half,-half,half);
//    }
    glColor3f(0.5,0.0,0.0);
    glCircle(0, 0, h/2);
    
}

static void display(){
    //GL_COLOR_BUFFER_BIT表示清除颜色
    glClear(GL_COLOR_BUFFER_BIT);
    //设置画线颜色
    glColor3f(0.5,0.5,0.5);
    //画点大小
    glPointSize(2);
    //画圆角矩形，大肚子
    glRoundRec(0,0,146,120,15);
    
    //画圆，中间小圈
    glCircle(0,0,10);
    //画矩形，脖子
    glRect(-25,60,25,76);
    //画圆角矩形，大脸
    glRoundRec(0,113,128,74,10);
    //两个眼睛
    glCircle(-30,111,10);
    glCircle(30,111,10);
    //两条天线
    glLine(-35,150,-35,173);
    glLine(35,150,35,173);
    //圆角矩形，两个耳朵
    glRoundRec(81,115,20,34,5);
    glRoundRec(-81,115,20,34,5);
    //圆弧，画嘴
    glArc(0,133,11*PI/8,13*PI/8,45);
    //画三角，肚子里的三角
    glTri(-30,-15,30,-15,0,28);
    //画矩形，胳膊连接处
    glRect(-81,43,-73,25);
    glRect(81,43,73,25);
    //画矩形，上臂
    glRect(-108,45,-81,0);
    glRect(108,45,81,0);
    //画矩形，中臂
    glRect(-101,0,-88,-4);
    glRect(101,0,88,-4);
    //画矩形，下臂
    glRect(-108,-4,-81,-37);
    glRect(108,-4,81,-37);
    //画圆形，手掌
    glCircle(-95,-47,10);
    glCircle(95,-47,10);
    //画腿连接处
    glRect(-41,-62,-21,-66);
    glRect(41,-62,21,-66);
    //画圆角矩形，大长腿
    glRoundRec(-32,-92,38,52,10);
    glRoundRec(32,-92,38,52,10);
    //画矩形，脚踝
    glRect(-41,-125,-21,-117);
    glRect(41,-125,21,-117);
    //画矩形，大脚掌
    glRect(-59,-125,-8,-137);
    glRect(59,-125,8,-137);
    
    //保证前面的OpenGL命令立即执行，而不是让它们在缓冲区中等待
    glFlush();
}

void display_views(int w,int h){
    
        //画分割线 ，分成四个视图区域
        glClear(GL_COLOR_BUFFER_BIT);
        glColor3f(1.0, 0.0, 0.0);
        glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    //将当前的用户坐标系的原点移到了屏幕中心
    glLoadIdentity();
    
        glBegin(GL_LINES);
        glVertex2f(-1.0, 0);
        glVertex2f(1.0, 0);
        glVertex2f(0, -1.0);
        glVertex2f(0, 1.0);
        glEnd();
        //定义左下角区域
        glColor3f(0.0, 1.0, 0.0);
        glViewport(0, 0, w/2, h/2);
        glBegin(GL_POLYGON);
        glVertex2f(-0.5, -0.5);
        glVertex2f(-0.5, 0.5);
        glVertex2f(0.5, 0.5);
        glVertex2f(0.5, -0.5);
        glEnd();
        //定义右上角区域
        glColor3f(0.0, 0.0, 1.0);
        glViewport(w/2, h/2,w/2, h/2);
        glBegin(GL_POLYGON);
        glVertex2f(-0.5, -0.5);
        glVertex2f(-0.5, 0.5);
        glVertex2f(0.5, 0.5);
        glVertex2f(0.5, -0.5);
        glEnd();
        //定义在左上角的区域
        glColor3f(1.0, 0.0, 0.0);
        glViewport(0, h/2, w/2, h/2);
        glBegin(GL_POLYGON);
        glVertex2f(-0.5, -0.5);
        glVertex2f(-0.5, 0.5);
        glVertex2f(0.5, 0.5);
        glVertex2f(0.5, -0.5);
        glEnd();
        //定义在右下角的区域
        glColor3f(1.0, 1.0, 1.0);
        glViewport(w/2, 0, w/2, h/2);
        glBegin(GL_POLYGON);
        glVertex2f(-0.5, -0.5);
        glVertex2f(-0.5, 0.5);
        glVertex2f(0.5, 0.5);
        glVertex2f(0.5, -0.5);
        glEnd();
        
        
        glFlush();
    

}

static int read_jpeg_file (const char * filename,uint8_t **data,int &width, int& height,unsigned short& depth)
{
    struct jpeg_decompress_struct cinfo;
    struct my_error_mgr {
        struct jpeg_error_mgr pub;    /* "public" fields */
        jmp_buf setjmp_buffer;    /* for return to caller */
    };
    typedef struct my_error_mgr * my_error_ptr;
    struct my_error_mgr jerr;
    /* More stuff */
    FILE * infile;        /* source file */
    JSAMPARRAY buffer;        /* Output row buffer */
    int row_stride;        /* physical row width in output buffer */
    if ((infile = fopen(filename, "rb")) == NULL) {
        fprintf(stderr, "can't open %s\n", filename);
        return 0;
    }
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = [](j_common_ptr cinfo)
    {
        my_error_ptr myerr = (my_error_ptr) cinfo->err;
        (*cinfo->err->output_message) (cinfo);
        longjmp(myerr->setjmp_buffer, 1);
    };
    
    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return 0;
    }
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);
    (void) jpeg_read_header(&cinfo, TRUE);
    (void) jpeg_start_decompress(&cinfo);
    width=cinfo.output_width;
    height=cinfo.output_height;
    depth=cinfo.output_components;
    
    *data = new uint8_t[width*height*depth];
    memset(*data,0,sizeof(uint8_t)*width*height*depth);
    row_stride = cinfo.output_width * cinfo.output_components;
    buffer = (*cinfo.mem->alloc_sarray)
    ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);
    uint8_t *p= *data;
    while (cinfo.output_scanline < cinfo.output_height) {
        (void) jpeg_read_scanlines(&cinfo, buffer, 1);
        memcpy(p,*buffer,width*depth);    //将buffer中的数据逐行给src_buff
        p+=width*depth;
    }
    (void) jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);
    return 1;
}

static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    static int near = -400;
    static int far = 200;
    const char* key_name = glfwGetKeyName(key, 0);
    fprintf(stdout,"key pressed :%s\n",(key_name == NULL ? "" : key_name));
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    else if( key == GLFW_KEY_1 && action == GLFW_PRESS){
         glOrtho(-400, 400, -300, 300, near, far);
        far +=50;
    }
        
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_RIGHT ){
        fprintf(stdout,"mouse button right %s \n",
                (action == GLFW_PRESS ? "pressed" : (action == GLFW_RELEASE ? "release" : "" )));
    }else if (button == GLFW_MOUSE_BUTTON_LEFT){
        fprintf(stdout,"mouse button left %s \n",
                (action == GLFW_PRESS ? "pressed" : (action == GLFW_RELEASE ? "release" : "" )));
    }
    
}


static void character_callback(GLFWwindow* window, unsigned int codepoint)
{
    fprintf(stdout,"character:%d\n",codepoint);
}

static void charmods_callback(GLFWwindow* window, unsigned int codepoint, int mods)
{
    fprintf(stdout,"character:%d,mods:%d\n",codepoint,mods);
}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    fprintf(stdout,"mouse:%f,%f\n",xpos,ypos);
}

static void cursor_enter_callback(GLFWwindow* window, int entered)
{
    if (entered)
    {
        fprintf(stdout, " The cursor entered the client area of the window \n");
    }
    else
    {
        fprintf(stdout, " The cursor left the client area of the window\n");
    }
}

static void frame_buffer_size_callback(GLFWwindow* window, int width, int height){
    //ChangeSize(width, height);
}
int main_tt(int argc, char **argv)
{
    GLFWwindow* window;
    
    glfwSetErrorCallback(error_callback);
    
    /* Initialize the library */
    if (!glfwInit())
        return 0;
    
    //glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    //glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(800, 600, "Audio Player", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return 0;
    }
    
    //glfwSetCharCallback(window, character_callback);
    //glfwSetCharModsCallback(window, charmods_callback);
    //glfwSetCursorPosCallback(window, cursor_position_callback);
    //glfwSetCursorEnterCallback(window, cursor_enter_callback);
    glfwSetKeyCallback(window, key_callback);
    //glfwSetCharCallback(window, character_callback);
    //glfwSetCharModsCallback(window, charmods_callback);
    //glfwSetCursorPosCallback(window, cursor_position_callback);
    //glfwSetCursorEnterCallback(window, cursor_enter_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetFramebufferSizeCallback(window,frame_buffer_size_callback);
    
    /* Make the window's context current */
    glfwMakeContextCurrent(window);
    
    glEnable(GL_TEXTURE_2D);
    
    //std::thread t(display_frames);
    GLuint texture_ID;
    glGenTextures(1, &texture_ID); // 分配一个纹理对象的编号
    glBindTexture(GL_TEXTURE_2D, texture_ID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    uint8_t *data = NULL;
    int w,h;
    unsigned short d;
    int channels;
    const char* filename = "/Users/wangbinfeng/dev/opencvdemo/star.jpg";
    //const char* filename = "/Users/wangbinfeng/dev/opencvdemo/IMG_3147.jpg";
    //data = SOIL_load_image(filename,&w, &h, 0, SOIL_LOAD_AUTO);
    
    data = stbi_load(filename, &w, &h, &channels, 0);
    
    printf("image w:%d,h:%d\n",w,h);
    //read_jpeg_file(filename, &data, w, h, d);
    
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, data);
    //glBindTexture(GL_TEXTURE_2D, texture_ID);
    //glGenerateMipmap(GL_TEXTURE_2D);
    
    glClearColor(1.0f,1.0f,1.0f,1.0f);
    //ChangeSize(800,600);
    //glRotatef(-90,0.0f,0.0f,1.0f);
    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        /* Render here */
        glClear(GL_COLOR_BUFFER_BIT);
        //display();
        
        glfwGetWindowSize(window, &w, &h);
        printf("w:%d,h:%d\n",w,h);
        glCircle(0, 0, h/2);
        display_views(w,h);
//        glBegin(GL_QUADS);
//        glTexCoord2i(0, 0); glVertex2f(-200.0f, 0.0f);
//        glTexCoord2f(0, h); glVertex2f(-200.0f, 200.0f);
//        glTexCoord2f(w, h); glVertex2f(0.0f, 200.0f);
//        glTexCoord2f(w, 0); glVertex2f(0.0f, 0.0f);
//        glEnd();
        
        /* Swap front and back buffers */
        glfwSwapBuffers(window);
        /* Poll for and process events */
        glfwPollEvents();
    }
    //SOIL_free_image_data(data);
    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1,&texture_ID);
    glDisable(GL_TEXTURE_2D);
    
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
