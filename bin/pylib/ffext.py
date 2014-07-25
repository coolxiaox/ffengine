# -*- coding: utf-8 -*-
import json
import ff
import sys
import datetime
import time

import thrift.Thrift as Thrift
import thrift.protocol.TBinaryProtocol as TBinaryProtocol
import thrift.protocol.TCompactProtocol as TCompactProtocol
import thrift.protocol.TJSONProtocol as TJSONProtocol
import thrift.transport.TTransport as TTransport

g_session_verify_callback  = None
g_session_enter_callback   = None
g_session_offline_callback = None
g_session_logic_callback_dict = {}#cmd -> func callback
g_timer_callback_dict = {}

def session_verify_callback(func_):
    global g_session_verify_callback
    g_session_verify_callback = func_
    return func_
def session_enter_callback(func_):
    global g_session_enter_callback
    g_session_enter_callback = func_
    return func_
def session_offline_callback(func_):
    global g_session_offline_callback
    g_session_offline_callback = func_
    return func_
GID = 0
def once_timer(timeout_, func_):
    global g_timer_callback_dict, GID
    GID += 1
    if GID > 1000000000:
        GID = 1
    g_timer_callback_dict[GID] = func_
    ff.ffscene_obj.once_timer(timeout_, GID);

def set_py_cmd2msg(cmd, msg_name):
    return ff.ffscene_obj.set_py_cmd2msg(cmd, msg_name)
def json_to_value(val_):
    return json.loads(val_)

def protobuf_to_value(msg_type_, val_):
    dest = msg_type_()
    dest.ParseFromString(val_)
    return dest

g_protocol = 0 #1 json
g_ReadTMemoryBuffer   = TTransport.TMemoryBuffer()
g_ReadTBinaryProtocol = TBinaryProtocol.TBinaryProtocol(g_ReadTMemoryBuffer)

def set_protocol_type(s):
    global g_protocol
    if s == "json":
        g_protocol = 1
    print('set_protocol_type', g_protocol)
    return True

def decode_buff(dest, val_):
    global g_ReadTMemoryBuffer, g_ReadTBinaryProtocol
    g_ReadTMemoryBuffer.cstringio_buf.truncate()
    g_ReadTMemoryBuffer.cstringio_buf.seek(0)
    g_ReadTMemoryBuffer.cstringio_buf.write(val_)
    g_ReadTMemoryBuffer.cstringio_buf.seek(0)
    if g_protocol == 1:
        proto = TJSONProtocol.TJSONProtocol(g_ReadTMemoryBuffer)
        proto.readMessageBegin();
        dest.read(proto)
        proto.readMessageEnd();
    else:
        dest.read(g_ReadTBinaryProtocol)
    return dest

def encode_msg(msg):
    return to_str(msg)
def decode_msg(dest, val_):
    return decode_buff(dest, val_)

def ignore_id(id_):
    return id_
def session_call(cmd_, protocol_type_ = 'json', convert_func_ = ignore_id):
    global g_session_logic_callback_dict
    def session_logic_callback(func_):
        if protocol_type_ == 'json':
            g_session_logic_callback_dict[cmd_] = (json_to_value, func_, convert_func_)
        elif hasattr(protocol_type_, 'thrift_spec'):
            def thrift_to_value(val_):
                if g_protocol == 1:
                    dest = protocol_type_()
                    tmp_ReadTMemoryBuffer   = TTransport.TMemoryBuffer()
                    tmp_ReadTJsonProtocol   = TJSONProtocol.TJSONProtocol(tmp_ReadTMemoryBuffer)
                    tmp_ReadTMemoryBuffer.cstringio_buf.truncate()
                    tmp_ReadTMemoryBuffer.cstringio_buf.seek(0)
                    tmp_ReadTMemoryBuffer.cstringio_buf.write(val_)
                    tmp_ReadTMemoryBuffer.cstringio_buf.seek(0)
                    tmp_ReadTJsonProtocol.readMessageBegin();
                    dest.read(tmp_ReadTJsonProtocol);
                    tmp_ReadTJsonProtocol.readMessageEnd();
                    #mb2 = TTransport.TMemoryBuffer(val_)
                    #bp2 = TBinaryProtocol.TBinaryProtocol(mb2)
                    #dest.read(bp2);
                    return dest
                else:
                    dest = protocol_type_()
                    global g_ReadTMemoryBuffer, g_ReadTBinaryProtocol
                    g_ReadTMemoryBuffer.cstringio_buf.truncate()
                    g_ReadTMemoryBuffer.cstringio_buf.seek(0)
                    g_ReadTMemoryBuffer.cstringio_buf.write(val_)
                    g_ReadTMemoryBuffer.cstringio_buf.seek(0)
                    dest.read(g_ReadTBinaryProtocol);
                    #mb2 = TTransport.TMemoryBuffer(val_)
                    #bp2 = TBinaryProtocol.TBinaryProtocol(mb2)
                    #dest.read(bp2);
                    return dest
            g_session_logic_callback_dict[cmd_] = (thrift_to_value, func_, convert_func_)

        else: #protobuf
            def protobuf_to_value(val_):
                dest = protocol_type_()
                dest.ParseFromString(val_)
                return dest
            g_session_logic_callback_dict[cmd_] = (protobuf_to_value, func_, convert_func_)
        return func_
    return session_logic_callback


