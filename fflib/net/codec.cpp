
#include "net/codec.h"

#include <thrift/FFThrift.h>
using namespace ff;

void ffthrift_codec_tool_t::write(msg_i& msg, ::apache::thrift::protocol::TProtocol* iprot)
{
    string tmp = msg.encode_data();
    ::apache::thrift::protocol::TTransport* trans_ = iprot->getTransport();
    trans_->write((const uint8_t*)tmp.c_str(), tmp.size());
}
void ffthrift_codec_tool_t::read(msg_i& msg, ::apache::thrift::protocol::TProtocol* iprot)
{
    ::apache::thrift::protocol::TTransport* trans_ = iprot->getTransport();
    apache::thrift::transport::FFTMemoryBuffer* pmem = (apache::thrift::transport::FFTMemoryBuffer*)trans_;
    msg.decode_data(pmem->get_rbuff());
}

