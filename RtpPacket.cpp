#include "RtpPacket.hpp"
#include <string>
#include <netinet/in.h>

#include <BitVector.hh>

void RtpPacket::init() {
    for (int i = 0; i < 256; i++) {
        memset(pt_map[i].enc, '\0', 10);
        memcpy(pt_map[i].enc, "????", 4);
        //pt_map[i].enc = "????";
        pt_map[i].rate = 0;
        pt_map[i].ch   = 0;
    }
    /* Updated 11 May 2002 by Akira Tsukamoto with current IANA assignments: */
    /* http://www.iana.org/assignments/rtp-parameters */
    /* Marked *r* items are indicated as 'reserved' by the IANA */
    strncpy(pt_map[  0].enc , "PCMU", 4); pt_map[  0].rate =  8000; pt_map[  0].ch = 1;
    strncpy(pt_map[  1].enc,  "1016", 4); pt_map[  1].rate =  8000; pt_map[  1].ch = 1;
    strncpy(pt_map[  2].enc , "G721", 4); pt_map[  2].rate =  8000; pt_map[  2].ch = 1;
    strncpy(pt_map[  3].enc , "GSM ", 4); pt_map[  3].rate =  8000; pt_map[  3].ch = 1;
    strncpy(pt_map[  4].enc , "G723", 4); pt_map[  4].rate =  8000; pt_map[  4].ch = 1;
    strncpy(pt_map[  5].enc , "DVI4", 4); pt_map[  5].rate =  8000; pt_map[  5].ch = 1;
    strncpy(pt_map[  6].enc , "DVI4", 4); pt_map[  6].rate = 16000; pt_map[  6].ch = 1;
    strncpy(pt_map[  7].enc , "LPC ", 4); pt_map[  7].rate =  8000; pt_map[  7].ch = 1;
    strncpy(pt_map[  8].enc , "PCMA", 4); pt_map[  8].rate =  8000; pt_map[  8].ch = 1;
    strncpy(pt_map[  9].enc , "G722", 4); pt_map[  9].rate =  8000; pt_map[  9].ch = 1;
    strncpy(pt_map[ 10].enc , "L16 ", 4); pt_map[ 10].rate = 44100; pt_map[ 10].ch = 2;
    strncpy(pt_map[ 11].enc , "L16 ", 4); pt_map[ 11].rate = 44100; pt_map[ 11].ch = 1;
    strncpy(pt_map[ 12].enc , "QCELP", 5); pt_map[ 12].rate = 8000; pt_map[ 12].ch = 1;
    strncpy(pt_map[ 14].enc , "MPA ", 4); pt_map[ 14].rate = 90000; pt_map[ 14].ch = 0;
    strncpy(pt_map[ 15].enc , "G728", 4); pt_map[ 15].rate =  8000; pt_map[ 15].ch = 1;
    strncpy(pt_map[ 16].enc , "DVI4", 4); pt_map[ 16].rate = 11025; pt_map[ 16].ch = 1;
    strncpy(pt_map[ 17].enc , "DVI4", 4); pt_map[ 17].rate = 22050; pt_map[ 17].ch = 1;
    strncpy(pt_map[ 18].enc , "G729", 4); pt_map[ 18].rate =  8000; pt_map[ 18].ch = 1;
    strncpy(pt_map[ 23].enc , "SCR ", 4); pt_map[ 23].rate = 90000; pt_map[ 23].ch = 0; /*r*/
    strncpy(pt_map[ 24].enc , "MPEG", 4); pt_map[ 24].rate = 90000; pt_map[ 24].ch = 0; /*r*/
    strncpy(pt_map[ 25].enc , "CelB", 4); pt_map[ 25].rate = 90000; pt_map[ 25].ch = 0;
    strncpy(pt_map[ 26].enc , "JPEG", 4); pt_map[ 26].rate = 90000; pt_map[ 26].ch = 0;
    strncpy(pt_map[ 27].enc , "CUSM", 4); pt_map[ 27].rate = 90000; pt_map[ 27].ch = 0; /*r*/
    strncpy(pt_map[ 28].enc , "nv  ", 4); pt_map[ 28].rate = 90000; pt_map[ 28].ch = 0;
    strncpy(pt_map[ 29].enc , "PicW", 4); pt_map[ 29].rate = 90000; pt_map[ 29].ch = 0; /*r*/
    strncpy(pt_map[ 30].enc , "CPV ", 4); pt_map[ 30].rate = 90000; pt_map[ 30].ch = 0; /*r*/
    strncpy(pt_map[ 31].enc , "H261", 4); pt_map[ 31].rate = 90000; pt_map[ 31].ch = 0;
    strncpy(pt_map[ 32].enc , "MPV ", 4); pt_map[ 32].rate = 90000; pt_map[ 32].ch = 0;
    strncpy(pt_map[ 33].enc , "MP2T", 4); pt_map[ 33].rate = 90000; pt_map[ 33].ch = 0;
    strncpy(pt_map[ 34].enc , "H263", 4); pt_map[ 34].rate = 90000; pt_map[ 34].ch = 0;
    decoder = nullptr;
}
// H264RtpPacket::H264RtpPacket() noexcept :RtpPacket(){
H264RtpPacket::H264RtpPacket() noexcept {
    h264_buf = reinterpret_cast<uint8_t*>(malloc(sizeof(uint8_t) * buffer_len));
    h264_sps_pps = new uint8_t[64];
    memset(h264_buf, '\0', buffer_len);
    memset(h264_sps_pps, '\0', 64);
    h264_buf_inx = 0;
    sps_size = 0;
    pps_size = 0;
    first_frame = true;
}