class session_mgr_t:
    def __init__(self):
        self.all_session = {} # id -> session]
        self.socket2session = {}
    def add(self, id, socket_id, session):
        self.all_session[id] = session
        self.socket2session[socket_id] = session
    def add_socket(self, socket_id, session):
        self.socket2session[socket_id] = session
    def get(self, id):
        return self.all_session.get(id)
    def get_by_sock(self, id):
        return self.socket2session.get(id)
    def remove(self, id):
        session = self.all_session.get(id)
        if None != session:
            del self.all_session[id]
            del self.socket2session[session.socket_id]
    def remove_by_sock(self, id):
        session = self.socket2session.get(id)
        if None != session:
            del self.socket2session[id]
            if None != self.all_session.get(session.get_id()):
                del self.all_session[session.get_id()]
                return 1
        return 0
    def foreach(self, func):
        for k, v in self.all_session.iteritems():
            func(v)
    def foreach_until(self, func):
        for k, v in self.all_session.iteritems():
            if True == func(v):
                return
g_session_mgr = session_mgr_t()
def get_session_mgr():
    return g_session_mgr
#区组名称
g_group_name = ''
def ff_set_group_name(group_name):
    global g_group_name
    g_group_name = group_name
    return 0
def ff_get_group_name():
    return g_group_name
class session_t:
    def __init__(self, socket_id_, ip, group_name, gate_name):
        self.socket_id   = socket_id_
        self.ip          = ip
        self.group_name  = group_name
        self.gate_name   = gate_name
        self.m_id        = 0
        self.m_name      = ''
        self.player      = None
        self.kf_flag     = False
        if self.group_name != '' and ff_get_group_name() != self.group_name and ff_get_group_name() != '':
            self.kf_flag = True
        #print('group_name:%s'%(ff_get_group_name()))
    #判断是否仍然有效
    def is_valid(self):
        if get_session_mgr().get_by_sock(self.socket_id):
            return True
        return False
    def verify_id(self, id_, extra_ = ''):
        self.m_id = id_
        if self.m_id != 0:
            get_session_mgr().add(self.m_id, self.socket_id, self)
        #print('verify_id', id_)
    def get_id(self):
        return self.m_id
    def get_name(self):
        return self.name
    def set_name(self, name):
        self.name = name
    def send_msg(self, cmd, ret_msg):
        if self.kf_flag == False:
            send_msg_session(self.gate_name, self.socket_id, cmd, ret_msg)
        else:
            kf_send_msg_session(self.group_name, self.gate_name, self.socket_id, cmd, ret_msg)
    def broadcast(self, cmd, ret_msg):
        ret_msg = encode_msg(ret_msg)
        def cb(tmp_session):
            tmp_session.send_msg(cmd, ret_msg)
        g_session_mgr.foreach(cb)
    def goto_scene(self, name_, msg_):
        change_session_scene(self.gate_name, self.socket_id, name_, encode_msg(msg_))
        get_session_mgr().remove(self.get_id())
    def goto_kf_scene(self, group_name, name_, msg_):
        change_session_kf_scene(group_name, self.gate_name, self.socket_id, name_, encode_msg(msg_))
        get_session_mgr().remove(self.get_id())
