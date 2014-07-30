# -*- coding: utf-8 -*-
from socket import *
import struct

import thrift.Thrift as Thrift
import thrift.protocol.TBinaryProtocol as TBinaryProtocol
import thrift.protocol.TCompactProtocol as TCompactProtocol
import thrift.protocol.TJSONProtocol as TJSONProtocol
import thrift.transport.TTransport as TTransport
import thrift.msg_def.ttypes as ffrpc_msg_def

from gen_py.echo import ttypes

g_WriteTMemoryBuffer   = TTransport.TMemoryBuffer()
g_WriteTBinaryProtocol = TBinaryProtocol.TBinaryProtocol(g_WriteTMemoryBuffer)
g_ReadTMemoryBuffer   = TTransport.TMemoryBuffer()
g_ReadTBinaryProtocol = TBinaryProtocol.TBinaryProtocol(g_ReadTMemoryBuffer)

def encode_msg(msg, g_protocol = 0):
    #debub print('encode_msg begin', msg)
    if hasattr(msg, 'thrift_spec'):
        g_WriteTMemoryBuffer.cstringio_buf.truncate()
        g_WriteTMemoryBuffer.cstringio_buf.seek(0)
        if g_protocol == 1:
            proto = TJSONProtocol.TJSONProtocol(g_WriteTMemoryBuffer)
            proto.writeMessageBegin(msg.__class__.__name__, 0, 0);
            msg.write(proto)
            proto.writeMessageEnd();
        else:
            g_WriteTBinaryProtocol.writeMessageBegin(msg.__class__.__name__, 0, 0);
            msg.write(g_WriteTBinaryProtocol)
            g_WriteTBinaryProtocol.writeMessageEnd();
        return g_WriteTMemoryBuffer.getvalue()
    elif hasattr(msg, 'SerializeToString'):
        return msg.SerializeToString()
    #debub print('encode_msg end', msg)
    return msg

def decode_msg(dest_msg, body_data, g_protocol = 0):
	global g_ReadTMemoryBuffer, g_ReadTBinaryProtocol
	g_ReadTMemoryBuffer.cstringio_buf.truncate()
	g_ReadTMemoryBuffer.cstringio_buf.seek(0)
	g_ReadTMemoryBuffer.cstringio_buf.write(body_data)
	g_ReadTMemoryBuffer.cstringio_buf.seek(0)
	if hasattr(dest_msg, 'thrift_spec'):
		if g_protocol == 1:
			proto = TJSONProtocol.TJSONProtocol(g_ReadTMemoryBuffer)
			proto.readMessageBegin();
			dest_msg.read(proto)
			proto.readMessageEnd();
		else:
			g_ReadTBinaryProtocol.readMessageBegin();
			dest_msg.read(g_ReadTBinaryProtocol)
			g_ReadTBinaryProtocol.readMessageEnd();
	elif hasattr(dest_msg, 'SerializeFromString'):
		dest_msg.SerializeFromString(body_data)
	return dest_msg

class ffrpc_t:
	def __init__(self, host, port, timeout_ = 0):
		self.host = host
		self.port = port
		self.err_info = ''
		self.timeout = timeout_
	def set_timeout(self, timeout_):
		self.timeout = timeout_
	def get_timeout(self):
		return self.timeout
	def error_msg(self):
		return self.err_info
	def connect(self, host, port):
		tcpCliSock = None
		try:
			tcpCliSock = socket(AF_INET, SOCK_STREAM)
			ADDR = (host, port)
			tcpCliSock.connect(ADDR)
		except Exception as e:
			tcpCliSock.close()
			tcpCliSock = None
			self.err_info = 'can\'t connect to dest address <%s::%d>'%(host, port)+ ' error:' + str(e)
		except:
			tcpCliSock.close()
			tcpCliSock = None
			self.err_info = 'can\'t connect to dest address<%s::%d>'%(host, port)
		return tcpCliSock
	def call(self, service_name, req_msg, ret_msg, namespace_ = ''):
		if len(namespace_) != 0:
			namespace_ += '::'
		self.err_info = ''
		tcpCliSock = self.connect(self.host, self.port)
		if None == tcpCliSock:
		   return False
		if self.timeout > 0:
			tcpCliSock.settimeout(self.timeout)
		cmd  = 5
		dest_msg_body = encode_msg(req_msg)
		dest_msg_name = namespace_ + req_msg.__class__.__name__
		
		ffmsg = ffrpc_msg_def.broker_route_msg_in_t()
		ffmsg.dest_namespace    = namespace_
		ffmsg.dest_service_name = service_name
		ffmsg.dest_msg_name     = dest_msg_name
		ffmsg.dest_node_id      = 0
		ffmsg.from_namespace    = ''
		ffmsg.from_node_id      = 0
		ffmsg.callback_id       = 0
		ffmsg.body              = dest_msg_body
		#print('dest_msg_body', len(dest_msg_body))
		ffmsg.err_info          = ''
		
		body  = encode_msg(ffmsg)
		head  = struct.pack("!IHH", len(body), cmd, 0)
		data  = head + body

		#debub print('data len=%d, body_len=%d'%(len(data), len(body)))
		tcpCliSock.send(data)
		#tcpCliSock.send('TTTTTTTTTTTTTTTTTTTTTt')
		#先读取包头，在读取body
		head_recv     = ''
		body_recv     = ''
		try:
			while len(head_recv) < 8:
				#debub print('recv head_recv', len(head_recv))
				tmp_data = tcpCliSock.recv(8 - len(head_recv))
				if len(tmp_data) == 0:
					self.err_info = 'call failed: read end for head'
					tcpCliSock.close()
					return False
				head_recv += tmp_data
			head_parse = struct.unpack('!IHH', head_recv)
			
			body_len   = head_parse[0]
			#print('recv head end body_len=%d, head=%d'%(body_len, len(head_recv)))
			#开始读取body
			while len(body_recv) < body_len:
				tmp_data = tcpCliSock.recv(body_len - len(body_recv))
				if len(tmp_data) == 0:
					self.err_info = 'call failed: read end for body'
					tcpCliSock.close()
					return False
				body_recv += tmp_data
			#解析body
			recv_msg = ffrpc_msg_def.broker_route_msg_in_t()
			decode_msg(recv_msg, body_recv)
			if recv_msg.err_info != None and recv_msg.err_info != '':
			    self.err_info = recv_msg.err_info
			else:
			    #print('recv_msg.body', len(recv_msg.body))
			    decode_msg(ret_msg, recv_msg.body)
		except Exception as e:
			self.err_info = 'call failed:'+str(e)
		except:
			self.err_info = 'call failed unknown exception'

		tcpCliSock.close()
		tcpCliSock = None
		if len(self.err_info) > 0:
			return False
		return True


if __name__ == '__main__':

    HOST = '127.0.0.1'
    PORT = 40241
    ffc = ffrpc_t(HOST, PORT, 15)
    
    req = ttypes.echo_thrift_in_t('ohNice')
    ret = ttypes.echo_thrift_out_t()
    ffc.call('scene@0', req, ret)
    
    print('error_info = %s' %(ffc.error_msg()), ret)
