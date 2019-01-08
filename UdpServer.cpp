//
//  UdpServer.cpp
//  testboost
//
//  Created by 王斌峰 on 2018/10/3.
//  Copyright © 2018年 Binfeng. All rights reserved.
//

#include <iostream>
#include <exception>
#include <memory>
#include <cstdint>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/array.hpp>
#include <boost/bind.hpp>

#include "UdpServer.hpp"

using namespace std;

void BoostUdpServer::run(){
    try{
        using namespace boost::asio::ip;
        
        io_context = new boost::asio::io_context();
        //udp::endpoint endpoint(udp::v4(), port);
        cout << "start udp server, port " << port << endl;
        socket_ = new udp::socket(*io_context);
        
        udp::endpoint local_add(address::from_string("0.0.0.0"), port);
        socket_->open(local_add.protocol());
        socket_->set_option(boost::asio::ip::udp::socket::reuse_address(true));
        socket_->bind(local_add);
       
        recv_buffer_ = new uint8_t[buffer_len];
        memset(recv_buffer_,'\0',buffer_len);
        stopped.store(false);
        
        socket_->async_receive_from(
                                    boost::asio::buffer(recv_buffer_,buffer_len),
                                    remote_endpoint_,
                                    boost::bind(&BoostUdpServer::handle_receive, this,
                                                boost::asio::placeholders::error,
                                                boost::asio::placeholders::bytes_transferred
                                                ));
        io_context->run();
        
    }catch(exception& e){
        if(socket_ != nullptr)
            socket_->close();
        io_context->stop();
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    
}

void BoostUdpServer::stop(){
    cout << "stop udp server, port " << port << endl;
    stopped.store( true );
    if(socket_ != nullptr){
        boost::system::error_code ec;
        socket_->shutdown(boost::asio::ip::udp::socket::shutdown_both,ec);
        
        try{
            socket_->close();
        }catch(...){
            cout <<"fail to close and showdown socket ..." <<endl;
        }
    }
    io_context->stop();
    delete []recv_buffer_;
    delete io_context;
}


void BoostUdpServer::handle_receive(const boost::system::error_code& error,
                                    std::size_t bytes_transferred)
{
    using namespace boost::asio::ip;
    std::cout << "read " << bytes_transferred << " from "<< remote_endpoint_.address().to_v4()<< std::endl;
    
    if(error || bytes_transferred == 0){
        boost::system::error_code ec;
        socket_->shutdown(boost::asio::ip::udp::socket::shutdown_both,ec);
        
        try{
            socket_->close();
        }catch(boost::system::error_code& ex){
            cout <<"fail to close and showdown socket ..."<< ex.message() <<endl;
        }
        return ;
    }
    
    for (auto p = rtp_parsers.cbegin(); p != rtp_parsers.cend(); ++p) {
        int ret = (*p)->parse_data(recv_buffer_,bytes_transferred);
        if (ret >= 0 )
            break;
    }
    if(stopped.load())
        return ;
    socket_->async_receive_from(
                                boost::asio::buffer(recv_buffer_,buffer_len),
                                remote_endpoint_,
                                boost::bind(&BoostUdpServer::handle_receive, this,
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::bytes_transferred));
    
}

 void BoostUdpClient::start(){
     try{
         using namespace boost::asio::ip;
         
         //boost::asio::io_context io_context;
         //udp::endpoint endpoint(udp::v4(), port);
         cout << "start udp client , port " << dest_port << endl;
         socket_ = new udp::socket(io_context,udp::v4());
         
         timer = new boost::asio::steady_timer(io_context);
         
         timer->expires_from_now(std::chrono::seconds(5));
         timer->async_wait(boost::bind(&BoostUdpClient::send,this,boost::asio::placeholders::error));
         cout << "timer after 5 seconds...." << endl;
         io_context.run();
         
     }catch(exception& e){
         if(socket_ != nullptr)
             socket_->close();
         io_context.stop();
         std::cerr << "Exception: " << e.what() << std::endl;
     }
}

void BoostUdpClient::close(){
    if(timer != nullptr)
    {
        timer->cancel();
        timer = nullptr;
    }
    if(socket_ != nullptr)
        socket_->close();
    
    io_context.stop();
}

int BoostUdpClient::send(const boost::system::error_code& ec ){
    if (!ec) {
        //started = true;
        
    }
    else if (ec != boost::asio::error::operation_aborted) {
        std::cerr << "tid: " << std::this_thread::get_id() << ", handle_timeout error " << ec.message() << "\n";
        return 0;
    }
    
    cout << "\n\n*****************************sending rtcp packet to ("<< dest_ip << ":" <<dest_port <<") ..." << endl;
    
    using namespace boost::asio::ip;
    udp::endpoint endpoint(address::from_string(dest_ip),dest_port);
    
    int len = 0, total = 8, count = 0;
    
    rtcp_t *r = (rtcp_t *)malloc(sizeof(rtcp_t));
    
    r->common.length  = 0;
    r->common.count   = 1;
    r->common.version = RTP_VERSION;
    r->common.pt      = RTCP_RR;
    r->common.p       = 0;
    
    r->r.rr.ssrc = htonl(1234);
    
    rtcp_rr_t *rr = r->r.rr.rr;
    rr->ssrc = htonl(1234);
    rr->fraction = 0;
    rr->lost = htonl(0);
    rr->last_seq = htonl(0);
    rr->jitter = htonl(0);
    rr->lsr = htonl(0);
    rr->dlsr = htonl(0);
    
    r->common.length += sizeof(rtcp_rr_t);
    
    size_t rl = socket_->send_to(boost::asio::buffer(r,r->common.length),
                         endpoint);
    
    // Do other things here while the send completes.
    cout << "*****************************sending rtcp packet("<< rl <<") ..." << endl;
    
    timer->expires_from_now(std::chrono::seconds(5));
    timer->async_wait(boost::bind(&BoostUdpClient::send,this,boost::asio::placeholders::error));
    
    return rl; // Blocks until the send is complete. Throws any errors.

}
