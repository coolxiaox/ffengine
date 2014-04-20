#ifndef _FF_CURL_IMPL_H_
#define _FF_CURL_IMPL_H_

#include <map>
#include <string>
using namespace std;

namespace ff
{

class ffcurl_t
{
public:
    struct param_t
    {
        param_t():
            arg_flag(0),
            larg(0)
        {}
        int    arg_flag;
        long   larg;
        string sarg;
    };
    struct result_t
    {
        string err;
        string data;
    };
public:
    ffcurl_t();
    ~ffcurl_t();

    void post(const map<int, param_t>& param_);

};
}
#endif

