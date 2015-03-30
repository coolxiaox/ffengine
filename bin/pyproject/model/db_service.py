# -*- coding: utf-8 -*-

import ffext
import id_generator

g_async_db = None
def get_async_db():
    global g_async_db
    return g_async_db

def init():
    global g_async_db
    #g_sync_db = ffext.ffdb_create('mysql://localhost:3306/root/acegame/pcgame')
    db_config = ffext.get_config('-db')
    print('db_config=%s'%(db_config))
    g_async_db = ffext.ffdb_create(db_config)
    if None == g_async_db:
        return False
    sql = '''create table IF NOT EXISTS player_info
    (
      UID bigint not null,
      NAME varchar(255) not null,
      X int not null,
      Y int not null,
      primary key(UID)
    )'''
    g_async_db.query(sql)
    return True

def load_by_name(name_, callback):
    sql = "select * from player_info where NAME='%s' limit 1"%(name_)
    def cb(ret):
        retData = {}
        if ret.flag == False or len(ret.result) == 0:
            callback(retData)
            return
        for i in range(0, len(ret.column)):
            retData[ret.column[i]] = ret.result[0][i]
        callback(retData)
        return
    get_async_db().query(sql, cb)
    return

def insert_player(field_data):
    sql = ''
    values = ''
    for k, v in field_data.iteritems():
        if sql == '':
           sql = "insert into player_info ({0}".format(k)
           values = ") values ('{0}'".format(v)
        else:
           sql += ", {0}".format(k)
           values += ", '{0}'".format(v)
    sql += values + ')'
    print(sql)
    g_async_db.query(sql)
    return

def update_player(uid, field_data):
    sql = ''
    for k, v in field_data.iteritems():
        if sql == '':
           sql = "update player_info set {0} = '{1}'".format(k, v)
        else:
           sql += ", {0} = '{1}'".format(k, v)
    sql += " where uid = '{0}'".format(uid)
    print(sql)
    g_async_db.query(sql)
    return

