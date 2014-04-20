# -*- coding: utf-8 -*-
import ffext
import json
from msg.ffengine import ttypes 
#from msg_def import ttypes

import random
import os
import time

def init():
    print('init......')
    return 0
    

def cleanup():
    print('cleanup.....')
    return 0


#@ffext.ff_reg_scene_interfacee(ttypes.echo_thrift_in_t, ttypes.echo_thrift_out_t)
def process_echo(ffreq):
    print('process_echo', ffreq.msg)
    ret_msg = ttypes.echo_thrift_out_t('TODO')
    ffreq.response(ret_msg)

@ffext.ff_reg_scene_interfacee(ttypes.process_cmd_req_t, ttypes.process_cmd_ret_t)
def process_echo(ffreq):
    print('process_echo cmd=%s'%(ffreq.msg.cmd))
    print('process_echo', ffreq.msg, {})
    ret_msg = ttypes.process_cmd_ret_t(1, '暂未开放此功能')
    ret_msg.ret_code = 1
    ret_msg.output_msg = ''
    ret_msg.info = {}
    if ffreq.msg.cmd == '查看状态':
        ret_msg.output_msg = '关闭'
    elif ffreq.msg.cmd == '启动':
        init_dir(ffreq.msg.process_name)
        config_name = gen_filename(ffreq.msg.process_name)
        app_name    = gen_app_name(ffreq.msg.process_name)
        f = open(config_name, 'w')
        if ffreq.msg.param != None:
            for k, v in ffreq.msg.param.items():
                f.write(k + ' ' + v + '\n')
            tmpS = "-log_path ./%s/log -log_filename log "%(ffreq.msg.process_name)+\
                   "-log_class DB_MGR,XX,BROKER,FFRPC,FFGATE,FFSCENE,FFSCENE_PYTHON,FFNET "+\
                   "-log_print_screen true -log_print_file true -log_level 6"
            f.write(tmpS+'\n')
            #f.write('-perf_timeout 20\n')
            f.write('-perf_path %s/perf\n'%(ffreq.msg.process_name))
            f.write('-db sqlite://./ff_demo.db\n')
            if ffreq.msg.param.get('-scene') != None:
                f.write('-python_path ./%s/pyproject \n'%(ffreq.msg.process_name))
        f.close()
        os.system(app_name)
        print(app_name)
        if ps_app(ffreq.msg.process_name):
            time.sleep(1)
            if ps_app(ffreq.msg.process_name):
                ret_msg.output_msg = ffreq.msg.process_name +'启动成功'
            else:
                ret_msg.ret_code = 0
                ret_msg.output_msg = ffreq.msg.process_name +'启动失败'
        else:
            ret_msg.ret_code = 0
            ret_msg.output_msg = ffreq.msg.process_name +'启动失败'
    elif ffreq.msg.cmd == '关闭':
        pid = ps_app(ffreq.msg.process_name)
        print('pid', pid)
        if pid == 0:
            ret_msg.output_msg = ffreq.msg.process_name +'该进程并未运行'
        elif kill_app(ffreq.msg.process_name, pid):
            ret_msg.output_msg = ffreq.msg.process_name +'关闭成功'
        else:
            ret_msg.output_msg = ffreq.msg.process_name +'关闭失败'
    elif ffreq.msg.cmd == '更新代码':
        print(ffreq.msg.param['svn_url'])
        ret_msg.output_msg = '代码更新成功:\n'+update_code(ffreq.msg.process_name, ffreq.msg.param['svn_url'])
    elif ffreq.msg.cmd == '查看日志':
        ret_msg.info['update_data'] = dump_path(ffreq.msg.process_name + '/log/'+ffreq.msg.param['path'])
    elif ffreq.msg.cmd == '查看代码':
        ret_msg.info['update_data'] = dump_path(ffreq.msg.process_name + '/pyproject/'+ffreq.msg.param['path'])
    elif ffreq.msg.cmd == '性能日志':
        ret_msg.info['update_data'] = dump_path(ffreq.msg.process_name + '/'+ffreq.msg.param['path'])
    elif ffreq.msg.cmd == '进程状态':
        pid = ps_app(ffreq.msg.process_name)
        print('pid', pid)
        if pid != 0:
            ret_msg.output_msg = ffreq.msg.process_name +'该进程正在运行'
            ret_msg.info['update_data'] = 'running'
        else:
            ret_msg.output_msg = ffreq.msg.process_name +'该进程并未运行'
            ret_msg.info['update_data'] = 'not running'
    elif ffreq.msg.cmd == '删除进程':
        cmd = 'rm -rf ' + ffreq.msg.process_name
        os.system(cmd)
        print(cmd)
    else:
        ret_msg.ret_code = 0
        ret_msg.output_msg = '当前后台不支持此操作'
    
    
    ffreq.response(ret_msg)

def init_dir(s):
    os.system('mkdir -p %s'%(s))
    os.system('mkdir -p %s/perf'%(s))
def gen_filename(s):
    return './' + s + '/default.config'
