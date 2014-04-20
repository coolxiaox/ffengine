//! 二进制序列化 
#ifndef _CODEC_H_
#define _CODEC_H_

#include <arpa/inet.h>

#include <string>
#include <stdexcept>
#include <vector>
#include <iostream>
#include <map>
#include <set>
using namespace std;

#include "net/message.h"
#include "base/singleton.h"
#include "base/atomic_op.h"
#include "base/lock.h"
#include "base/fftype.h"
#include "base/smart_ptr.h"

namespace apache
{
    namespace thrift{
        namespace protocol
        {
            class TProtocol;
        }
    }
}
namespace ff {
 

#define swap64_ops(ll) (((ll) >> 56) | \
                    (((ll) & 0x00ff000000000000) >> 40) | \
                    (((ll) & 0x0000ff0000000000) >> 24) | \
                    (((ll) & 0x000000ff00000000) >> 8)    | \
                    (((ll) & 0x00000000ff000000) << 8)    | \
                    (((ll) & 0x0000000000ff0000) << 24) |   \
                    (((ll) & 0x000000000000ff00) << 40) | \
                    (((ll) << 56)))  

struct endian_too_t
{
    static bool is_bigendian()  
    {
        const int16_t n = 1;  
        if(*(char *)&n)  
        {  
            return false;  
        }  
        return true;  
    }
};
#define hton64(ll) (endian_too_t::is_bigendian() ? ll : (ll) )
#define ntoh64(ll) (endian_too_t::is_bigendian() ? ll:  (ll))
    
struct codec_i
{
    virtual ~codec_i(){}
    virtual string encode_data()                      = 0;
    virtual void decode_data(const string& src_buff_) = 0;
};

class bin_encoder_t;
class bin_decoder_t;
struct codec_helper_i
{
    virtual ~codec_helper_i(){}
    virtual void encode(bin_encoder_t&) const = 0;
    virtual void decode(bin_decoder_t&)       = 0;
};


template<typename T>
struct option_t
{
    option_t():
        data(NULL)
    {}
    option_t(const option_t& src_):
        data(NULL)
    {
        if (src_->data)
        {
            data = new T(src_->data);
        }
    }
    ~option_t()
    {
        release();
    }
    void release()
    {
        if (data)
        {
            delete data;
            data = NULL;
        }
    }
    void reset(T* p = NULL)
    {
        release();
        data = p;
    }
    T* get()        { return data; }
    T* operator->() { return data; }
    T* operator*()  { return data; }
    option_t& operator=(const option_t& src_)
    {
        release();
        if (src_->data)
        {
            data = new T(src_->data);
        }
        return *this;
    }
    T* data;
};

template<typename T>
struct codec_tool_t;
class bin_encoder_t
{
public:
    const string& get_buff() const { return m_dest_buff; }
    
    template<typename T>
    bin_encoder_t& operator << (const T& val_);

    void clear()
    {
        m_dest_buff.clear();
    }
    inline bin_encoder_t& copy_value(const void* src_, size_t size_)
    {
        m_dest_buff.append((const char*)(src_), size_);
        return *this;
    }
    inline bin_encoder_t& copy_value(const string& str_)
    {
        uint32_t str_size =::htonl( str_.size());
        copy_value((const char*)(&str_size), sizeof(str_size));
        copy_value(str_.data(), str_.size());
        return *this;
    }

private:
    string         m_dest_buff;
};

class bin_decoder_t
{
public:
    bin_decoder_t():
        m_index_ptr(NULL),
        m_remaindered(0)
    {}
    explicit bin_decoder_t(const string& src_buff_):
        m_index_ptr(src_buff_.data()),
        m_remaindered(src_buff_.size())
    {
    }
    bin_decoder_t& init(const string& str_buff_)
    {
        m_index_ptr = str_buff_.data();
        m_remaindered = str_buff_.size();
        return *this;
    }
    template<typename T>
    bin_decoder_t& operator >> (T& val_);

    bin_decoder_t& copy_value(void* dest_, uint32_t var_size_)
    {
        if (m_remaindered < var_size_)
        {
            throw runtime_error("bin_decoder_t:msg size not enough");
        }
        ::memcpy(dest_, m_index_ptr, var_size_);
        m_index_ptr     += var_size_;
        m_remaindered  -= var_size_;
        return *this;
    }
    
