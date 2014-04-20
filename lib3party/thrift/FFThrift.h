#ifndef _THRIFT_FFTHRIFT_H_
#define _THRIFT_FFTHRIFT_H_ 1

#include <string>

#include <thrift/Thrift.h>
#include <thrift/transport/TTransport.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/FFTransport.h>

struct ffthrift_t
{
    template<typename T>
    static bool EncodeToString(T& msg_, std::string& data_)
    {
        using apache::thrift::transport::FFTMemoryBuffer;
        using apache::thrift::protocol::TBinaryProtocol;
        FFTMemoryBuffer ffmem(&data_);
        TBinaryProtocol  proto(&ffmem);
        msg_.write(&proto);
        return true;
    }
    template<typename T>
    static std::string EncodeAsString(T& msg_)
    {
        std::string ret;
        ffthrift_t::EncodeToString(msg_, ret);
        return ret;
    }
    template<typename T>
    static bool DecodeFromString(T& msg_, const std::string& data_)
    {
        using apache::thrift::transport::FFTMemoryBuffer;
        using apache::thrift::protocol::TBinaryProtocol;
        FFTMemoryBuffer ffmem(data_);
        TBinaryProtocol  proto(&ffmem);
        msg_.read(&proto);
        return true;   
    }
};

#endif