def on_verify(func_):
    global g_session_verify_callback
    g_session_verify_callback = func_
    return func_
def reg(cmd_, protocol_type_ = 'json'):
    def func_id_to_session(id):
        return g_session_mgr.get_by_sock(id)
    if protocol_type_ != 'json':
        set_py_cmd2msg(cmd_, protocol_type_.__name__)
    else:
        set_py_cmd2msg(cmd_, 'CMD=%d'%(cmd_))
    return session_call(cmd_, protocol_type_, func_id_to_session)

def on_login(cmd_, protocol_type_ = 'json'):
    def func_id_to_session(id):
        return g_session_mgr.get(id)
    if protocol_type_ != 'json':
        set_py_cmd2msg(cmd_, protocol_type_.__name__)
    else:
        set_py_cmd2msg(cmd_, 'CMD=%d'%(cmd_))
    return session_call(cmd_, protocol_type_, func_id_to_session)

def on_enter(msg_type):
    def wrap_enter(func_):
        global g_session_enter_callback
        def to_session(group_name, gate_name, socket_id, from_scene, extra_data):
            session_vefify = session_t(socket_id, '', group_name, gate_name)
            get_session_mgr().add_socket(socket_id, session_vefify)
            dest_msg = msg_type()
            decode_msg(dest_msg, extra_data)
            return func_(session_vefify, dest_msg)
        g_session_enter_callback = to_session
        return func_
    return wrap_enter
def on_logout(func_):
    global g_session_offline_callback
    def to_session(session_id):
        session = get_session_mgr().get_by_sock(session_id)
        if 1 == get_session_mgr().remove_by_sock(session_id):
            func_(session)
    g_session_offline_callback = to_session
    return func_


def ff_session_verify(cmd, session_body, socket_id, ip, gate_name):
    '''
    session_key 为client发过来的验证key，可能包括账号密码
    online_time 为上线时间
    gate_name 为从哪个gate登陆的
    '''
    info = g_session_logic_callback_dict.get(cmd)
    if None == info:
        print('cmd=%d not found'%(cmd))
        return 0
    arg  = info[0](session_body)
    convert_func = info[2]
    session_vefify = session_t(socket_id, ip, '', gate_name)
    get_session_mgr().add_socket(socket_id, session_vefify)
    return info[1](session_vefify, arg)


def ff_session_enter(group_name, gate_name, session_id, from_scene, extra_data):
    '''
    session_id 为client id
    from_scene 为从哪个scene过来的，若为空，则表示第一次进入
    extra_data 从from_scene附带过来的数据
    '''
    if g_session_enter_callback != None:
       return g_session_enter_callback(group_name, gate_name, session_id, from_scene, extra_data)

def ff_session_offline(session_id):
    '''
    session_id 为client id
    online_time 为上线时间
    '''
    if g_session_offline_callback != None:
       return g_session_offline_callback(session_id)

def ff_session_logic(session_id, cmd, body):
    '''
    session_id 为client id
    body 为请求的消息
    '''
    #print('ff_session_logic', session_id, cmd, body)
    info = g_session_logic_callback_dict.get(cmd)
    if None == info:
        print('cmd=%d not found'%(cmd))
        return 0
    arg  = info[0](body)
    convert_func = info[2]
    return info[1](convert_func(session_id), arg)

def ff_timer_callback(id):
    try:
        cb = g_timer_callback_dict[id]
        del g_timer_callback_dict[id]
        cb()
    except:
        import traceback
        print traceback.format_exc()
        return False

g_WriteTMemoryBuffer   = TTransport.TMemoryBuffer()
g_WriteTBinaryProtocol = TBinaryProtocol.TBinaryProtocol(g_WriteTMemoryBuffer)

