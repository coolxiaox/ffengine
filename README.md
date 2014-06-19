ffengine
========

ffengine是高效的tcp 服务器框架，适用于端游、页游、手游的实时服务器

## ffengine Feature
 *  组件化，ffengine 包含两个重要组件gate和scene，但是集成在一个进程中，既可以把两个组件启动到一个进程中，也可以启动到多个进程中，甚至启动到分布式环境，只需要修改一下ip，这得益于ffengine使用了原创的ffrpc通信库，使得进程间通讯非常的高效和简洁。
 *  通信异步化，组件之间的通信都是异步完成的，不会阻塞主线程，这样ffengine采用的单（主）线程的架构技能发挥稳定和简单的特点，又能保持高效，最大程度发挥IO的效率。
 *  数据库异步化，我们知道内存计算是飞快的，但是数据库操作设计到io，效率低一个数量级，所以ffengine数据库的访问采用异步加回调的方式，加上python 对于lambda的完美支持，异步加回调非常的容易。
 *  多协议支持，做游戏的对于protobuff 和 thrift 都非常熟悉了，支持pb 和 thrift 必须的。
 *  异步日志组件，包含trace、debug、info、warn、error、fatal等多种日志级别，不同的日志级别显示的颜色不同（独家创新）！
 *  独立组件复用，游戏中常见的组件，比如全局id生成器，任务系统，统计等，ffengine正在开发更多的优秀组件。
 *  ffengine 默认使用python 作为开发的主语言，1方面本引擎将io操作都已异步化，python用于逻辑计足够高效，并且python开发逻辑相当的敏捷，这对于页游和手游项目是非常宝贵的。另外对于像阿里云这种平台，python 更易于集成到云中，阿里云正在推游戏云，个人觉得ffengine 是对于阿里游戏云的非常好的补充，希望有一天ffengine能够构建出成熟的游戏服务器云平台，本人自己开发的开源私有服务器云平台http://ffown.sinaapp.com 大家可以先体验体验。
 *  实时性能监控，人们常常抱怨脚本语言运行慢，其实很大程度上是开发者在开发的过程中对于性能不敏感，刚开始项目小的时候运行很快，但是项目大了，运行就慢了，又找不到原因，简单归结于语言是不正确的。ffengine很大程度上从基础设施上支持性能数据采集，ffengine的每个接口都会被记录性能数据，每隔一段时间（默认一小时）输出汇总数据到文件中，可以通过工具可以图形化查看。这样系统慢了我们可以很清晰的知道是哪个接口导致的，同时当系统出现意外的卡顿时，可以方便查询问题，比如数据库变慢了，查看性能接口数据就可以知道，另外有些接口在某些时段会变慢，这个都可以通过性能数据很好的分析出来。
 *  

## ffengine example 构建一个服务器只需要三步
 *  使用@ffext.on_login 处理玩家登陆游戏
 *  使用@ffext.on_logout 处理玩家退出游戏
 *  使用@ffext.reg 处理玩家的逻辑操作
 

``` python

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


```

## ffengine Install 
操作系统：Linux Centos
数据库： Mysql-client
Python：2.6 ~ 2.7

1. 下载代码：
    svn checkoout https://github.com/fanchy/ffengine
2. 编译
   cd bin
   make

3. 修改配置
    default.config 为配置文件，可以相应的修改ip 端口等参数

4. 启动
   ./app_game -f default.config

5. 示例客户端，
    打开页面： http://ffown.sinaapp.com/front/html5_rpg/
   修改ip 端口，连接自己刚才的服务器




