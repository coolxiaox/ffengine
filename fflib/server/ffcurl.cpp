
#ifdef FF_ENABLE_CURL

#include "server/ffcurl.h"
#include <curl/curl.h>
//! 中文

using namespace ff;

static size_t write_data_func(void *ptr, size_t size, size_t nmemb, void *stream)
{
    ffcurl_t::result_t* ret = (ffcurl_t::result_t*)stream;
    int written = size * nmemb;
    if (ret)
    {
        ret->data.append((const char*)ptr, written);
    }

    return written;
}

void ffcurl_t::post(const map<int, param_t>& param_)
{
    CURL* curl = NULL;
    CURLcode res;
	curl = curl_easy_init();
	result_t result;
	if(curl)
	{
	    for (map<int, param_t>::const_iterator it = param_.begin(); it != param_.end(); ++it)
	    {
	        if (it->second.arg_flag == 0)
	        {
	            curl_easy_setopt(curl, (CURLoption)it->first, it->second.larg);
	        }
	        else
            {
                curl_easy_setopt(curl, (CURLoption)it->first, it->second.sarg.c_str());
            }
	    }
	    
	    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data_func);
	    curl_easy_setopt(curl, CURLOPT_WRITEHEADER, &result);
		res = curl_easy_perform(curl);
		/* Check for errors */ 
        if(res != CURLE_OK)
        {
            result.err = curl_easy_strerror(res);
        }
		curl_easy_cleanup(curl);
	}
}

#endif
