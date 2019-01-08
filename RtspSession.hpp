//
//  RtspSession.hpp
//  testboost
//
//  Created by 王斌峰 on 2018/10/3.
//  Copyright © 2018年 Binfeng. All rights reserved.
//

#ifndef RtspSession_hpp
#define RtspSession_hpp

#include <iostream>
#include "UdpServer.hpp"

using namespace std;

typedef struct SPropRecord {
public:
    ~SPropRecord() { delete[] sPropBytes; }
    
    unsigned sPropLength; // in bytes
    unsigned char* sPropBytes;
}SPropRecord;

class RtspSession{
    
public:
    explicit RtspSession(const string _sessionId):sessionId(_sessionId),rtpUdpServer(nullptr),rtcpUdpServer(nullptr),
    clientRtpPort(0),clientRtcpPort(0),serverRtpPort(0),serverRtcpPort(0){}
    ~RtspSession(){}
    
    void start();
    void close();
    string& getSessionId(){return sessionId;}
    void setTransport(const string& transport_);
    void setBody(const string& body_);
    bool setRTPInfo(const char* paramsStr);
    
private:
    
     int timeout;
     int clientRtpPort;
     int clientRtcpPort;
     int serverRtpPort;
     int serverRtcpPort;
    
    string sessionId;
    string transport;
    string startDate;
    string rtpInfo;
    string sdp;
    string url;
    
    string source;
    string dest;
    
    string mline;
    string video_codec_name = "";
    string audio_codec_name = "";
    string prop_parameter_sets;
    string profile_level_id;
    string packetization_mode;
    
    BoostUdpServer* rtpUdpServer;
    BoostUdpServer* rtcpUdpServer;
    BoostUdpClient* rtcpUdpClient;
    
    thread* rtpThread;
    thread* rtcpThread;
    
    thread* rtcpServerThread;

    string protocolName;
    unsigned payloadFormat;
    unsigned short fClientPortNum;
    
    u_int16_t seqNum;
    u_int32_t timestamp;
    
    u_int16_t size_length = 0;
    u_int16_t index_length = 0;
    u_int16_t index_delta_length = 0;
    unsigned config;
    
    unique_ptr<FFmpegTools> decoder;
    unique_ptr<H264RtpPacket> video_parser;
    unique_ptr<AACRtpPacket> audio_parser;
    
private:
    bool parseSDPAttribute_fmtp(char const* sdpLine);
    bool parseSDPAttribute_rtpmap(char const* sdpLine, const string& mline);
    SPropRecord* parseSPropParameterSets(char const* sPropParameterSetsStr,
                                                      unsigned& numSPropRecords) ;
    unsigned sampling_frequency_from_audio_specific_config(char const* configStr);
};


#endif /* RtspSession_hpp */
