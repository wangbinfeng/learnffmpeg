//
//  RtspRequest.cpp
//  testboost
//
//  Created by 王斌峰 on 2018/9/30.
//  Copyright © 2018年 Binfeng. All rights reserved.
//

#include <string>
#include <iostream>
#include <sstream>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#include "RtspRequest.hpp"

using namespace std;

string RtspRequest::to_string()
{
        stringstream ss;
        
        ss << method << " " << url << " " << version << "\r\n"
        << "CSeq: "<< cseq << "\r\n"
        << "User-Agent: " << userAgent << "\r\n";
        
        if(!accept.empty())
            ss << "Accept: " << accept << "\r\n";
        
        if(!transport.empty())
            ss << "Transport: " << transport << "\r\n";
        
        if(!session.empty())
            ss << "Session: " << session << "\r\n";
        
        if(!range.empty())
            ss << "Range: " << range << "\r\n";
        
        ss << "\r\n";
        
        if(!contentType.empty())
            ss << "Content-Type: " << contentType << "\r\n";
        
        if(contentLength > 0)
            ss << "Content-Length: " << contentLength << "\r\n";
        
        if(!body.empty())
            ss << body << "\r\n";
        
        return ss.str();
}


string RtspResponse::to_string()
{
    stringstream ss;
    
    ss << "Response: " << response << "\r\n"
    << "Status: "<< status << "\r\n"
    << "Cseq: " << cseq << "\r\n"
    << "Date: " << respDate << "\r\n";
    
    if(!session.empty())
        ss << "Session: " << session << "\r\n";
    
    if(!range.empty())
        ss << "Range: " << range << "\r\n";
    
    if(!respPublic.empty())
        ss << "Public: " << respPublic << "\r\n";
    
    if(!contentBase.empty())
        ss << "Content-Base: " << contentBase << "\r\n";
    
    if(!contentType.empty())
        ss << "Content-Type: " << contentBase << "\r\n";
    
    if(contentLength > 0)
        ss << "Content-Length: " << contentLength << "\r\n";
    
    
    if(!transport.empty())
        ss << "Transport: " << transport << "\r\n";
    
    if(!rtpInfo.empty())
        ss << "RTP-Info: " << rtpInfo << "\r\n";
    
    return ss.str();
}