    bin_decoder_t& copy_value(string& dest_)
    {
        uint32_t str_len = 0;
        copy_value(&str_len, sizeof(str_len));
        str_len = ::ntohl(str_len);

        if (m_remaindered < str_len)
        {
            throw runtime_error("bin_decoder_t:msg size not enough");
        }
        
        dest_.assign((const char*)m_index_ptr, str_len);
        m_index_ptr     += str_len;
        m_remaindered   -= str_len;
        return *this;
    }

private:
    const char*  m_index_ptr;
    size_t       m_remaindered;
};


//! 用于traits 序列化的参数
template<typename T>
struct codec_tool_t
{
    static void encode(bin_encoder_t& en_, const T& val_)
    {
        T* pobj = (T*)(&val_);
        en_ << pobj->encode_data();
    }
    static void decode(bin_decoder_t& de_, T& val_)
    {
        string data;
        de_ >> data;
        val_.decode_data(data);
    }
};

//! 用于traits 序列化的参数
#define GEN_CODE_ENCODE_DECODE(X)                                          \
template<>                                                          \
struct codec_tool_t<X>                                              \
{                                                                   \
    static void encode(bin_encoder_t& en_, const X& val_)           \
    {                                                               \
        en_.copy_value((const char*)(&val_), sizeof(val_));         \
    }                                                               \
    static void decode(bin_decoder_t& de_, X& val_)           \
    {                                                               \
        de_.copy_value((void*)(&val_), sizeof(val_));         \
    }                                                               \
};

#define GEN_CODE_ENCODE_DECODE_SHORT(X)                                          \
template<>                                                          \
struct codec_tool_t<X>                                              \
{                                                                   \
    static void encode(bin_encoder_t& en_, const X& src_val_)       \
    {                                                               \
        X val_ = (X)::htons(src_val_);                                 \
        en_.copy_value((const char*)(&val_), sizeof(val_));         \
    }                                                               \
    static void decode(bin_decoder_t& de_, X& val_)                 \
    {                                                               \
        de_.copy_value((void*)(&val_), sizeof(val_));               \
        val_ = (X)::ntohs(val_);                                       \
    }                                                               \
};
#define GEN_CODE_ENCODE_DECODE_LONG(X)                                          \
template<>                                                          \
struct codec_tool_t<X>                                              \
{                                                                   \
    static void encode(bin_encoder_t& en_, const X& src_val_)       \
    {                                                               \
        X val_ = (X)::htonl(src_val_);                                 \
        en_.copy_value((const char*)(&val_), sizeof(val_));         \
    }                                                               \
    static void decode(bin_decoder_t& de_, X& val_)                 \
    {                                                               \
        de_.copy_value((void*)(&val_), sizeof(val_));               \
        val_ = (X)::ntohl(val_);                                       \
    }                                                               \
};
#define GEN_CODE_ENCODE_DECODE_64(X)                                \
template<>                                                          \
struct codec_tool_t<X>                                              \
{                                                                   \
    static void encode(bin_encoder_t& en_, const X& src_val_)       \
    {                                                               \
        X val_ = (X)hton64(src_val_);                                 \
        en_.copy_value((const char*)(&val_), sizeof(val_));         \
    }                                                               \
    static void decode(bin_decoder_t& de_, X& val_)                 \
    {                                                               \
        de_.copy_value((void*)(&val_), sizeof(val_));               \
        val_ = (X)ntoh64(val_);                                     \
    }                                                               \
};


