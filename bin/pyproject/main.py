# -*- coding: utf-8 -*-
import ffext
import handler.player_handler
from model import db_service

import echo_msg_def.ttypes as ttypes

def init():
    print('init......')
    if db_service.init() == False:
        return -1
    return 0
    

def cleanup():
    print('cleanup.....')
    return 0
    

@ffext.ff_reg_scene_interfacee(ttypes.echo_thrift_in_t, ttypes.echo_thrift_out_t)
def test_echo(req):
    
    ret_msg = ttypes.echo_thrift_out_t()
    ret_msg.data = req.msg.data
    req.response(ret_msg)
    #req.response('hahahahahah')
    #print('test_echo', req.msg, ret_msg)
    return

def ff_test(a):
    
    msg = ttypes.echo_thrift_in_t()
    ffext.decode_msg(msg, a)
    #print('ff_test', msg)
    return a
    
    