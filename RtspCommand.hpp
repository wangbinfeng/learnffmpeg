//
//  RtspCommand.hpp
//  testboost
//
//  Created by 王斌峰 on 2018/9/30.
//  Copyright © 2018年 Binfeng. All rights reserved.
//

#ifndef RtspCommand_hpp
#define RtspCommand_hpp

#include <memory>
#include <boost/asio.hpp>
#include "RtspRequest.hpp"


class CommandExecutor {
public:
    virtual void send(RtspRequest* request,RtspResponse* response) = 0;
    virtual ~CommandExecutor(){};
};


class MediaCommand {
public:
    virtual void run() = 0;
};

class RtspCommand : public MediaCommand {
public:
    RtspCommand(RtspRequest* _request, RtspResponse* _response,CommandExecutor* _executor):
    rtspRequest(_request), rtspResponse(_response),exec(_executor){
    }
    
    virtual void run() override{
        exec->send(rtspRequest,rtspResponse);
    }
private:
    RtspRequest* rtspRequest;
    RtspResponse* rtspResponse;
    CommandExecutor* exec;
};

class BoostCommandExecutor : public CommandExecutor {
public:
    explicit BoostCommandExecutor(const string& _ip, const int& _port)
    :ip(_ip),port(_port),socket_(nullptr){
        init();
    }
    virtual ~BoostCommandExecutor();
    virtual void send(RtspRequest* request,RtspResponse *response);
private:
    void init();

    string ip={};
    int port={};
    boost::asio::ip::tcp::socket *socket_=nullptr;
};



#endif /* RtspCommand_hpp */
