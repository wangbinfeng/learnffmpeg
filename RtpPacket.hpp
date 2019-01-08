//
//  RtpPacket.hpp
//  testboost
//
//  Created by 王斌峰 on 2018/10/3.
//  Copyright © 2018年 Binfeng. All rights reserved.
//

#ifndef RtpPacket_hpp
#define RtpPacket_hpp

#include <stdio.h>
#include <cstdint>
#include <cstring>
#include "rtp.h"
#include "ffmpegtools.hpp"

class RtpPacket{
public:
    RtpPacket() noexcept{
        init();
    }
    virtual ~RtpPacket(){};
    
    int parse_header(char *buf);
    int parse_control(char *buf, int len);
    char *rtp_read_sdes(char *b, int len);
    void member_sdes(uint32_t m, rtcp_sdes_type_t t, char *b, int len);
    
    virtual int parse_data( uint8_t* buf, const int size){ return 0;}
    void set_decoder(FFmpegTools* _decoder){decoder = _decoder;}

protected:
    inline void hex(const char *buf, const int len);
    inline std::string to_hex(const uint8_t* const buff, int len);
    FFmpegTools* decoder = nullptr;
    int buffer_len = 8090;
private:
   
    void init();
    
    PT_MAP pt_map[256];
    
};

class H264RtpPacket : public RtpPacket{
    using RtpPacket::RtpPacket;
    
public:
    H264RtpPacket() noexcept;
    ~H264RtpPacket();
    void set_seq_no(const int& seq){seq_no = seq;}
    virtual int parse_data( uint8_t* buf, int size) override final;
    void set_sps_pps(const uint8_t* _sps = NULL, const int _sps_size = 0,
                     const uint8_t* _pps = NULL, const int _pps_size = 0);
    
private:
    int buffer_len = 8090;
    
    const uint8_t nalu_header[4] = { 0x00, 0x00, 0x00, 0x01};
    const uint8_t ox41[1] ={ 0x41};
    const uint8_t ox65[1] ={ 0x65};
    
    uint8_t *h264_buf = nullptr;
    uint8_t *h264_sps_pps = nullptr;
    int pps_size = {}, sps_size = {};
    int h264_buf_inx = {};
    int seq_no = {};
    bool first_frame = true;
    bool sps = false,pps = false;
    
private:
    PT_MAP pt_map[256];
    
};


typedef struct PutBitContext {
    uint32_t bit_buf;
    int bit_left;
    uint8_t *buf, *buf_ptr, *buf_end;
    int size_in_bits;
} PutBitContext;



class AACRtpPacket : public RtpPacket {
     using RtpPacket::RtpPacket;
public:
    explicit AACRtpPacket(u_int16_t size_length_ = 0,
                          u_int16_t index_length_ = 0,
                          u_int16_t index_delta_length_ = 0,
                          signed sample_rate_ = 44100) noexcept ;
    
    ~AACRtpPacket();
    virtual int parse_data(uint8_t* buf, int size) override final;
private:
    const unsigned ADTS_HEADER_SIZE = 7 ;
    int buffer_len = 8090;
    
    u_int16_t size_length = 0;
    u_int16_t index_length = 0;
    u_int16_t index_delta_length = 0;
    signed sample_rate = 44100;
    int seq_no = {};
    
    typedef struct AUHeader {
        unsigned size;
        unsigned index; // indexDelta for the 2nd & subsequent headers
    } AUHeader;
    
   
    typedef struct ADTS_Header{
        unsigned short syncword:12;
        unsigned char ID:1;
        unsigned char layer:2;
        unsigned char protection_absent:1;
        
        unsigned char profile:2;
        unsigned char sampling_Frequency_Index:4;
        unsigned char private_bit:1;
        unsigned char channel_configuration:3;
        unsigned char original_copy:1;
        unsigned char home:1;
        unsigned char copyrighted_id:1;
        unsigned char copyright_id_start:1;
        
        unsigned short frame_length:13;
        
        unsigned short buffer_fullness:11;
        unsigned char num_of_frames:2;
    } ADTS_Header;
    
    ADTS_Header adts_header;

    void add_adts_header(unsigned char *p, int len);
    void add_latm_header(uint8_t *p, int len);
    
    AUHeader* au_headers = nullptr;
    uint8_t *aac_buf = nullptr;
    
    inline void init_put_bits(PutBitContext *s, uint8_t *buffer, int buffer_size);
    inline void put_bits(PutBitContext *s, int n, unsigned int value);
    inline void flush_put_bits(PutBitContext *s);
    int add_adts_header(uint8_t *buf, int size, int pce_size);
};




#endif /* RtpPacket_hpp */