H264RtpPacket::~H264RtpPacket() {
    delete[] h264_sps_pps;
    free(h264_buf);
}

AACRtpPacket::AACRtpPacket(u_int16_t size_length_ ,
             u_int16_t index_length_,
             u_int16_t index_delta_length_, signed sample_rate_) noexcept {
    size_length = size_length_;
    index_length = index_length_;
    index_delta_length = index_delta_length_;
    sample_rate = sample_rate_;
    aac_buf = reinterpret_cast<uint8_t*>(malloc(sizeof(uint8_t) * buffer_len));
    memset(aac_buf, '\0', buffer_len);
}

AACRtpPacket::~AACRtpPacket() {
    if(au_headers)
        delete[] au_headers;
    free(aac_buf);
}

inline void RtpPacket::hex(const char *buf, int len) {
    for (int i = 0; i < len; i++) {
        printf("%02x", (unsigned char)buf[i]);
    }
}

/*
 * Return header length of RTP packet contained in 'buf'.
 */
int RtpPacket::parse_header(char *buf) {
    rtp_hdr_t *r = (rtp_hdr_t *)buf;
    rtp_hdr_ext_t *ext;
    int ext_len;
    int hlen = 0;

    if (r->version == 2) {
        hlen = 12 + r->cc * 4;

        if (r->x) {  /* header extension */
            ext = (rtp_hdr_ext_t *)((char *)buf + hlen);
            ext_len = ntohs(ext->len);
            hlen += 4 + (ext_len * 4);
        }
    }

    printf("v=%d p=%d x=%d cc=%d m=%d pt=%d (%s,%d,%d) seq=%u ts=%lu ssrc=0x%lx ",
           r->version, r->p, r->x, r->cc, r->m,
           r->pt, pt_map[r->pt].enc, pt_map[r->pt].ch, pt_map[r->pt].rate,
           ntohs(r->seq),
           (unsigned long)ntohl(r->ts),
           (unsigned long)ntohl(r->ssrc));
    for (int i = 0; i < r->cc; i++) {
        printf("csrc[%d]=0x%0lx ", i, r->csrc[i]);
    }
    if (r->x) {  /* header extension */
        ext = (rtp_hdr_ext_t *)((char *)buf + hlen);
        ext_len = ntohs(ext->len);

        printf("ext_type=0x%x ", ntohs(ext->ext_type));
        printf("ext_len=%d ", ext_len);

        if (ext_len) {
            printf("ext_data=");
            hex((char *)(ext+1), (ext_len*4));
            printf(" ");
        }
    }

    printf("\n");
    
    return hlen;
} /* parse_header */

/*
 * Show SDES information for one member.
 */
void RtpPacket::member_sdes(member_t m, rtcp_sdes_type_t t, char *b, int len) {
    static struct {
        rtcp_sdes_type_t t;
        const char *name;
    } map[] = {
        {RTCP_SDES_END,    "end"},
        {RTCP_SDES_CNAME,  "CNAME"},
        {RTCP_SDES_NAME,   "NAME"},
        {RTCP_SDES_EMAIL,  "EMAIL"},
        {RTCP_SDES_PHONE,  "PHONE"},
        {RTCP_SDES_LOC,    "LOC"},
        {RTCP_SDES_TOOL,   "TOOL"},
        {RTCP_SDES_NOTE,   "NOTE"},
        {RTCP_SDES_PRIV,   "PRIV"},
        {RTCP_SOURCE,      "SOURCE"},
        {RTCP_NONE,0}
    };
    int i;
    char num[10];
    
    sprintf(num, "%d", t);
    for (i = 0; map[i].name; i++) {
        if (map[i].t == t) break;
    }
    printf("%s=\"%*.*s\" ",
            map[i].name ? map[i].name : num, len, len, b);
} /* member_sdes */

/*
 * Parse one SDES chunk (one SRC description). Total length is 'len'.
 * Return new buffer position or zero if error.
 */
