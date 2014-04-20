# -*- coding: utf-8 -*-
import ffext

class idgen_t:
    def __init__(self, db_host_, type_id_ = 0, server_id_ = 0):
        self.type_id = type_id_
        self.server_id = server_id_
        self.auto_inc_id = 0
        self.db_host = db_host_
        self.db      = None
        self.saving_flag = False
        self.runing_flag = 0
    def init(self):
        self.db = ffext.ffdb_create(self.db_host)
        if None == self.db:
            print("数据库连接出错")
            return False
        ret = self.db.sync_query("SELECT `AUTO_INC_ID`, `RUNING_FLAG` FROM `id_generator` WHERE `TYPE` = '%d' AND `SERVER_ID` = '%d'" % (self.type_id, self.server_id))
        #print(ret.flag, ret.result, ret.column)
        if len(ret.result) == 0:
            #数据库中还没有这一行，插入
            self.db.sync_query("INSERT INTO `id_generator` SET `AUTO_INC_ID` = '0',`TYPE` = '%d', `SERVER_ID` = '%d', `RUNING_FLAG` = '1' " % (self.type_id, self.server_id))
            return True
        else:
            self.auto_inc_id = int(ret.result[0][0])
            self.runing_flag = int(ret.result[0][1])
            if self.runing_flag != 0:
                self.auto_inc_id += 10000
                ffext.ERROR('last idgen shut down not ok, inc 10000')
            self.db.sync_query("UPDATE `id_generator` SET `RUNING_FLAG` = '1' WHERE `TYPE` = '%d' AND `SERVER_ID` = '%d'" % (self.type_id, self.server_id))
        #if self.auto_inc_id < 65535:
        #    self.auto_inc_id = 65535
        return True
    def cleanup(self):
        db = ffext.ffdb_create(self.db_host)
        now_val = self.auto_inc_id
        db.sync_query("UPDATE `id_generator` SET `AUTO_INC_ID` = '%d', `RUNING_FLAG` = '0' WHERE `TYPE` = '%d' AND `SERVER_ID` = '%d'" % (now_val, self.type_id, self.server_id))
        print('id_generator cleanup ok')
        return True
    def gen_id(self):
        self.auto_inc_id += 1
        self.update_id()
        #low16 = self.auto_inc_id & 0xFFFF
        #high  = (self.auto_inc_id >> 16) << 32
        #return high | (self.server_id << 16)| low16
        return self.auto_inc_id
    def dump_id(self, id_):
        low16 = id_ & 0xFFFF
        high  = id_ >> 32
        return high << 16 | low16
    def update_id(self):
        if True == self.saving_flag:
            return
        self.saving_flag = True
        now_val = self.auto_inc_id
        def cb(ret):
            #print(ret.flag, ret.result, ret.column)
            self.saving_flag = False
            if now_val < self.auto_inc_id:
                self.update_id()
        self.db.query("UPDATE `id_generator` SET `AUTO_INC_ID` = '%d' WHERE `TYPE` = '%d' AND `SERVER_ID` = '%d' AND `AUTO_INC_ID` < '%d'" % (now_val, self.type_id, self.server_id, now_val), cb)
        return

id_generator = None
item_id_generator = None
def init(host_):
    global id_generator, item_id_generator
    id_generator = idgen_t(host_)
    item_id_generator = idgen_t(host_, 1)
    if False == id_generator.init():
        return False
    return item_id_generator.init()

def cleanup():
    global id_generator, item_id_generator
    if None != id_generator:
        id_generator.cleanup()
        id_generator = None
    if None != item_id_generator:
        item_id_generator.cleanup()
        item_id_generator = None
    return True

def alloc_id():
    global id_generator
    return id_generator.gen_id()

def alloc_item_id():
    global item_id_generator
    return item_id_generator.gen_id()