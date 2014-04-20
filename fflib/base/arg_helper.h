#ifndef _ARG_HELPER_H_
#define _ARG_HELPER_H_

#include <string>
#include <vector>
#include <fstream>
using namespace std;

#include "base/strtool.h"

class arg_helper_t
{
public:
    arg_helper_t(int argc, char* argv[])
    {
        for (int i = 0; i < argc; ++i)
        {
            m_args.push_back(argv[i]);
        }
        if (is_enable_option("-f"))
        {
            load_from_file(get_option_value("-f"));
        }
    }
    arg_helper_t(string arg_str_)
    {
        vector<string> v;
        strtool::split(arg_str_, v, " ");
        m_args.insert(m_args.end(), v.begin(), v.end());
        if (is_enable_option("-f"))
        {
            load_from_file(get_option_value("-f"));
        }
    }
    string get_option(int idx_) const
    {   
        if ((size_t)idx_ >= m_args.size())
        {   
                return ""; 
        }   
        return m_args[idx_];
    }   
    bool is_enable_option(string opt_) const
    {
        for (size_t i = 0; i < m_args.size(); ++i)
        {
            if (opt_ == m_args[i])
            {
                    return true;
            }
        }
        return false;
    }

    string get_option_value(string opt_) const
    {
        string ret;
        for (size_t i = 0; i < m_args.size(); ++i)
        {   
            if (opt_ == m_args[i])
            {   
                size_t value_idx = ++ i;
                if (value_idx >= m_args.size())
                {
                        return ret;
                }
                ret = m_args[value_idx];
                return ret;
            }   
        }	
        return ret;
    }
    int load_from_file(const string& file_)
    {
        ifstream inf(file_.c_str());
        string all;
        string tmp;
        while (inf.eof() == false)
        {
            if (!getline(inf, tmp))
            {
                break;
            }
            if (tmp.empty() == false && tmp[0] == '#')//!过滤注释
            {
                continue;
            }
            //printf("get:%s\n", tmp.c_str());
            //sleep(1);
            all += tmp + " ";
            tmp.clear();
        }
        vector<string> v;
        strtool::split(all, v, " ");
        m_args.insert(m_args.end(), v.begin(), v.end());
        return 0;
    }
private:
    vector<string>  m_args;
};

#endif