char *RtpPacket::rtp_read_sdes(char *b, int len) {
    rtcp_sdes_item_t *rsp;
    uint32_t src = *(uint32_t *)b;
    int total_len = 0;
    
    len -= 4;  // subtract SSRC from available bytes
    if (len <= 0) {
        return 0;
    }
    rsp = (rtcp_sdes_item_t *)(b + 4);
    for (; rsp->type; rsp = (rtcp_sdes_item_t *)((char *)rsp + rsp->length + 2)) {
        
        rtcp_sdes_type_t t = RTCP_NONE;
        switch (rsp->type) {
                case 0: t = RTCP_SDES_END; break;
                case 1: t = RTCP_SDES_CNAME; break;
                case 2: t = RTCP_SDES_NAME; break;
                case 3: t = RTCP_SDES_EMAIL; break;
                case 4: t = RTCP_SDES_PHONE; break;
                case 5: t = RTCP_SDES_LOC; break;
                case 6: t = RTCP_SDES_TOOL; break;
                case 7: t = RTCP_SDES_NOTE; break;
                case 8: t = RTCP_SDES_PRIV; break;
                case 11: t=RTCP_SOURCE; break;
        }
        member_sdes(src, t, rsp->data, rsp->length);
        total_len += rsp->length + 2;
    }
    if (total_len >= len) {
        fprintf(stderr,
                "Remaining length of %d bytes for SSRC item too short (has %u bytes)\n",
                len, total_len);
        return 0;
    }
    b = (char *)rsp + 1;
    // skip padding //
    return b + ((4 - ((int)(*b) & 0x3)) & 0x3);
} // rtp_read_sdes

int RtpPacket::parse_control(char *buf, int len) {
    rtcp_t *r;         // RTCP header
    int i;
    
    r = (rtcp_t *)buf;
    if (r->common.version == 2) {
        printf( "\n");
        while (len > 0) {
            len -= (ntohs(r->common.length) + 1) << 2;
            if (len < 0) {
                // something wrong with packet format
                printf( "Illegal RTCP packet length %d words.\n",
                        ntohs(r->common.length));
                return -1;
            }
            
            switch (r->common.pt) {
                case RTCP_SR:
                    printf( " (SR ssrc=0x%lx p=%d count=%d len=%d\n",
                            (unsigned long)ntohl(r->r.rr.ssrc),
                            r->common.p, r->common.count,
                            ntohs(r->common.length));
                    printf( "  ntp=%lu.%lu ts=%lu psent=%lu osent=%lu\n",
                            (unsigned long)ntohl(r->r.sr.ntp_sec),
                            (unsigned long)ntohl(r->r.sr.ntp_frac),
                            (unsigned long)ntohl(r->r.sr.rtp_ts),
                            (unsigned long)ntohl(r->r.sr.psent),
                            (unsigned long)ntohl(r->r.sr.osent));
                    for (i = 0; i < r->common.count; i++) {
                        printf( "  (ssrc=0x%lx fraction=%g lost=%lu last_seq=%lu jit=%lu lsr=%lu dlsr=%lu )\n",
                                (unsigned long)ntohl(r->r.sr.rr[i].ssrc),
                                r->r.sr.rr[i].fraction / 256.,
                                (unsigned long)ntohl(r->r.sr.rr[i].lost), // XXX I'm pretty sure this is wrong
                                (unsigned long)ntohl(r->r.sr.rr[i].last_seq),
                                (unsigned long)ntohl(r->r.sr.rr[i].jitter),
                                (unsigned long)ntohl(r->r.sr.rr[i].lsr),
                                (unsigned long)ntohl(r->r.sr.rr[i].dlsr));
                    }
                    printf( " )\n");
                    break;

                case RTCP_RR:
                    printf( " (RR ssrc=0x%lx p=%d count=%d len=%d\n",
                            (unsigned long)ntohl(r->r.rr.ssrc), r->common.p, r->common.count,
                            ntohs(r->common.length));
                    for (i = 0; i < r->common.count; i++) {
                        printf( "  (ssrc=0x%lx fraction=%g lost=%lu last_seq=%lu jit=%lu lsr=%lu dlsr=%lu )\n",
                                (unsigned long)ntohl(r->r.rr.rr[i].ssrc),
                                r->r.rr.rr[i].fraction / 256.,
                                (unsigned long)ntohl(r->r.rr.rr[i].lost),
                                (unsigned long)ntohl(r->r.rr.rr[i].last_seq),
                                (unsigned long)ntohl(r->r.rr.rr[i].jitter),
                                (unsigned long)ntohl(r->r.rr.rr[i].lsr),
                                (unsigned long)ntohl(r->r.rr.rr[i].dlsr));
                    }
                    printf( " )\n");
                    break;

                case RTCP_SDES:
                    printf( " (SDES p=%d count=%d len=%d\n",
                            r->common.p, r->common.count, ntohs(r->common.length));
                    buf = (char *)&r->r.sdes;
                    for (i = 0; i < r->common.count; i++) {
                        int remaining = (ntohs(r->common.length) << 2) -
                        (buf - (char *)&r->r.sdes);
                        
                        printf( "(src=0x%lx ",
                        (unsigned long)ntohl(((struct rtcp_sdes *)buf)->src));
                        if (remaining > 0) {
                            buf = rtp_read_sdes(buf,
                                                (ntohs(r->common.length) << 2) - (buf - (char *)&r->r.sdes));
                            if (!buf) return -1;
                        }
                        else {
                            fprintf(stderr, "Missing at least %d bytes.\n", -remaining);
                            return -1;
                        }
                        printf( ")\n");
                    }
                    printf( " )\n");
                    break;

                case RTCP_BYE:
                    printf( " (BYE p=%d count=%d len=%d\n",
                            r->common.p, r->common.count, ntohs(r->common.length));
                    for (i = 0; i < r->common.count; i++) {
                        printf( "  (ssrc[%d]=0x%0lx ", i,
                                (unsigned long)ntohl(r->r.bye.src[i]));
                    }
                    printf( ")\n");
                    if (ntohs(r->common.length) > r->common.count) {
                        buf = (char *)&r->r.bye.src[r->common.count];
                        printf( "reason=\"%*.*s\"", *buf, *buf, buf+1);
                    }
                    printf( " )\n");
                    break;
                    
                    // invalid type
                default:
                    printf( "(? pt=%d src=0x%lx)\n", r->common.pt,
                            (unsigned long)ntohl(r->r.sdes.src));
                    break;
            }
            r = (rtcp_t *)((uint32_t *)r + ntohs(r->common.length) + 1);
        }
    }
    else {
        printf( "invalid version %d\n", r->common.version);
    }
    return len;
} /* parse_control */

