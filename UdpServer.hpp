//
//  UdpServer.hpp
//  testboost
//
//  Created by 王斌峰 on 2018/10/3.
//  Copyright © 2018年 Binfeng. All rights reserved.
//

#ifndef UdpServer_hpp
#define UdpServer_hpp

#include <cstdint>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include "RtpPacket.hpp"

class BoostUdpServer{
public:
    
    explicit BoostUdpServer( int _port):port(_port),formatType(0){}
    explicit BoostUdpServer( int _port, int type):port(_port),formatType(type){}
    
    ~BoostUdpServer(){}
    void run();
    void stop();
    void set_packet_parser(RtpPacket* _parser){ rtp_parsers.push_back(_parser);}
    
private:
    void handle_receive(const boost::system::error_code& error,
                        std::size_t bytes_transferred);
    const int buffer_len = 8090;
private:
    int port;
    int formatType;
    std::atomic_bool stopped;
    
    uint8_t *recv_buffer_;
    boost::asio::ip::udp::endpoint remote_endpoint_;
    boost::asio::ip::udp::socket *socket_;
    boost::asio::io_context* io_context;
    
    std::vector<RtpPacket*> rtp_parsers;
};

class BoostUdpClient{
public:
    
    BoostUdpClient(const std::string& _ip,const int& _port,const int& _src_port):dest_ip(_ip),dest_port(_port),src_port(_src_port),formatType(0)
    ,timer(nullptr){};
     ~BoostUdpClient(){}
    
    void start();
    int send(const boost::system::error_code& error );
    void close();
    
private:
    std::string dest_ip;
    unsigned int dest_port;
    unsigned int src_port;
    int formatType;
    boost::asio::ip::udp::socket *socket_;
    boost::asio::io_context io_context;
    boost::asio::steady_timer *timer;
};






#endif /* UdpServer_hpp */
