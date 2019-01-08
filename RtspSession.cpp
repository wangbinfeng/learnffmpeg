//
//  RtspSession.cpp
//  testboost
//
//  Created by 王斌峰 on 2018/10/3.
//  Copyright © 2018年 Binfeng. All rights reserved.
//
#include "RtspSession.hpp"

#include <memory>
#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#include "strDup.hh"
#include <Base64.HH>

#include "ffmpegtools.hpp"

void RtspSession::start() {
    cout << "\n start session ... " << endl;
    decoder = make_unique<FFmpegTools>();
    
    u_int8_t* sps = NULL; unsigned spsSize = 0;
    u_int8_t* pps = NULL; unsigned ppsSize = 0;
    if ( !prop_parameter_sets.empty() ) {
        cout << "parse prop_parameter_sets, get sps and pps ..." << endl;
        unsigned numSPropRecords;
        SPropRecord* sPropRecords = parseSPropParameterSets(prop_parameter_sets.c_str(), numSPropRecords);
        for (unsigned i = 0; i < numSPropRecords; ++i) {
            if (sPropRecords[i].sPropLength == 0) continue; // bad data
            u_int8_t nal_unit_type = (sPropRecords[i].sPropBytes[0])&0x1F;
            if ( nal_unit_type == 7/*SPS*/) {
                sps = sPropRecords[i].sPropBytes;
                spsSize = sPropRecords[i].sPropLength;
                cout << "it's SPS, size:"<< spsSize << endl;
            } else if ( nal_unit_type == 8/*PPS*/ ) {
                pps = sPropRecords[i].sPropBytes;
                ppsSize = sPropRecords[i].sPropLength;
                cout << "it's PPS, size:"<<ppsSize << endl;
            }
        }
    }
    
    decoder->open_codec(video_codec_name.c_str(),audio_codec_name.c_str(),
                        sps, spsSize, pps, ppsSize );

    video_parser = make_unique<H264RtpPacket>();
    video_parser->set_decoder(decoder.get());
    video_parser->set_seq_no(seqNum);
    video_parser->set_sps_pps(sps, spsSize, pps, ppsSize);

    audio_parser = make_unique<AACRtpPacket>(size_length, index_length, index_delta_length);
    audio_parser->set_decoder(decoder.get());
    
    rtpUdpServer = new BoostUdpServer(clientRtpPort);
    rtpUdpServer->set_packet_parser(video_parser.get());
    rtpUdpServer->set_packet_parser(audio_parser.get());
    
    rtcpUdpServer = new BoostUdpServer(clientRtcpPort,1);
    
    auto tf = [](BoostUdpServer* _server) {
        _server->run();
    };
    cout << "start thread ..." << endl;
    rtpThread = new thread(tf, rtpUdpServer);
    //rtcpThread = new thread(tf,rtcpUdpServer);
    
    auto tf2 = [](BoostUdpClient* _client) {
        _client->start();
    };
    
    //rtcpUdpClient = new BoostUdpClient(dest,serverRtcpPort);
    //rtcpServerThread = new thread(tf2,rtcpUdpClient);
    
}

void RtspSession::close(){
    cout << "stop udpserver port:" << clientRtpPort << endl;
    if (rtpUdpServer != nullptr) {
        rtpUdpServer->stop();
        rtpUdpServer = nullptr;
    }
     cout << "stop udpserver port:" << clientRtcpPort << endl;
    if (rtcpUdpServer != nullptr) {
        rtcpUdpServer->stop();
        rtcpUdpServer = nullptr;
    }

    if (rtcpUdpClient != nullptr) {
        rtcpUdpClient->close();
        rtcpUdpClient = nullptr;
    }

    cout << "join thread...." << endl;
    if ( rtpThread != nullptr )
       rtpThread->join();
    
    if ( rtcpThread != nullptr )
       rtcpThread->join();
    
    if ( rtcpServerThread != nullptr )
       rtcpServerThread->join();
}


