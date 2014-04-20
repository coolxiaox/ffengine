# -*- coding: utf-8 -*-
import ffext
import ff
from user_data import ttypes
import time
import msg_def.ttypes as msg_def
import random

MAX_X = 800
MAX_Y = 640
MOVE_SPEED = 3

class player_t:
    def __init(self):
        self.id = 0
        self.name = ''
        self.x = 0
        self.y = 0

def init():
    print('init......')
    return 0
    def cb():
        print('timer.....')
        def task_cb(data):
            print('task cb', data)
        ffext.post_task('scene@1', 'test_task', [1,2,3,4,5,6], task_cb)
        #ff.post_task('fflua@1', 'test', [1,2,3,4,5,6], 0)
        def foo(ffreq):
            print('foo', ffreq)
        #ffext.ff_rpc_call('scene@0', '[1,2,3,444,555]', foo, 'test')
        msg = ttypes.friend_t()
        #ffext.ff_rpc_call('scene@0', msg, ffext.ff_rpc_callback(foo, ttypes.friend_t))
        return
    ffext.once_timer(1000, cb)
    return 0


def cleanup():
    print('cleanup.....')
    return 0

@ffext.ff_reg_scene_interfacee('ff::test')
def process_test(ffreq):
    print('pyprocess_test', ffreq.msg)
    ffreq.response(ffreq.msg)

@ffext.ff_reg_scene_interfacee(ttypes.friend_t, ttypes.friend_t)
def process_friend(ffreq):
    print('process_friend', ffreq.msg)
    ffreq.response(ffreq.msg)


@ffext.bind_task('py_task')
def py_test_task(args, cb):
    print('py_test_task', args)
    cb(args)

@ffext.on_verify
def real_session_verify(session_verify): #session_key,
    print('real_session_verify')
    session_verify.verify_id(ffext.alloc_id())
    return []

g_move_offset = {
    "LEFT_ARROW":(-1, 0),
    "RIGHT_ARROW":(1, 0),
    "UP_ARROW":(0, -1),
    "DOWN_ARROW":(0, 1),
}

@ffext.reg(msg_def.client_cmd_e.INPUT_REQ, msg_def.input_req_t)
def process_move(session, msg):
    print('process_move', msg.ops)
    offset = g_move_offset.get(msg.ops)
    if None == offset:
        return
    session.player.x += offset[0] * MOVE_SPEED
    session.player.y += offset[1] * MOVE_SPEED
    if session.player.x < 0:
        session.player.x = 0
    if session.player.y < 0:
        session.player.y = 0

    ret_msg     = msg_def.input_ret_t()
    ret_msg.id  = session.get_id()
    ret_msg.ops = msg.ops
    ret_msg.x   = session.player.x
    ret_msg.y   = session.player.y
    print('input', ret_msg)

    session.broadcast(msg_def.server_cmd_e.INPUT_RET, ret_msg)
    return

g_alloc_names = ['ºì', 'Ã÷', '¾ü', 'À¼', 'Öí', 'éª', 'Î¢', '·¼', 'Èç', 'ÐÜ', 'Å£', 'ÍÃ', 'Æß', '»Æ']
@ffext.on_enter
def real_session_enter(session, from_scene, extra_data):
    session.player = player_t()
    session.player.id = session.get_id()
    session.player.name = 'Ð¡' + g_alloc_names[session.get_id() % (len(g_alloc_names))]
    session.player.x = 100 + random.randint(0, 300)
    session.player.y = 100 + random.randint(0, 300)
    
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

@ffext.on_logout
def process_logout(session):
    ret_msg = msg_def.logout_ret_t(session.get_id())
    session.broadcast(msg_def.server_cmd_e.LOGOUT_RET, ret_msg)
    print('process_logout', ret_msg)