//! 用于traits 序列化的参数
template<>
struct codec_tool_t<string>
{
    static void encode(bin_encoder_t& en_, const string& val_)
    {
        en_.copy_value(val_);
    }
    static void decode(bin_decoder_t& de_, string& val_)
    {
        de_.copy_value(val_);
    }
};
template<typename T>
struct codec_tool_t<vector<T> >
{
    static void encode(bin_encoder_t& en_, const vector<T>& val_)
    {
        uint32_t num = val_.size();
        en_.copy_value((const char*)(&num), sizeof(num));
        typename vector<T>::const_iterator it = val_.begin();
        for (; it != val_.end(); ++it)
        {
            en_ << (*it);
        }
    }
    static void decode(bin_decoder_t& de_, vector<T>& val_)
    {
        uint32_t num = 0;
        de_.copy_value((void*)(&num), sizeof(num));
        for (uint32_t i = 0; i < num; ++i)
        {
            T tmp_key;
            de_ >> tmp_key;
            val_.push_back(tmp_key);
        }
    }
};
template<typename T>
struct codec_tool_t<list<T> >
{
    static void encode(bin_encoder_t& en_, const list<T>& val_)
    {
        uint32_t num = val_.size();
        en_.copy_value((const char*)(&num), sizeof(num));
        typename list<T>::const_iterator it = val_.begin();
        for (; it != val_.end(); ++it)
        {
            en_ << (*it);
        }
    }
    static void decode(bin_decoder_t& de_, list<T>& val_)
    {
        uint32_t num = 0;
        de_.copy_value((void*)(&num), sizeof(num));
        for (uint32_t i = 0; i < num; ++i)
        {
            T tmp_key;
            de_ >> tmp_key;
            val_.push_back(tmp_key);
        }
    }
};
template<typename T>
struct codec_tool_t<set<T> >
{
    static void encode(bin_encoder_t& en_, const set<T>& val_)
    {
        uint32_t num = val_.size();
        en_.copy_value((const char*)(&num), sizeof(num));
        typename set<T>::const_iterator it = val_.begin();
        for (; it != val_.end(); ++it)
        {
            en_ << (*it);
        }
    }
    static void decode(bin_decoder_t& de_, set<T>& val_)
    {
        uint32_t num = 0;
        de_.copy_value((void*)(&num), sizeof(num));
        for (uint32_t i = 0; i < num; ++i)
        {
            T tmp_key;
            de_ >> tmp_key;
            val_.insert(tmp_key);
        }
    }
};
template<typename T, typename R>
struct codec_tool_t<map<T, R> >
{
    static void encode(bin_encoder_t& en_, const map<T, R>& val_)
    {
        uint32_t num = val_.size();
        en_.copy_value((const char*)(&num), sizeof(num));
        typename map<T, R>::const_iterator it = val_.begin();
        for (; it != val_.end(); ++it)
        {
            en_ << it->first << it->second;
        }
    }
    static void decode(bin_decoder_t& de_, map<T, R>& val_)
    {
        uint32_t num = 0;
        de_.copy_value((void*)(&num), sizeof(num));
        for (uint32_t i = 0; i < num; ++i)
        {
            T tmp_key;
            R tmp_val;
            de_ >> tmp_key >> tmp_val;
            val_[tmp_key] = tmp_val;
        }
    }
};

GEN_CODE_ENCODE_DECODE(bool)
GEN_CODE_ENCODE_DECODE(int8_t)
GEN_CODE_ENCODE_DECODE(uint8_t)
GEN_CODE_ENCODE_DECODE_SHORT(int16_t)
GEN_CODE_ENCODE_DECODE_SHORT(uint16_t)
GEN_CODE_ENCODE_DECODE_LONG(int32_t)
GEN_CODE_ENCODE_DECODE_LONG(uint32_t)
GEN_CODE_ENCODE_DECODE_64(int64_t)
GEN_CODE_ENCODE_DECODE_64(uint64_t)

template<typename T>
bin_encoder_t& bin_encoder_t::operator << (const T& val_)
{
    codec_tool_t<T>::encode(*this, val_);
    return *this;
}
template<typename T>
bin_decoder_t& bin_decoder_t::operator >> (T& val_)
{
    codec_tool_t<T>::decode(*this, val_);
    return *this;
}

class msg_i : public codec_i
{
public:
    virtual ~msg_i(){}
    msg_i(const char* msg_name_):
        m_msg_name(msg_name_)
    {}
    const char* get_type_name()  const
    {
        return m_msg_name;
    }
    bin_encoder_t& encoder()
    {
        return m_encoder;
    }
    bin_decoder_t& decoder()
    {
        return m_decoder;
    }
    virtual string encode_data()
    {
        m_encoder.clear();
        encode();
        return m_encoder.get_buff();
    }
    virtual void decode_data(const string& src_buff_)
    {
        m_decoder.init(src_buff_);
        decode();
    }
    virtual void encode()                      = 0;
    virtual void decode()                      = 0;
    const char*   m_msg_name;
    bin_encoder_t m_encoder;
    bin_decoder_t m_decoder;
};

struct ffthrift_codec_tool_t
{
    static void write(msg_i& msg, ::apache::thrift::protocol::TProtocol* iprot);
    static void read(msg_i& msg, ::apache::thrift::protocol::TProtocol* iprot);
};

template<typename T>
class ffmsg_t: public msg_i
{
public:
    ffmsg_t():
        msg_i(TYPE_NAME(T).c_str())
    {}
    virtual ~ffmsg_t(){}
    void write(::apache::thrift::protocol::TProtocol* iprot)
    {
        string tmp = this->encode_data();
        ffthrift_codec_tool_t::write(*this, iprot);
    }
    void read(::apache::thrift::protocol::TProtocol* iprot)
    {
        ffthrift_codec_tool_t::read(*this, iprot);
    }
};

}



#endif