void RtspSession::setTransport(const string& _transport){
    vector<string> sv{};
    boost::split(sv, _transport, boost::is_any_of(";"), boost::token_compress_on );
    if(sv.size()>1)
    {
        cout << sv.size() << endl;
        rtpInfo = sv[0];
        cout<<sv[2] << endl;
        dest = sv[2].substr(12);
        cout << sv[3] << endl;
        source = sv[3].substr(7);
        cout << sv[4] << endl;
        string client_port = sv[4].substr(12);
        std::string::size_type pos = client_port.find("-");
        if(pos != std::string::npos){
            clientRtpPort = std::stoi(client_port.substr(0,pos));
            clientRtcpPort = std::stoi(client_port.substr(pos+1));
            std::cout << clientRtpPort << ", " << clientRtpPort << endl;
        }
        cout << sv[5] << endl;
        string server_port = sv[5].substr(12);
        pos = server_port.find("-");
        if(pos != std::string::npos){
            serverRtpPort = std::stoi(server_port.substr(0,pos));
            serverRtcpPort = std::stoi(server_port.substr(pos+1));
            std::cout << serverRtpPort << ", " << serverRtcpPort << endl;
        }
        
        std::cout << client_port << ", " << server_port << endl;
    }
    
}

void RtspSession::setBody(const string& body_){
    std::istringstream in_stream(body_.c_str());
    std::string sdpline;
    
    while (std::getline(in_stream, sdpline,'\r')) {
        cout << "setBody:"<< sdpline << endl;
        if (sdpline.at(0) == 'm') {
            char* mediumName = strDupSize(sdpline.c_str()); // ensures we have enough space
           
            if ((sscanf(sdpline.c_str(), "m=%s %hu RTP/AVP %u",
                        mediumName, &fClientPortNum, &payloadFormat) == 3 ||
                 sscanf(sdpline.c_str(), "m=%s %hu/%*u RTP/AVP %u",
                        mediumName, &fClientPortNum, &payloadFormat) == 3)
                && payloadFormat <= 127) {
                protocolName = "RTP";
                cout << "payload:"<<payloadFormat << endl;
            }
            mline = strDup(mediumName);
            
            delete[] mediumName;
        }
        if (parseSDPAttribute_fmtp(sdpline.c_str()))
            continue;
        if (parseSDPAttribute_rtpmap(sdpline.c_str(), mline))
            continue;
    }
    
    cout << "\n\n\nprop_parameter_sets:" << prop_parameter_sets << endl;
    cout << "profile-level-id:" << profile_level_id << endl;
    cout << "packetization-mode:" << packetization_mode << endl;
    cout << "size_length:"<< size_length << ", config:" << config << endl;
}

SPropRecord* RtspSession::parseSPropParameterSets(char const* sPropParameterSetsStr,
                                     unsigned& numSPropRecords) {
    // Make a copy of the input string, so we can replace the commas with '\0's:
    char* inStr = strDup(sPropParameterSetsStr);
    if (inStr == NULL) {
        numSPropRecords = 0;
        return NULL;
    }
    
    // Count the number of commas (and thus the number of parameter sets):
    numSPropRecords = 1;
    char* s;
    for (s = inStr; *s != '\0'; ++s) {
        if (*s == ',') {
            ++numSPropRecords;
            *s = '\0';
        }
    }
    // Allocate and fill in the result array:
    SPropRecord* resultArray = new SPropRecord[numSPropRecords];
    s = inStr;
    for (unsigned i = 0; i < numSPropRecords; ++i) {
        resultArray[i].sPropBytes = base64Decode(s, resultArray[i].sPropLength);
        s += strlen(s) + 1;
    }
    
    delete[] inStr;
    return resultArray;
}