def gen_app_name(s):
    return './app_ffengine_ctrl -f ./' + s + '/default.config -d '
def ps_app(s):
    cmd = 'ps aux|grep ' + s + '  > tmp.dump'
    os.system(cmd)
    print(cmd)
    f = open('tmp.dump')
    dataStr = f.read()
    f.close()
    for data in dataStr.split('\n'):
        if data.find('app_ffengine_ctrl') >= 0:
            print('ps_app', data)
            col_list = []
            pid_list = data.split('app_game')[0].split(' ')
            for k in pid_list:
                if k != '':
                    col_list.append(k)
            return int(col_list[1])
    return 0
def kill_app(s, pid):
    if pid == 0:
        print('进程已经关闭')
        return True
    cmd = 'kill %d'%(pid)
    os.system(cmd)
    print(cmd)
    for k in range(0, 10):
        if 0 == ps_app(s):
            return True
    return False

def update_code(app_name, url):
    os.system('rm -rf ./tmp_svn_dir')
    os.system('mkdir -p %s ./%s/pyproject'%(app_name, app_name))
    os.system('svn export %s tmp_svn_dir > tmp.dump' % (url))
    os.system('cp -rf ./tmp_svn_dir/* ./%s/pyproject'%(app_name))
    f = open('./tmp.dump')
    a = f.readlines() 
    f.close()
    if len(a) > 0:
        ver = a[len(a) -1]
        return ver
    return ""
def dump_path(path):
    #print('process_dump', ffreq.msg)
    ret_msg = {}
    ret_msg['flag'] = 1 #dir =1， file=2
    ret_msg['dir']  = []
    ret_msg['data'] = ''
    prefix = "./"
    path_name = prefix + path
    print(path_name)
    raw_flag = False
    if path_name[-1:] == '@':
        raw_flag = True
        path_name = path_name[0:len(path_name)-1]
    if os.path.isdir(path_name):
        list_files = os.listdir(path_name)
        for k in list_files:
            if k != '' and k[0:1] != '.' and k[0:4] != 'app_' and k[-2:] != '.o' and k[-4:] != '.pyc':
                ret_msg['dir'].append(k)
    elif os.path.isfile(path_name):
        ret_msg['flag'] = 2
        pre_code = ''
        end_code = ''
        f = open(path_name, 'r')
        
        #if path_name[-4:] == '.lua':
        #    pre_code = '<textarea  cols="80" rows="20" class="pane"><pre class=\'brush: lua\'>'
        #    end_code = '</pre></textarea>'
        #elif path_name[-3:] == '.py':
        #    pre_code = '<textarea  cols="80" rows="20" class="pane"><pre class=\'brush: python\'>'
        #    end_code = '</pre></textarea>'
        data = pre_code
        for line in f.readlines():
           if raw_flag:
               data += line
           else:
               data += line + "\n\n" 
        f.close()
        if data[-1:] == '\n':
            data = data[0:-1]
        data += end_code
        ret_msg['data'] = data
    else:
        ret_msg['flag'] = 0
	
    #print('process_dump ret_msg', ret_msg)
    tmpJson = json.dumps(ret_msg, ensure_ascii=False)
    print('process_dump ret_msg', tmpJson)
    return tmpJson

#@ffext.ff_reg_scene_interfacee(ttypes.dump_file_req_t, ttypes.dump_file_ret_t)
def process_dump(ffreq):
    print('process_dump', ffreq.msg)
    ret_msg = ttypes.dump_file_ret_t(0, '', [])
    ret_msg.flag = 1 #dir =1， file=2
    
    prefix = "./"
    path_name = prefix + ffreq.msg.name
    print(path_name)
    raw_flag = False
    if path_name[-1:] == '@':
        raw_flag = True
        path_name = path_name[0:len(path_name)-1]
    if os.path.isdir(path_name):
        list_files = os.listdir(path_name)
        for k in list_files:
            if k != '' and k[0:1] != '.' and k[0:4] != 'app_' and k[-2:] != '.o':
                ret_msg.dir.append(k)
    elif os.path.isfile(path_name):
        ret_msg.flag = 2
        pre_code = ''
        end_code = ''
        f = open(path_name, 'r')
        
        #if path_name[-4:] == '.lua':
        #    pre_code = '<textarea  cols="80" rows="20" class="pane"><pre class=\'brush: lua\'>'
        #    end_code = '</pre></textarea>'
        #elif path_name[-3:] == '.py':
        #    pre_code = '<textarea  cols="80" rows="20" class="pane"><pre class=\'brush: python\'>'
        #    end_code = '</pre></textarea>'
        data = pre_code
        for line in f.readlines():
           if raw_flag:
               data += line
           else:
               data += line + "<br/>" 
        f.close()
        if data[-1:] == '\n':
            data = data[0:-1]
        data += end_code
        ret_msg.data = data
    else:
        ret_msg.flag = 0
	
    ffreq.response(ret_msg)
    print('process_dump ret_msg', ret_msg)