def to_str(msg):
    if hasattr(msg, 'thrift_spec'):
        global g_WriteTMemoryBuffer, g_WriteTBinaryProtocol
        g_WriteTMemoryBuffer.cstringio_buf.truncate()
        g_WriteTMemoryBuffer.cstringio_buf.seek(0)
        ret = ''
        if g_protocol == 1:
            tmp_WriteTMemoryBuffer   = TTransport.TMemoryBuffer()
            proto = TJSONProtocol.TJSONProtocol(tmp_WriteTMemoryBuffer)
            proto.writeMessageBegin(msg.__class__.__name__, 0, 0);
            msg.write(proto)
            proto.writeMessageEnd();
            ret = tmp_WriteTMemoryBuffer.getvalue()
        else:
            msg.write(g_WriteTBinaryProtocol)
            ret = g_WriteTMemoryBuffer.getvalue()
        #print('tostr',len(ret), ret)
        return ret
    elif hasattr(msg, 'SerializeToString'):
        return msg.SerializeToString()
    elif isinstance(msg, unicode):
        return msg.encode('utf-8')
    elif isinstance(msg, str):
        return msg
    elif msg.__class__ == int or msg.__class__ == long or msg.__class__ == float:
        return str(msg)
    else:
        return json.dumps(msg, ensure_ascii=False)

def send_msg_session(gate_name, session_id, cmd_, body):
    if str != body.__class__:
        body = encode_msg(body)
    return ff.py_send_msg_session(gate_name, session_id, cmd_, body)
def kf_send_msg_session(group_name, gate_name, session_id, cmd_, body):
    return ff.py_kf_send_msg_session(group_name, gate_name, session_id, cmd_, to_str(body))

def multi_send_msg_session(session_id_list, cmd_, body):
    return ff.ffscene_obj.multicast_msg_session(session_id_list, cmd_, to_str(body))
#def broadcast_msg_session(cmd_, body):
#    return ff.py_broadcast_msg_session(cmd_, to_str(body))
#    return ff.ffscene_obj.broadcast_msg_session(cmd_, to_str(body))
def broadcast_msg_gate(gate_name_, cmd_, body):
    return ff.ffscene_obj.broadcast_msg_gate(gate_name_, cmd_, body)
def close_session(uid):
    session = get_session_mgr().get(uid)
    if session:
        ff.ffscene_obj.close_session(session.gate_name, session.socket_id)
def change_session_scene(gate_name, socket_id_, toSceneName, data_extra = ''):
    ff.ffscene_obj.change_session_scene(gate_name, socket_id_, toSceneName, data_extra)
def change_session_kf_scene(group_name, gate_name, socket_id_, toSceneName, data_extra = ''):
    ff.ffscene_obj.change_session_kf_scene(group_name, sgate_name, socket_id_, toSceneName, data_extra)

def reload(name_):
    if name_ != 'ff':
        return ff.ffscene_obj.reload(name_)

singleton_register_dict = {}
def singleton(type_):
    try:
	    return type_._singleton
    except:
        global singleton_register_dict
        name = type_.__name__
        obj  = singleton_register_dict.get(name)
        if obj == None:
            obj  = type_()
            singleton_register_dict[name] = obj
        type_._singleton = obj
        return obj


#数据库连接相关接口
DB_CALLBACK_ID = 0
DB_CALLBACK_DICT = {}
class ffdb_t(object):
    def __init__(self, host, id):
        self.host   = host
        self.db_id  = id
    def host(self):
        return self.host
    def query(self, sql_, callback_ = None):
        global DB_CALLBACK_DICT, DB_CALLBACK_ID
        DB_CALLBACK_ID += 1
        DB_CALLBACK_DICT[DB_CALLBACK_ID] = callback_
        ff.ffscene_obj.db_query(self.db_id, sql_, DB_CALLBACK_ID)
    def sync_query(self, sql_):
        ret = ff.ffscene_obj.sync_db_query(self.db_id, sql_)
        if len(ret) == 0:
            return query_result_t(False, [], [])
        col = ret[len(ret) - 1]
        data = []
        for k in range(0, len(ret)-1):
            data.append(ret[k])
        return query_result_t(True, data, col)
#封装query返回的结果
class query_result_t(object):
    def __init__(self, flag_, result_, col_):
        self.flag    = flag_
        self.result  = result_
        self.column  = col_
    def dump(self):
        print(self.flag, self.result, self.column)

#C++ 异步执行完毕db操作回调
def ff_db_query_callback(callback_id_, flag_, result_, col_):
    global DB_CALLBACK_DICT
    cb = DB_CALLBACK_DICT.get(callback_id_)
    del DB_CALLBACK_DICT[callback_id_]
    #print('db_query_callback', callback_id_, flag_, result_, col_)
    if cb != None:
        ret = query_result_t(flag_, result_, col_)
        cb(ret)

