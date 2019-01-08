//
//  RtspClient.hpp
//  testboost
//
//  Created by 王斌峰 on 2018/9/30.
//  Copyright © 2018年 Binfeng. All rights reserved.
//

#ifndef RtspClient_hpp
#define RtspClient_hpp

#include "RtspSession.hpp"
#include "RtspCommand.hpp"

class RtspClient{
public:
    RtspClient(const std::string& _ip, const int& _port, const std::string& _url):
    ip(_ip),port(_port),url(_url){};
    ~RtspClient(){}
    
    void start();
    void close();
    
private:
    std::string ip = {};
    std::string url = {};
    int port = {};
    
    RtspSession* session = nullptr;
    CommandExecutor* exec = nullptr;
};

#endif /* RtspClient_hpp */