inline std::string RtpPacket::to_hex(const uint8_t* const buff, int len) {
    static char const hex_chars[16] = { '0', '1', '2', '3', '4', '5',
        '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    std::string h;
    for ( int i = 0; i < len; ++i )
    {
        char const byte = buff[i];
        h += hex_chars[ ( byte & 0xF0 ) >> 4 ];
        h += hex_chars[ ( byte & 0x0F ) >> 0 ];
    }
    return h;
};


void H264RtpPacket::set_sps_pps(const uint8_t* _sps, const int _sps_size,
                 const uint8_t* _pps, const int _pps_size) {
    
    if ( !_sps || !_pps || _sps_size < 1 || _pps_size < 1 ) {
        fprintf(stderr, "sps/pps is not valid\n");
        return ;
    }
    std::cout << "sps:" << to_hex(_sps, _sps_size) << std::endl;
    std::cout << "pps:" << to_hex(_pps, _pps_size) << std::endl;
    sps_size = _sps_size;
    pps_size = _pps_size;
    
    memcpy(h264_sps_pps, _sps, sps_size);
    memcpy(h264_sps_pps+sps_size, _pps, pps_size);
    sps = false;
    pps = false;
}

int H264RtpPacket::parse_data( uint8_t* recv_buffer_, int size) {
    using namespace std;
    cout << "receive buf size:" << size << endl;
    if(size < 1)
        return -1;
    
    rtp_hdr_t *r = (rtp_hdr_t *)recv_buffer_;
    rtp_hdr_ext_t *ext;
    int ext_len = 0;
    int hlen = 0;
    
    if ( r->version == 2 ) {
        hlen = 12 + r->cc * 4;
        
        if (r->x) {  /* header extension */
            ext = (rtp_hdr_ext_t *)((char *)recv_buffer_ + hlen);
            ext_len = ntohs(ext->len);
            hlen += 4 + (ext_len * 4);
        }
    }
    
    printf("\nv=%d p=%d x=%d cc=%d m=%d pt=%d  seq=%u(cur:%u) ts=%lu ssrc=0x%lx ",
           r->version, r->p, r->x, r->cc, r->m,
           r->pt,
           ntohs(r->seq),seq_no,
           (unsigned long)ntohl(r->ts),
           (unsigned long)ntohl(r->ssrc));
    
    for (int i = 0; i < r->cc; i++) {
        printf("csrc[%d]=0x%0lx ", i, r->csrc[i]);
    }
    if ( r->x ) {  /* header extension */
        ext = (rtp_hdr_ext_t *)((char *)recv_buffer_ + hlen);
        ext_len = ntohs(ext->len);

        printf("ext_type=0x%x ", ntohs(ext->ext_type));
        printf("ext_len=%d ", ext_len);

        if (ext_len) {
            printf("ext_data=");
            hex((char *)(ext+1), (ext_len*4));
            printf(" ");
        }
    }
    printf("\n");

    // std::cout << to_hex(recv_buffer_, size) << endl;
    int this_seq = ntohs(r->seq);
    printf("first is %02x\n", (unsigned char)recv_buffer_[hlen]);

    if ( recv_buffer_[hlen] == 0x67 ) {
        cout << "This is sps" << endl;
        sps = true;
        seq_no = this_seq;
        memcpy(h264_buf, nalu_header, 4);
        memcpy(h264_buf+4, recv_buffer_+hlen, size-hlen);
        decoder->decode_video_rtp(h264_buf, size-hlen+4);
        return 0;
    } else if ( recv_buffer_[hlen] == 0x68 ) {
        pps = true;
        cout << "This is pps" << endl;
        seq_no = this_seq;
        memcpy(h264_buf, nalu_header, 4);
        memcpy(h264_buf+4, recv_buffer_+hlen, size-hlen);
        decoder->decode_video_rtp(h264_buf, size-hlen+4);
        return 0;
    }

    if ( !pps || !sps ){
         cout << "pps or sps is not ready " << endl;
        return -1;
    }else
        cout << "pps or sps is ready, last seq_num is "<< seq_no << endl;

    if ( this_seq <= seq_no ) {
        cout << "ignore this package due to seq:"<< this_seq << " <= last seq:" << seq_no << endl;
        return -1;
    }
    seq_no = this_seq;

    if ( r->m == 0 ) {
        cout << "receive buf size:" <<size  << ", h264_buf_inx:"
        << h264_buf_inx << ", buffer_len:" << buffer_len <<", first frame ? "<< first_frame << endl;
        
        if ( h264_buf_inx == 0 ) {
            memcpy(h264_buf, nalu_header, 4);
            h264_buf_inx += 4;
            
            printf("hlen=%d,first two are %02x%02x\n", hlen,
                   (unsigned char)recv_buffer_[hlen],  (unsigned char)recv_buffer_[hlen+1]) ;
            
            if ( (recv_buffer_[hlen] == 0x5c) &&
                (recv_buffer_[hlen+1] == 0x81) )
                memcpy(h264_buf+h264_buf_inx, ox41, 1);
            else if ( recv_buffer_[hlen] == 0x7c &&
                     recv_buffer_[hlen+1] == 0x85 )
                memcpy(h264_buf+h264_buf_inx, ox65, 1);
            
            h264_buf_inx += 1;
        }
        hlen += 2;
        int cur_len = h264_buf_inx + size - hlen ;
        if ( cur_len > buffer_len ) {
            cout << "enlarge h264_buf due to len("<< buffer_len <<") is less than "<< cur_len << endl;
            buffer_len = cur_len + buffer_len;
            h264_buf = (uint8_t*)realloc(h264_buf, buffer_len);
        }
        memcpy(h264_buf+h264_buf_inx, recv_buffer_+hlen, size-hlen);
        h264_buf_inx += size - hlen;
    } else if ( r->m > 0 ) {
        hlen += 2;
        cout << "receive buf size:" << size  << ", h264_buf_inx:" << h264_buf_inx << ", buffer_len:" << buffer_len <<endl;
        int cur_len = h264_buf_inx + size - hlen;
        if ( cur_len > buffer_len ) {
            cout << "enlarge h264_buf due to len("<< buffer_len <<") is less than "<< cur_len << endl;
            buffer_len = cur_len + buffer_len;
            h264_buf = (uint8_t*)realloc(h264_buf, buffer_len);
        }
        
        memcpy(h264_buf+h264_buf_inx, recv_buffer_+hlen, size-hlen);
        h264_buf_inx += size - hlen;
        cout << "EOF,final h264_buf_inx:"<< h264_buf_inx << endl;
        cout << "payload:[" << to_hex(h264_buf, h264_buf_inx) << "]"<< endl;
        
        decoder->decode_video_rtp(h264_buf, h264_buf_inx);
        memset(h264_buf, '\0', buffer_len);
        h264_buf_inx = 0;
    }
    
    return 0;
    
}

int AACRtpPacket::parse_data(uint8_t* recv_buffer_, int size) {
    using namespace std;
    cout << "\nAAC : receive buf size:" << size << endl;
    if(size < 1)
        return -1;
    
    rtp_hdr_t *r = (rtp_hdr_t *)recv_buffer_;
    rtp_hdr_ext_t *ext;
    int ext_len = 0;
    int hlen = 0;
    
    if ( r->version == 2 ) {
        hlen = 12 + r->cc * 4;
        
        if (r->x) {  /* header extension */
            ext = (rtp_hdr_ext_t *)((char *)recv_buffer_ + hlen);
            ext_len = ntohs(ext->len);
            hlen += 4 + (ext_len * 4);
        }
    }
    
    printf("v=%d p=%d x=%d cc=%d m=%d pt=%d  seq=%u ts=%lu ssrc=0x%lx ",
           r->version, r->p, r->x, r->cc, r->m,
           r->pt,
           ntohs(r->seq),
           (unsigned long)ntohl(r->ts),
           (unsigned long)ntohl(r->ssrc));
    
    for (int i = 0; i < r->cc; i++) {
        printf("csrc[%d]=0x%0lx ", i, r->csrc[i]);
    }
    if ( r->x ) {  /* header extension */
        ext = (rtp_hdr_ext_t *)((char *)recv_buffer_ + hlen);
        ext_len = ntohs(ext->len);
        
        printf("ext_type=0x%x ", ntohs(ext->ext_type));
        printf("ext_len=%d ", ext_len);
        
        if (ext_len) {
            printf("ext_data=");
            hex((char *)(ext+1), (ext_len*4));
            printf(" ");
        }
    }
    printf("\n");
    
    // std::cout << to_hex(recv_buffer_, size) << endl;
    int this_seq = ntohs(r->seq);
    printf("first is %02x\n", (unsigned char)recv_buffer_[hlen]);
    
    if ( this_seq <= seq_no ) {
        cout << "ignore this package due to seq:"<< this_seq << " <= last seq:" << seq_no << endl;
        return -1;
    }
    seq_no = this_seq;
    
    unsigned au_headers_num = 0;
    unsigned resultSpecialHeaderSize = 0;
    if (size_length > 0) {
        // The packet begins with a "AU Header Section".  Parse it, to
        // determine the "AU-header"s for each frame present in this packet:
        unsigned packetSize = size - hlen;
        
        resultSpecialHeaderSize += 2;
        if (packetSize < resultSpecialHeaderSize)
            return -1;

        /*
         AAC的另外一种RTP打包格式是mpeg4-generic，定义在RFC 3640。
         SDP中几个参数含义：config，就是AudioSpecificConfig的十六进制表示；sizeLength=13; indexLength=3，这是每个rtp包头都是固定的。
         每个RTP包的载荷，最前面2个字节一般是0x00 10，
         这是 AU-headers-length，表示AU header的长度是16个比特也就是2个字节。
         后面2个字节，高13位是AAC帧的长度，低3位为0。
        */
        unsigned AU_headers_length = (recv_buffer_[hlen]<<8)|recv_buffer_[hlen+1];
        unsigned AU_headers_length_bytes = (AU_headers_length+7)/8;
        
        cout << "AU_headers_length:"<<AU_headers_length << ",AU_headers_length_bytes:" <<  AU_headers_length_bytes<< endl;
        
        if (packetSize < resultSpecialHeaderSize + AU_headers_length_bytes)
            return -1;
        
        resultSpecialHeaderSize += AU_headers_length_bytes;
        
        // Figure out how many AU-headers are present in the packet:
        int bitsAvail = AU_headers_length - (size_length + index_length);
        if (bitsAvail >= 0 && (size_length + index_delta_length) > 0) {
            au_headers_num = 1 + bitsAvail/(size_length + index_delta_length);
        }
        cout << "au_headers_num : " << au_headers_num << endl;
        if (au_headers_num > 0) {
            au_headers = new AUHeader[au_headers_num];
            // Fill in each header:
            BitVector bv(&recv_buffer_[hlen+2], 0, AU_headers_length);
            
            au_headers[0].size = bv.getBits(size_length);
            au_headers[0].index = bv.getBits(index_length);
            cout << "au_headers_num : 0"
                 << ",size:" << au_headers[0].size
                << ",index:" << au_headers[0].index << endl;
            
            //std::cout << to_hex(&recv_buffer_[AU_headers_length + au_headers[0].index], au_headers[0].size) << endl;
            //decoder->decode_audio_rtp(&recv_buffer_[AU_headers_length + au_headers[0].index], au_headers[0].size);
            //std::cout << to_hex(&recv_buffer_[hlen], size-hlen) << endl;
            
            memset(aac_buf, '\0', buffer_len);
            //add_adts_header(aac_buf, au_headers[0].size, 0);
            add_adts_header(aac_buf, 2);
            //FFF150802C7FFC
            //FF8F110149F83F
            std::cout << to_hex(aac_buf, 7) << endl;
            
            memcpy(aac_buf+ADTS_HEADER_SIZE,
                   recv_buffer_ + AU_headers_length + au_headers[0].index,
                   au_headers[0].size);
            
            decoder->decode_audio_rtp(aac_buf,au_headers[0].size + ADTS_HEADER_SIZE);
            
            for (unsigned i = 1; i < au_headers_num; ++i) {
                au_headers[i].size = bv.getBits(size_length);
                au_headers[i].index = bv.getBits(index_delta_length);
                cout << "au_headers_num : " << i
                << ",size:" << au_headers[i].size
                << ",index:" << au_headers[i].index << endl;
                std::cout << to_hex(&recv_buffer_[AU_headers_length + au_headers[i].index], au_headers[i].size) << endl;
                
                memset(aac_buf, '\0', buffer_len);
                //add_adts_header(aac_buf, au_headers[i].size, 0);
                add_adts_header(aac_buf, 2);
                memcpy(aac_buf+ADTS_HEADER_SIZE,
                       recv_buffer_ + AU_headers_length + au_headers[i].index,
                       au_headers[i].size);
                
            }
        }
        
        return 0;
    }
    
    return -1;
}


void AACRtpPacket::add_adts_header(unsigned char *p, int len) {
    static int frame_len = ADTS_HEADER_SIZE + len;
    static int m_sampleRateIndex = 4; //44100
    /*
    96000, 88200, 64000, 48000,
    44100, 32000, 24000, 22050,
    16000, 12000, 11025, 8000,
    7350, 0, 0, 0
     */
    static int m_channel = 2; // 双声道
    /*
     channel_configuration: 表示声道数
     0: Defined in AOT Specifc Config
     1: 1 channel: front-center
     2: 2 channels: front-left, front-right
     3: 3 channels: front-center, front-left, front-right
     4: 4 channels: front-center, front-left, front-right, back-center
     5: 5 channels: front-center, front-left, front-right, back-left, back-right
     6: 6 channels: front-center, front-left, front-right, back-left, back-right, LFE-channel
     7: 8 channels: front-center, front-left, front-right, side-left, side-right, back-left, back-right, LFE-channel
     8-15: Reserved
     */
    static int m_profile = 1; // AAC(Version 4) LC(low complexity profile),
                              // 0: main profile,2: SSR scalable sampling rate profile
                              // 3: reserved

    *p++ = 0xff;                                    //syncword  (0xfff, high_8bits)
    *p = 0xf0;                                      //syncword  (0xfff, low_4bits)
    *p |= (0 << 3);                                 //ID (0, 1bit), MPEG Version: 0 for MPEG-4, 1 for MPEG-2
    *p |= (0 << 1);                                 //layer (0, 2bits)
    *p |= 1;                                        //protection_absent (1, 1bit)
    p++;
    *p = (unsigned char) ((m_profile & 0x3) << 6);  //profile (profile, 2bits)
    *p |= ((m_sampleRateIndex & 0xf) << 2);         //sampling_frequency_index (sam_idx, 4bits)
    *p |= (0 << 1);                                 //private_bit (0, 1bit)
    *p |= ((m_channel & 0x4) >> 2);                 //channel_configuration (channel, high_1bit)
    p++;
    *p = ((m_channel & 0x3) << 6);                  //channel_configuration (channel, low_2bits)
    *p |= (0 << 5);                                 //original/copy (0, 1bit)
    *p |= (0 << 4);                                 //home  (0, 1bit);
    *p |= (0 << 3);                                 //copyright_identification_bit (0, 1bit)
    *p |= (0 << 2);                                 //copyright_identification_start (0, 1bit)
    *p |= ((frame_len & 0x1800) >> 11);             //frame_length (value, high_2bits)
    p++;
    *p++ = (unsigned char) ((frame_len & 0x7f8) >> 3);  //frame_length (value, middle_8bits)
    *p = (unsigned char) ((frame_len & 0x7) << 5);      //frame_length (value, low_3bits)
    *p |= 0x1f;                                         //adts_buffer_fullness (0x7ff, high_5bits)
    p++;
    *p = 0xfc;                                          //adts_buffer_fullness (0x7ff, low_6bits)
    *p |= 0;                                            //number_of_raw_data_blocks_in_frame (0, 2bits);
    p++;
    
}

void AACRtpPacket::add_latm_header(uint8_t *p,int len) {
    
    PutBitContext pb;
    init_put_bits(&pb, p, len);
    
    static int object_type = 2;
    /*
     https://en.wikipedia.org/wiki/MPEG-4_Part_3#MPEG-4_Audio_Object_Types
    Audio Object Types
    MPEG-4 Audio Object Types:
    
    0: Null
    1: AAC Main
    2: AAC LC (Low Complexity)
    3: AAC SSR (Scalable Sample Rate)
    4: AAC LTP (Long Term Prediction)
    5: SBR (Spectral Band Replication)
    6: AAC Scalable
    7: TwinVQ
    8: CELP (Code Excited Linear Prediction)
    9: HXVC (Harmonic Vector eXcitation Coding)
    10: Reserved
    11: Reserved
    12: TTSI (Text-To-Speech Interface)
    13: Main Synthesis
    14: Wavetable Synthesis
    15: General MIDI
    16: Algorithmic Synthesis and Audio Effects
    17: ER (Error Resilient) AAC LC
    18: Reserved
    19: ER AAC LTP
    20: ER AAC Scalable
    21: ER TwinVQ
    22: ER BSAC (Bit-Sliced Arithmetic Coding)
    23: ER AAC LD (Low Delay)
    24: ER CELP
    25: ER HVXC
    26: ER HILN (Harmonic and Individual Lines plus Noise)
    27: ER Parametric
    28: SSC (SinuSoidal Coding)
    29: PS (Parametric Stereo)
    30: MPEG Surround
    31: (Escape value)
    32: Layer-1
    33: Layer-2
    34: Layer-3
    35: DST (Direct Stream Transfer)
    36: ALS (Audio Lossless)
    37: SLS (Scalable LosslesS)
    38: SLS non-core
    39: ER AAC ELD (Enhanced Low Delay)
    40: SMR (Symbolic Music Representation) Simple
    41: SMR Main
    42: USAC (Unified Speech and Audio Coding) (no SBR)
    43: SAOC (Spatial Audio Object Coding)
    44: LD MPEG Surround
    45: USAC
    */
    
    static int samplerate_index = 4; //44100
    static int channels = 2; // 双声道
    
    /*
     https://wiki.multimedia.cx/index.php?title=MPEG-4_Audio#Audio_Specific_Config
     
    5 bits: object type
    if (object type == 31)
        6 bits + 32: object type
        4 bits: frequency index
        if (frequency index == 15)
            24 bits: frequency
            4 bits: channel configuration
            var bits: AOT Specific Config
    */
    
    if(object_type == 31)
        put_bits(&pb, 6, 32);
    else
        put_bits(&pb, 5, object_type);
    
    if(samplerate_index == 15)
        put_bits(&pb, 24, samplerate_index);
    else
        put_bits(&pb, 4, samplerate_index);
    
    put_bits(&pb, 2, channels);
    
    flush_put_bits(&pb);
    
}


inline void AACRtpPacket::init_put_bits(PutBitContext *s, uint8_t *buffer,
                          int buffer_size)
{
    if (buffer_size < 0) {
        buffer_size = 0;
        buffer      = NULL;
    }
    
    s->size_in_bits = 8 * buffer_size;
    s->buf          = buffer;
    s->buf_end      = s->buf + buffer_size;
    s->buf_ptr      = s->buf;
    s->bit_left     = 32;
    s->bit_buf      = 0;
}

inline void AACRtpPacket::put_bits(PutBitContext *s, int n, unsigned int value)
{

#define AV_WL32(p, val) do {                 \
uint32_t d = (val);                     \
((uint8_t*)(p))[0] = (d);               \
((uint8_t*)(p))[1] = (d)>>8;            \
((uint8_t*)(p))[2] = (d)>>16;           \
((uint8_t*)(p))[3] = (d)>>24;           \
} while(0)
    
#   define AV_WB32(p, val) do {                 \
uint32_t d = (val);                     \
((uint8_t*)(p))[3] = (d);               \
((uint8_t*)(p))[2] = (d)>>8;            \
((uint8_t*)(p))[1] = (d)>>16;           \
((uint8_t*)(p))[0] = (d)>>24;           \
} while(0)

    
    unsigned int bit_buf;
    int bit_left;
    
    assert(n <= 31 && value < (1U << n));
    
    bit_buf  = s->bit_buf;
    bit_left = s->bit_left;
    
    /* XXX: optimize */