# 封装异步操作数据库类
def ffdb_create(host_):
    db_id = ff.ffscene_obj.connect_db(host_)
    if db_id == 0:
        return None
    return ffdb_t(host_, db_id)


#封装escape操作
def escape(s_):
    return ff.escape(s_)


g_py_service_func_dict = {}
# c++ 调用的
def ff_scene_call(cmd_, msg_):
    func = g_py_service_func_dict.get(cmd_)
    ret = func(msg_)
    if ret != None:
        if hasattr(ret, 'SerializeToString2'):
            return [ret.__name__, ret.SerializeToString()]
        elif isinstance(ret, unicode):
            return ['json', ret.encode('utf-8')]
        elif isinstance(ret, str):
            return ['json', ret]
        else:
            return ['json', json.dumps(ret, ensure_ascii=False)]
    return ['', '']

# 注册接口
def reg_service(cmd_, msg_type_ = None):
    def bind_func(func_):
        def impl_func(msg_):
            if None == msg_type_:
                return func_(json_to_value(msg_))
            else: #protobuf
                return func_(protobuf_to_value(msg_type_, msg_))

        global g_py_service_func_dict
        g_py_service_func_dict[cmd_] = impl_func
        return func_
    return bind_func

#将python的标准输出导出到日志
save_stdout = None
class mystdout_t(object):
    def write(self, x):
        if x == '\n':
            return 1
        ff.ffscene_obj.pylog(6, 'FFSCENE_PYTHON', x)
        return len(x)
def dump_stdout_to_log():
    save_stdout = sys.stdout
    sys.stdout = mystdout_t()

#分配id
G_ALLOC_ID = 0
def alloc_id():
    global G_ALLOC_ID
    G_ALLOC_ID += 1
    return G_ALLOC_ID


#日志相关的接口
def LOGTRACE(mod_, content_):
    return ff.ffscene_obj.pylog(6, mod_, content_)
def LOGDEBUG(mod_, content_):
    return ff.ffscene_obj.pylog(5, mod_, content_)
def LOGINFO(mod_, content_):
    return ff.ffscene_obj.pylog(4, mod_, content_)
def LOGWARN(mod_, content_):
    return ff.ffscene_obj.pylog(3, mod_, content_)
def LOGERROR(mod_, content_):
    return ff.ffscene_obj.pylog(2, mod_, content_)
def LOGFATAL(mod_, content_):
    return ff.ffscene_obj.pylog(1, mod_, content_)

def ERROR(content_):
    return LOGERROR('PY', content_)

def TRACE(content_):
    return LOGTRACE('PY', content_)
def INFO(content_):
    return LOGINFO('PY', content_)
def WARN(content_):
    return LOGWARN('PY', content_)

#时间相关的操作
def get_time():
    return time.time()

def datetime_now():
    return datetime.datetime.now()#fromtimestamp(time.time())

def datetime_from_time(tm_):
    datetime.datetime.fromtimestamp(tm_)

def datetime_from_str(s_):
    return datetime.datetime.strptime(s_, '%Y-%m-%d %H:%M:%S')

def datetime_to_str(dt_):
    return dt_.strftime('%Y-%m-%d %H:%M:%S')

def get_config(key_):
    return ff.py_get_config(key_)



g_reg_scene_interface_dict = {}
g_call_service_wait_return_dict = {}
g_call_service_id   = 1

#ffrpc对象
class ffreq_t:
    def __init__(self, callback_id):
        self.msg = None
        self.err = None
        self.callback_id = callback_id
    def response(self, msg):
        if self.callback_id == 0:
            return
        if msg.__class__ == str or msg.__class__ == list or msg.__class__ == dict:
            return ff.ffscene.rpc_response(self.callback_id, '', encode_msg(msg))
        else:
            return ff.ffscene.rpc_response(self.callback_id, msg.__class__.__name__, encode_msg(msg))
    def __repr__(self):
        return 'msg:'+ `self.msg`+',err:'+str(self.err)

