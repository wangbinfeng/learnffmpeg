//
//  RtspCommand.cpp
//  testboost
//
//  Created by 王斌峰 on 2018/9/30.
//  Copyright © 2018年 Binfeng. All rights reserved.
//
#include <iostream>
#include <exception>
#include <memory>
#include <algorithm>
#include <functional>
#include <string>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/array.hpp>
#include <boost/bind.hpp>

#include "RtspCommand.hpp"
#include "RtspRequest.hpp"

using namespace std;
using boost::asio::ip::tcp;


void BoostCommandExecutor::init(){
    
    try{
        boost::asio::io_context io_context;
        tcp::endpoint endpoint(boost::asio::ip::address::from_string(ip),
                               port);
        
        socket_ = new tcp::socket(io_context);
        socket_->connect(endpoint);
        
    }catch(exception& e){
        if(socket_ != nullptr)
            socket_->close();
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    
}

BoostCommandExecutor::~BoostCommandExecutor(){
    if(socket_ != nullptr){
        cout << "close boost command executor ..."<< endl;
        socket_->close();
        delete socket_;
    }
}

void BoostCommandExecutor::send( RtspRequest* rtspRequest,RtspResponse *rtspResponse){
    
    /*
    boost::asio::io_context io_context;
        
    tcp::resolver resolver(io_context);
    tcp::endpoint endpoint(boost::asio::ip::address::from_string(ip),
                           port);
    
    tcp::socket sock(io_context);
    sock.connect(endpoint);
    
    cout << "send to [" << ip << ":" << port << "]:" << endl << msg << endl;
    
    boost::system::error_code ec;
    sock.write_some(boost::asio::buffer(msg, msg.length()),ec);
    if(ec){
        std::cout << boost::system::system_error(ec).what() << std::endl;
        sock.close();
    }
    
    char buf[1024];
    size_t len=sock.read_some(boost::asio::buffer(buf), ec);
    if(ec){
        std::cout << boost::system::system_error(ec).what() << std::endl;
        sock.close();
    }else{
        std::cout << "Reply is: ";
        std::cout.write(buf, len);
        std::cout << "\n";
    }
    sock.close();
    */
    
    const string msg = rtspRequest->to_string();
    size_t request_length = msg.length();
    boost::asio::write(*socket_, boost::asio::buffer(msg, request_length));
    
    boost::asio::streambuf response;
    boost::system::error_code error;
    
    boost::asio::read_until(*socket_,response,"\r\n",error);
    
    if (error == boost::asio::error::eof){
        cout << "Connection closed cleanly by peer." << endl;
        throw boost::system::system_error(error);
        
    }else if (error){
        cout << "Some other error." << endl;
        throw boost::system::system_error(error);
    }
    
    // Check that response is OK.
    std::istream response_stream(&response);
    std::string http_version;
    
    response_stream >> http_version;
    response_stream >> rtspResponse->status;
    
    if (!response_stream || http_version.substr(0, 5) != "RTSP/")
    {
        std::cout << "Invalid response\n";
    }else if (rtspResponse->status != 200)
    {
        std::cout << "Response returned with status code " << rtspResponse->status << endl;
    }else{
        
        boost::asio::read_until(*socket_,response,"\r\n\r\n",error);
        
        if (error == boost::asio::error::eof){
            cout << "Connection closed cleanly by peer." << endl;
            throw boost::system::system_error(error);
            
        }else if (error){
            cout << "Some other error." << endl;
            throw boost::system::system_error(error);
        }
        
        std::string header;
        vector<string> sv;
        while (std::getline(response_stream, header) && header != "\r")
        {
            std::cout << "HEADs: " << header << endl;
            sv.clear();
            boost::split(sv, header, boost::is_any_of(":"), boost::token_compress_on );
            if(sv.size()<=1)
                continue;
            
            string key = sv[0];
            string temp = boost::trim_left_copy(sv[1]);
            string value = boost::trim_right_copy(temp);
            
            if(key == "CSeq"){
                rtspResponse->cseq = value;
            }else if(key == "Date"){
                rtspResponse->respDate = value;
            }else if(key == "Public"){
                rtspResponse->respPublic = value;
            }else if(key == "Transport"){
                rtspResponse->transport = value;
            }else if(key == "Session"){
                //std::string::size_type pos = value.find(";");
                if(const auto pos = value.find(";"); pos != std::string::npos)
                    value = value.substr(0,pos);
                rtspResponse->session = value;
                
            }else if(key == "Range"){
                rtspResponse->range = value;
            }else if(key == "RTP-Info"){
                rtspResponse->rtpInfo = header;
            }else if(key == "Content-Type"){
                rtspResponse->contentType = value;
            }else if(key == "Content-Base"){
                rtspResponse->contentBase = value;
            }else if(key == "Content-Length"){
                cout << "content-length:" << value << endl;
                rtspResponse->contentLength = std::stoi(value);
            }
        }
        
        if(rtspResponse->contentLength > 0){
             cout <<endl << "body:" << endl;
            string acontrol = "a=control:";
             do{
                 boost::asio::read_until(*socket_,response,"\r\n",error);
                 if (error == boost::asio::error::eof){
                     cout << "Connection closed cleanly by peer." << endl;
                     break;
                 }else if (error){
                     cout << "Some other error." << endl;
                     break;
                 }
                 
                 std::getline(response_stream, header);
                 cout << header ;
                 rtspResponse->body += header;
                 
//                 auto it = std::search(header.begin(), header.end(),
//                                       acontrol.begin(), acontrol.end());
//                 if(it != header.end())
//                     continue;
                 
                 sv.clear();
                 boost::split(sv, header, boost::is_any_of(":"), boost::token_compress_on );
                 if(sv.size()>1)
                 {
                     string key = sv[0];
                     string temp = boost::trim_left_copy(sv[1]);
                     string value = boost::trim_right_copy(temp);

                     if(key == "a=control" && value != "*")
                         break;
                 }
             }while(1);
         }
    }
  
}