#ifdef BITSTREAM_WRITER_LE
    bit_buf |= value << (32 - bit_left);
    if (n >= bit_left) {
        if (3 < s->buf_end - s->buf_ptr) {
            AV_WL32(s->buf_ptr, bit_buf);
            s->buf_ptr += 4;
        } else {
            av_log(NULL, AV_LOG_ERROR, "Internal error, put_bits buffer too small\n");
            assert(0);
        }
        bit_buf     = value >> bit_left;
        bit_left   += 32;
    }
    bit_left -= n;
#else
    if (n < bit_left) {
        bit_buf     = (bit_buf << n) | value;
        bit_left   -= n;
    } else {
        bit_buf   <<= bit_left;
        bit_buf    |= value >> (n - bit_left);
        if (3 < s->buf_end - s->buf_ptr) {
            AV_WB32(s->buf_ptr, bit_buf);
            s->buf_ptr += 4;
        } else {
            av_log(NULL, AV_LOG_ERROR, "Internal error, put_bits buffer too small\n");
            assert(0);
        }
        bit_left   += 32 - n;
        bit_buf     = value;
    }
#endif
    
    s->bit_buf  = bit_buf;
    s->bit_left = bit_left;
}

inline void AACRtpPacket::flush_put_bits(PutBitContext *s)
{
#ifndef BITSTREAM_WRITER_LE
    if (s->bit_left < 32)
        s->bit_buf <<= s->bit_left;
#endif
    while (s->bit_left < 32) {
        assert(s->buf_ptr < s->buf_end);
#ifdef BITSTREAM_WRITER_LE
        *s->buf_ptr++ = s->bit_buf;
        s->bit_buf  >>= 8;
#else
        *s->buf_ptr++ = s->bit_buf >> 24;
        s->bit_buf  <<= 8;
#endif
        s->bit_left  += 8;
    }
    s->bit_left = 32;
    s->bit_buf  = 0;
}

