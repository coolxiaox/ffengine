# -*- coding: utf-8 -*-
import ffext
import time
import random

import msg_def.ttypes as msg_def
from model import db_service
from model.player_model import player_t

MAX_X = 800
MAX_Y = 640
MOVE_SPEED = 3

g_move_offset = {
    "LEFT_ARROW":(-1, 0),
    "RIGHT_ARROW":(1, 0),
    "UP_ARROW":(0, -1),
    "DOWN_ARROW":(0, 1),
}
g_alloc_names = ['小红', '小明', '小军', '小兰', '小猪', '小楠', '小微', '小芳', '小如', '小熊',
                 '小牛', '小兔', '小七', '小黄']

@ffext.on_login(msg_def.client_cmd_e.LOGIN_REQ, msg_def.input_req_t)
def process_login(session, msg): #session_key,
    print('real_session_verify')
    #本demo随便取了几个角色名，正常应该是用户输入
    req_name = g_alloc_names[ffext.alloc_id() % (len(g_alloc_names))] 
    
    def callback(db_data):
        #异步载入数据库，需要验证session是否仍然有效，有可能下线了
        if session.is_valid() == False:
            return
        session.player = player_t()
        session.player.name = req_name
        if len(db_data) == 0:
            session.verify_id(ffext.alloc_id())

            session.player.id = session.get_id()
            session.player.x = 100 + random.randint(0, 100)
            session.player.y = 100 + random.randint(0, 100)

            #创建新角色
            new_data = {
                'UID':session.player.id,
                'NAME':session.player.name,
                'X':session.player.x,
                'Y':session.player.y,
            }
            db_service.insert_player(new_data)
            print('create new player =%s'%(session.player.name))
        else:
            #载入数据
            print(db_data)
            print('load player =%s'%(session.player.name))
            session.player.id = int(db_data['UID'])
            session.player.x = int(db_data['X'])
            session.player.y = int(db_data['Y'])
            session.verify_id(session.player.id)
        #发消息通知
        ret_msg = msg_def.login_ret_t()
        ret_msg.id = session.get_id()
        ret_msg.x  = session.player.x
        ret_msg.y  = session.player.y
        ret_msg.name = session.player.name
        ret_msg.speed = MOVE_SPEED
        
        session.broadcast(msg_def.server_cmd_e.LOGIN_RET, ret_msg)
        print('real_session_enter', ret_msg)
        
        def notify(tmp_session):
            if tmp_session != session:
                ret_msg.id = tmp_session.get_id()
                ret_msg.x  = tmp_session.player.x
                ret_msg.y  = tmp_session.player.y
                ret_msg.name = tmp_session.player.name
                session.send_msg(msg_def.server_cmd_e.LOGIN_RET, ret_msg)
        ffext.get_session_mgr().foreach(notify)
        return
    #异步载入数据库数据
    db_service.load_by_name(req_name, callback)
    return


@ffext.reg(msg_def.client_cmd_e.INPUT_REQ, msg_def.input_req_t)
def process_move(session, msg):
    print('process_move %s %s'%(session.player.name, msg.ops))
    now_tm = time.time()
    if now_tm - session.player.last_move_tm < 0.1:
        print('move too fast')
        return
    session.player.last_move_tm = now_tm
    offset = g_move_offset.get(msg.ops)
    if None == offset:
        return
    session.player.x += offset[0] * MOVE_SPEED
    session.player.y += offset[1] * MOVE_SPEED
    if session.player.x < 0:
        session.player.x = 0
    elif session.player.x > MAX_X:
        session.player.x = MAX_X
    if session.player.y < 0:
        session.player.y = 0
    elif session.player.y > MAX_Y:
        session.player.y = MAX_Y

    ret_msg     = msg_def.input_ret_t()
    ret_msg.id  = session.get_id()
    ret_msg.ops = msg.ops
    ret_msg.x   = session.player.x
    ret_msg.y   = session.player.y
    print('input', ret_msg)

    session.broadcast(msg_def.server_cmd_e.INPUT_RET, ret_msg)
    #下面的代码模拟的是调scene，goto_scene直接完成调scene，so easy!
    #由于本demo没有启动多个scene，所以这里只是把player对象传过去，没有给client发送player消失消息
    goto_msg = msg_def.input_req_t()
    goto_msg.ops = '%d_%d_%d_%s'%(session.get_id(), session.player.x, session.player.y,
                                  session.player.name)
    
    print('goto_msg:', goto_msg)
    session.goto_scene('scene@0', goto_msg)
    return

@ffext.on_logout
def process_logout(session):
    ret_msg = msg_def.logout_ret_t(session.get_id())
    session.broadcast(msg_def.server_cmd_e.LOGOUT_RET, ret_msg)
    
    db_data = {
        'X': session.player.x,
        'Y': session.player.y,
    }
    db_service.update_player(session.get_id(), db_data)
    print('process_logout', ret_msg, session.socket_id)


@ffext.on_enter(msg_def.input_req_t)
def process_enter(session, msg):
    session.player = player_t()
    param = msg.ops.split('_')
    session.player.id = int(param[0])
    session.verify_id(session.player.id)
    
    session.player.x = int(param[1])
    session.player.y = int(param[2])
    session.player.name = param[3]
    print('process_enter', msg, session.socket_id)


