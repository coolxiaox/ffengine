#ifndef _FF_LUA_JSON_TRAITS_H_
#define _FF_LUA_JSON_TRAITS_H_

#include "lua/fflua.h"


namespace ff
{

template<> struct lua_op_t<ffjson_tool_t>
{
    static void push_stack(lua_State* ls_, const ffjson_tool_t& jtool_)
	{
	    lua_op_t<ffjson_tool_t>::push_stack(ls_, *(jtool_.jval.get()));
	}
	static void push_stack(lua_State* ls_, const json_value_t& jval)
	{
	    //const char* kTypeNames[] = { "Null", "False", "True", "Object", "Array", "String", "Number" };
	    //printf("jval type=%s\n", kTypeNames[jval.GetType()]);
	    if (jval.IsString())
	    {
	        lua_pushlstring(ls_, jval.GetString(), jval.GetStringLength());
	        //lua_pushstring(ls_, jval.GetString());
	    }
	    else if (jval.IsBool())
	    {
	        lua_pushboolean(ls_, jval.GetBool());
	    }
	    else if (jval.IsNumber())
	    {
	        lua_pushnumber(ls_, (lua_Number)(jval.GetDouble()));
	    }
		else if (jval.IsArray())
		{
		    lua_newtable(ls_);
		    for (int i = 0; i < (int)jval.Size(); i++)
		    {
		        lua_op_t<int>::push_stack(ls_, i+1);
		        lua_op_t<ffjson_tool_t>::push_stack(ls_, jval[i]);
		        lua_settable(ls_, -3);
		    }
		}
		else if (jval.IsObject())
		{
		    lua_newtable(ls_);
		    for (json_value_t::ConstMemberIterator itr = jval.MemberBegin(); itr != jval.MemberEnd(); ++itr)
		    {
		        string tmp = itr->name.GetString();
		        lua_op_t<string>::push_stack(ls_, tmp);
		        lua_op_t<ffjson_tool_t>::push_stack(ls_, itr->value);
		        lua_settable(ls_, -3);
            }
		}
		else
	    {
	        lua_pushnil(ls_);
	    }
	}
	/*
	static int get_ret_value(lua_State* ls_, int pos_, ffjson_tool_t& param_)
	{
		return 0;
	}
	*/
	static int lua_to_value(lua_State* ls_, int pos_, ffjson_tool_t& ffjson_tool)
	{
	    lua_op_t<ffjson_tool_t>::lua_to_value(ls_, pos_, ffjson_tool, *(ffjson_tool.jval));
	    return 0;
	}
	static int lua_to_value(lua_State* ls_, int pos_, ffjson_tool_t& ffjson_tool, json_value_t& jval, int depth = 0)
	{
	    if (depth > 20)
	    {
	        return -1;
	    }
	    ++depth;
        int t = lua_type(ls_, pos_);
        //printf("lua_to_value	%s -\n", lua_typename(ls_, lua_type(ls_, pos_)));
        switch (t)
        {
            case LUA_TSTRING:
            {
                //printf("LUA_TSTRING `%s'\n", lua_tostring(ls_, pos_));
                jval.SetString(lua_tostring(ls_, pos_), *ffjson_tool.allocator);
            }
            break;
            case LUA_TBOOLEAN:
            {
                //printf(lua_toboolean(ls_, pos_) ? "true" : "false");
                jval.SetBool(lua_toboolean(ls_, pos_));
            }
            break;
            case LUA_TNUMBER:
            {
                double d = lua_tonumber(ls_, pos_);
                long   l = long(d);
                int    i = int(l);
                if ((d > 0 && d > l) || (d < 0 && d < l))
                {
                    jval.SetDouble(d);
                }
                else if (i == l)
                {
                    jval.SetInt(i);
                }
                else
                {
                    jval.SetInt64(l);
                }
            }
            break;
            case LUA_TTABLE:
            {
            	//printf("table begin pos_=%d\n", pos_);
            	int table_pos = lua_gettop(ls_);
            	if (pos_ < 0)
            	{
            	    pos_= table_pos + (pos_ - (-1));
            	}
            	
            	set<int> tmp_set;
            	lua_pushnil(ls_);
            	size_t index = 0;
                while (lua_next(ls_, pos_) != 0) {
                    if (lua_type(ls_, -2) == LUA_TNUMBER)
                    {
                        tmp_set.insert(int(lua_tonumber(ls_, -2)));
                    }
                    lua_pop(ls_, 1);
                    ++ index;
                }
                bool is_array = false;
                //! 检查是不是数组
                if (tmp_set.size() == index)
                {
                    is_array = true;
                    int start = -1;
                    if (index > 1)
                    {
                        for (set<int>::iterator it = tmp_set.begin(); it != tmp_set.end(); ++it)
                        {
                            if (start < 0)
                            {
                                start = *it;
                            }
                            else if (*it != start + 1)
                            {
                                is_array = false;
                                break;
                            }
                            else
                            {
                                ++start;
                            }
                        }
                    }
                    else if (index == 1)
                    {
                        if (*tmp_set.begin() != 0 && *tmp_set.begin() != 1)
                        {
                            is_array = false;
                            break;
                        }
                    }
                }
                if (is_array)
                {
                    jval.SetArray();
                    lua_pushnil(ls_);
                    while (lua_next(ls_, pos_) != 0) {
                        //printf("	%s - %s\n",
                        //        lua_typename(ls_, lua_type(ls_, -2)),
                        //        lua_typename(ls_, lua_type(ls_, -1)));

                        json_value_t tmp_val;
                        lua_op_t<ffjson_tool_t>::lua_to_value(ls_, -1, ffjson_tool, tmp_val, depth);
                        
                        //printf("key=`%s'\n", lua_tostring(ls_, -2));
                        jval.PushBack(tmp_val, *ffjson_tool.allocator);
                        lua_pop(ls_, 1);
                    }
                }
                else
                {
                    jval.SetObject();
                    lua_pushnil(ls_);
                    while (lua_next(ls_, pos_) != 0) {
                        //printf("	%s - %s\n",
                        //        lua_typename(ls_, lua_type(ls_, -2)),
                        //        lua_typename(ls_, lua_type(ls_, -1)));
                                
                        json_value_t tmp_key;
                        json_value_t tmp_val;
                        string str_key;
                        lua_op_t<string>::lua_to_value(ls_, -2, str_key);
                        lua_op_t<ffjson_tool_t>::lua_to_value(ls_, -1, ffjson_tool, tmp_val, depth);
                        tmp_key.SetString(str_key.c_str(), str_key.length(), *ffjson_tool.allocator);
                        jval.AddMember(tmp_key, tmp_val, *(ffjson_tool.allocator));
                        //printf("key=`%s'\n", lua_tostring(ls_, -2));
                        
                        lua_pop(ls_, 1);
                    }
                }
                //printf("table end\n");
            }
            break;
            default:
            {
                jval.SetNull();
                //printf("`%s`", lua_typename(ls_, t));
            }
            break;
        }
		return 0;
	}
};

}

#endif