int AACRtpPacket::add_adts_header(uint8_t *buf, int size, int pce_size)
{
    std::cout << "add_adts_header ..." << std::endl;
    const unsigned ADTS_HEADER_SIZE = 7 ;
    
    PutBitContext pb;
    init_put_bits(&pb, buf, ADTS_HEADER_SIZE);
   
    static int samplerate_index = 4; //44100
    /*
     96000, 88200, 64000, 48000,
     44100, 32000, 24000, 22050,
     16000, 12000, 11025, 8000,
     7350, 0, 0, 0
     */
    static int channels = 2; // 双声道
    /*
     channel_configuration: 表示声道数
     0: Defined in AOT Specifc Config
     1: 1 channel: front-center
     2: 2 channels: front-left, front-right
     3: 3 channels: front-center, front-left, front-right
     4: 4 channels: front-center, front-left, front-right, back-center
     5: 5 channels: front-center, front-left, front-right, back-left, back-right
     6: 6 channels: front-center, front-left, front-right, back-left, back-right, LFE-channel
     7: 8 channels: front-center, front-left, front-right, side-left, side-right, back-left, back-right, LFE-channel
     8-15: Reserved
     */
    static int profile = 1; // AAC(Version 4) LC(low complexity profile),
    // 0: main profile,2: SSR scalable sampling rate profile
    // 3: reserved
    
    /* adts_fixed_header */
    put_bits(&pb, 12, 0xfff);   /* syncword */
    put_bits(&pb, 1, 0);        /* ID */
    put_bits(&pb, 2, 0);        /* layer */
    put_bits(&pb, 1, 1);        /* protection_absent */
    put_bits(&pb, 2, profile); /* profile_objecttype */
    put_bits(&pb, 4, samplerate_index);
    put_bits(&pb, 1, 0);        /* private_bit */
    put_bits(&pb, 3, channels); /* channel_configuration */
    put_bits(&pb, 1, 0);        /* original_copy */
    put_bits(&pb, 1, 0);        /* home */
    
    /* adts_variable_header */
    put_bits(&pb, 1, 0);        /* copyright_identification_bit */
    put_bits(&pb, 1, 0);        /* copyright_identification_start */
    put_bits(&pb, 13, ADTS_HEADER_SIZE + size + pce_size); /* aac_frame_length */
    put_bits(&pb, 11, 0x7ff);   /* adts_buffer_fullness */
    put_bits(&pb, 2, 0);        /* number_of_raw_data_blocks_in_frame */
    
    flush_put_bits(&pb);
    
    return 0;
}
