//
//  RtspRequest.hpp
//  testboost
//
//  Created by 王斌峰 on 2018/9/30.
//  Copyright © 2018年 Binfeng. All rights reserved.
//

#include <string>

#ifndef RtspRequest_hpp
#define RtspRequest_hpp


using namespace std;

class RtspRequest{
    
public:
    RtspRequest():version("RTSP/1.0"),method(""),cseq(""),url(""),userAgent("mytest 1.0"),
    transport(""),session(""),range(""),accept(""),
    contentType(""),contentLength(0),body("")
    {
        
    }
    RtspRequest(const string& _url,const string& _method):
    version("RTSP/1.0"),method(_method),cseq(""),url(_url),userAgent("mytest 1.0"),
    transport(""),session(""),range(""),accept(""),
    contentType(""),contentLength(0),body("")
    {
        
    }
    
    string getVersion() {
        return version;
    }
    void setVersion(string version) {
        this->version = version;
    }
    
    string getMethod() {
        return method;
    }
    void setMethod(string method) {
        this->method = method;
    }
    string getCseq() {
        return cseq;
    }
    void setCseq(string cseq) {
        this->cseq = cseq;
    }
    string getUrl() {
        return url;
    }
    void setUrl(string url) {
        this->url = url;
    }
    string getUserAgent() {
        return userAgent;
    }
    void setUserAgent(string userAgent) {
        this->userAgent = userAgent;
    }
    
    string getTransport() {
        return transport;
    }
    void setTransport(string transport) {
        this->transport = transport;
    }
    
    string getSession() {
        return session;
    }
    void setSession(string session) {
        this->session = session;
    }
    string getRange() {
        return range;
    }
    void setRange(string range) {
        this->range = range;
    }
    
    string getAccept() {
        return accept;
    }
    void setAccept(string accept) {
        this->accept = accept;
    }
    
    string to_string();
    
    
private:
    string version ;
    string method;
    string cseq;
    string url;
    string userAgent;
    string transport ;
    string session;
    string range;
    string accept;
    int contentLength;
    string body;
    string contentType;
    
};


class RtspOptionsRequest: public RtspRequest{
public:
    explicit RtspOptionsRequest(const string& _url):RtspRequest(_url,"OPTIONS")
    {
        // setMethod("OPTIONS");
        // setUrl(_url);
    }
};

class RtspDescribeRequest: public RtspRequest{
public:
    explicit RtspDescribeRequest(const string& _url):RtspRequest(_url,"DESCRIBE")
    {
        setAccept("application/sdp");
    }
};

class RtspSetupRequest: public RtspRequest{
public:
    explicit RtspSetupRequest(const string& _url):RtspRequest(_url,"SETUP")
    {
        setTransport( "RTP/AVP;unicast;client_port=54006-54007");
    }
};

class RtspTeardownRequest: public RtspRequest{
public:
    explicit RtspTeardownRequest(const string& _url):RtspRequest(_url,"TEARDOWN")
    {
        
    }
};

class RtspPlayRequest: public RtspRequest{
public:
    explicit RtspPlayRequest(const string& _url):RtspRequest(_url,"PLAY")
    {
        setRange("npt=0.000-");
    }
};


typedef struct RtspResponse{
    
    string response;
    unsigned int status = 0;
    string cseq;
    string respDate;
    string respPublic;
    string contentBase;
    string contentType;
    unsigned int contentLength = 0;
    string transport;
    string session;
    string range;
    string rtpInfo;
    string body;
    
    string to_string();
    
} RtspResponse;

enum class NotiType {
    UNKNOWN,
    OPTIONS ,
    DESCRIBE ,
    SETUP ,
    PLAY ,
    TEARDOWN ,
    GET_PARAMETER,
    SET_PARAMETER,
    PAUSE ,
    RECORD ,
    REDIRECT
};




#endif /* RtspRequest_hpp */