#注册scene接口修饰器
def ff_reg_scene_interfacee(req_msg, ret_msg = '', name_space_ = ''):
    global g_reg_scene_interface_dict
    def json_interface_wrap(func):
        def json_process(body, callback_id):
            ffreq = ffreq_t(callback_id)
            ffreq.msg = json_to_value(body)
            func(ffreq)
        g_reg_scene_interface_dict[req_msg] = json_process
        ff.ffscene.reg_scene_interface(req_msg)
        return func
    def bin_interface_wrap(func):
        def bin_process(body, callback_id):
            ffreq = ffreq_t(callback_id)
            ffreq.msg = req_msg()
            decode_msg(ffreq.msg, body)
            func(ffreq)
        g_reg_scene_interface_dict[req_msg.__name__] = bin_process
        ff.ffscene.reg_scene_interface(req_msg.__name__)
        return func
    if hasattr(req_msg, 'thrift_spec') or hasattr(req_msg, '__name__'):
        return bin_interface_wrap
    elif req_msg.__class__ == str:
        return json_interface_wrap

        
#外部调用scene的接口 
def ff_call_scene_interface(name, body, callback_id):
    func = g_reg_scene_interface_dict.get(name)
    if None == func:
        print('dest name=%s not found'%(name))
        return
    print('ff_call_scene_interface body_len=%d'%(len(body)))
    func(body, callback_id)
    
#调用外部的接口 
class ff_rpc_callback_t:
    def __init__(self, cb, msg_type):
        self.cb = cb
        self.msg_type = msg_type
    def exe(self, body, err):
        ffreq = ffreq_t(0)
        ffreq.msg = self.msg_type()
        ffreq.msg = decode_msg(ffreq.msg, body)
        self.cb(ffreq)
def ff_rpc_callback(cb, msg):
    return ff_rpc_callback_t(cb, msg)
        
def ff_rpc_call(service_name, msg, cb, msg_head = '', name_space_ = ''):
    global g_call_service_id
    tmp_name = msg_head
    tmp_body = msg
    if msg.__class__ != str:
        tmp_name += msg.__class__.__name__
        tmp_body  = encode_msg(msg)
    cb_id = 0
    if cb:
        g_call_service_id += 1
        cb_id = g_call_service_id
        if cb.__class__ == ff_rpc_callback_t:
            def callback_func(body, err):
                cb.exe(body, err)
            g_call_service_wait_return_dict[g_call_service_id] = callback_func
        else:
            def callback_func(body, err):
                cb(json_to_value(body))
            g_call_service_wait_return_dict[g_call_service_id] = callback_func
    ff.ffscene.call_service(name_space_, service_name, tmp_name, tmp_body, cb_id)
# c++ 调用的
def ff_rpc_call_return_msg(id_, body_, err_):
    func = g_call_service_wait_return_dict.get(id_)
    if func != None:
        del g_call_service_wait_return_dict[id_]
        func(body_, err_)
        return 0
    return -1

# 投递任务消息
def post_task(mod_name, msg_name, args, cb):
    global g_call_service_id
    cb_id = 0
    if cb:
        g_call_service_id += 1
        cb_id = g_call_service_id
        if cb.__class__ == ff_rpc_callback_t:
            def callback_func(body, err):
                cb.exe(body, err)
            g_call_service_wait_return_dict[g_call_service_id] = callback_func
        else:
            def callback_func(body, err):
                cb(body)
            g_call_service_wait_return_dict[g_call_service_id] = callback_func
    ff.post_task(mod_name, msg_name, args, cb_id)
    return
# 投递任务消息 回调  
def process_task_callback(body_, id_):
    func = g_call_service_wait_return_dict.get(id_)
    if func != None:
        del g_call_service_wait_return_dict[id_]
        func(body_, '')
        return 0
    return -1

g_bind_task_func_dict = {}
# 注册处理任务的接口
def bind_task(task_name):
    global g_bind_task_func_dict
    def wrap(func):
        g_bind_task_func_dict[task_name] = func
        return func
    return wrap
#处理外部投递任务消息
def process_task(task_name, args, from_name, cb_id):
    func = g_bind_task_func_dict.get(task_name)
    def cb(ret_args):
        if cb_id != 0:
            return ff.py_task_callback(from_name, ret_args, cb_id)
    if func:
        func(args, cb)
    return 0