bool RtspSession::parseSDPAttribute_fmtp(char const* sdpLine) {
    // Check for a "a=fmtp:" line:
    // Later: Check that payload format number matches; #####
    do {
        if (strncmp(sdpLine, "a=fmtp:", 7) != 0) break; sdpLine += 7;
        while (isdigit(*sdpLine)) ++sdpLine;
        
        unsigned const sdpLineLen = strlen(sdpLine);
        char* nameStr = new char[sdpLineLen+1];
        char* valueStr = new char[sdpLineLen+1];
        
        while (*sdpLine != '\0' && *sdpLine != '\r' && *sdpLine != '\n') {
            int sscanfResult = sscanf(sdpLine, " %[^=; \t\r\n] = %[^; \t\r\n]", nameStr, valueStr);
            if (sscanfResult >= 1) {
                for (char* c = nameStr; *c != '\0'; ++c) *c = tolower(*c);
                
                cout << nameStr << ":" << valueStr << endl;
                if (sscanfResult == 1) {
                    // <name>
                    
                } else {
                    // <name>=<value>
                    if (strncmp(nameStr,"sprop-parameter-sets",20) == 0) {
                        prop_parameter_sets = valueStr;
                    }else if (strncmp(nameStr,"profile-level-id",16) == 0) {
                        profile_level_id = valueStr;
                    }else if (strncmp(nameStr,"packetization-mode",18) == 0) {
                        packetization_mode = valueStr;
                    }else if (strncmp(nameStr,"mode",4) == 0) {
                        
                    }else if (strncmp(nameStr,"sizelength",10) == 0 ) {
                        size_length = atoi(valueStr);
                    }else if (strncmp(nameStr,"indexlength",11) == 0 ) {
                        index_length = atoi(valueStr);
                    }else if (strncmp(nameStr,"indexdeltalength",16) == 0 ) {
                        index_delta_length = atoi(valueStr);
                    }else if (strncmp(nameStr,"config",6) == 0 ) {
                        config = sampling_frequency_from_audio_specific_config(valueStr);
                    }else if (strncmp(nameStr,"cpresent",8) == 0 ) {
                        
                    }else if (strncmp(nameStr,"object",6) == 0 ) {
                        
                    }
                }
            }
            
            // Move to the next parameter assignment string:
            while (*sdpLine != '\0' && *sdpLine != '\r' && *sdpLine != '\n' && *sdpLine != ';') ++sdpLine;
            while (*sdpLine == ';') ++sdpLine;
        }
        delete[] nameStr; delete[] valueStr;
        return true;
    } while (0);
    
    return false;
}


bool RtspSession::parseSDPAttribute_rtpmap(char const* sdpLine,const string& mline) {
    // Check for a "a=rtpmap:<fmt> <codec>/<freq>" line:
    // (Also check without the "/<freq>"; RealNetworks omits this)
    // Also check for a trailing "/<numChannels>".
    bool parseSuccess = false;
    
    unsigned rtpmapPayloadFormat;
    char* codecName = strDupSize(sdpLine); // ensures we have enough space
    unsigned rtpTimestampFrequency = 0;
    unsigned numChannels = 1;
    
    if (sscanf(sdpLine, "a=rtpmap: %u %[^/]/%u/%u",
               &rtpmapPayloadFormat, codecName, &rtpTimestampFrequency,
               &numChannels) == 4
        || sscanf(sdpLine, "a=rtpmap: %u %[^/]/%u",
                  &rtpmapPayloadFormat, codecName, &rtpTimestampFrequency) == 3
        || sscanf(sdpLine, "a=rtpmap: %u %s",
                  &rtpmapPayloadFormat, codecName) == 2) {
            
            parseSuccess = true;
            if (rtpmapPayloadFormat == payloadFormat)
            {
                for (char* p = codecName; *p != '\0'; ++p) *p = toupper(*p);
            }
            if (mline == "video")
                video_codec_name = strDup(codecName);
            else if (mline == "audio")
                audio_codec_name = strDup(codecName);
    }
    
    delete[] codecName;
    
    return parseSuccess;
}

