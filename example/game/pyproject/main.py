# -*- coding: utf-8 -*-
import ffext
import handler.player_handler
from model import db_service

def init():
    print('init......')
    if db_service.init() == False:
        return -1

    return 0
    

def cleanup():
    print('cleanup.....')
    return 0