bool RtspSession::setRTPInfo(char const* paramsStr) {
    cout << "rtp-info:" << paramsStr << endl;
    if (paramsStr == NULL || paramsStr[0] == '\0') return false;
    while (paramsStr[0] == ',') ++paramsStr;
    
    // "paramsStr" now consists of a ';'-separated list of parameters, ending with ',' or '\0'.
    char* field = strDupSize(paramsStr);
    
    bool sawSeq = false, sawRtptime = false;
    while (sscanf(paramsStr, "%[^;,]", field) == 1) {
        if (sscanf(field, "seq=%hu", &seqNum) == 1) {
            sawSeq = true;
        } else if (sscanf(field, "rtptime=%u", &timestamp) == 1) {
            sawRtptime = true;
        }
        
        paramsStr += strlen(field);
        if (paramsStr[0] == '\0' || paramsStr[0] == ',') break;
        // ASSERT: paramsStr[0] == ';'
        ++paramsStr; // skip over the ';'
    }
    
    delete[] field;
    cout << "seq=" << seqNum << ", rtptime=" << timestamp << endl;
    // For the "RTP-Info:" parameters to be useful to us, we need to have seen both the "seq=" and "rtptime=" parameters:
    return sawSeq && sawRtptime;
}

unsigned RtspSession::sampling_frequency_from_audio_specific_config(char const* configStr) {
    unsigned char* config = NULL;
    unsigned result = 0; // if returned, indicates an error
    
    auto getNibble = [=](char const*& configStr,
                             unsigned char& resultNibble)-> bool {
        char c = configStr[0];
        if (c == '\0') return false; // we've reached the end
        
        if (c >= '0' && c <= '9') {
            resultNibble = c - '0';
        } else if (c >= 'A' && c <= 'F') {
            resultNibble = 10 + c - 'A';
        } else if (c >= 'a' && c <= 'f') {
            resultNibble = 10 + c - 'a';
        } else {
            return false;
        }
        
        ++configStr; // move to the next nibble
        return true;
    };
    
    auto getByte = [=](char const*& configStr, unsigned char& resultByte) -> bool {
        resultByte = 0; // by default, in case parsing fails
        
        unsigned char firstNibble;
        if (!getNibble(configStr, firstNibble)) return false;
        resultByte = firstNibble<<4;
        
        unsigned char secondNibble = 0;
        if (!getNibble(configStr, secondNibble) && configStr[0] != '\0') {
            // There's a second nibble, but it's malformed
            return false;
        }
        resultByte |= secondNibble;
        
        return true;
    };
    
    auto parse_general_config_str = [=](char const* configStr,
                                       unsigned& configSize) -> uint8_t* {
            uint8_t* config = NULL;
            do {
                if (configStr == NULL) break;
                configSize = (strlen(configStr)+1)/2;
                
                config = new unsigned char[configSize];
                if (config == NULL) break;
                
                unsigned i;
                for (i = 0; i < configSize; ++i) {
                    if (!getByte(configStr, config[i])) break;
                }
                if (i != configSize) break; // part of the string was bad
                
                return config;
            } while (0);
            
            configSize = 0;
            delete[] config;
        
            return nullptr;
    };
    
    
    static unsigned const samplingFrequencyFromIndex[16] = {
        96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
        16000, 12000, 11025, 8000, 7350, 0, 0, 0
    };
    
    do {
        // Begin by parsing the config string:
        unsigned configSize;
        config = parse_general_config_str(configStr, configSize);
        if (config == NULL) break;
        
        if (configSize < 2) break;
        unsigned char samplingFrequencyIndex = ((config[0]&0x07)<<1) | (config[1]>>7);
        if (samplingFrequencyIndex < 15) {
            result = samplingFrequencyFromIndex[samplingFrequencyIndex];
            break;
        }
        
        // Index == 15 means that the actual frequency is next (24 bits):
        if (configSize < 5) break;
        result = ((config[1]&0x7F)<<17) | (config[2]<<9) | (config[3]<<1) | (config[4]>>7);
    } while (0);
    
    delete[] config;
    return result;
}
