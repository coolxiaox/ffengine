<?php
$GLOBALS['THRIFT_ROOT'] = './';
/* Dependencies. In the proper order. */
require_once $GLOBALS['THRIFT_ROOT'].'/Thrift/Transport/TTransport.php';
require_once $GLOBALS['THRIFT_ROOT'].'/Thrift/Transport/TSocket.php';
require_once $GLOBALS['THRIFT_ROOT'].'/Thrift/Protocol/TProtocol.php';
require_once $GLOBALS['THRIFT_ROOT'].'/Thrift/Protocol/TBinaryProtocol.php';
require_once $GLOBALS['THRIFT_ROOT'].'/Thrift/Transport/TMemoryBuffer.php';
require_once $GLOBALS['THRIFT_ROOT'].'/Thrift/Type/TMessageType.php';
require_once $GLOBALS['THRIFT_ROOT'].'/Thrift/Factory/TStringFuncFactory.php';
require_once $GLOBALS['THRIFT_ROOT'].'/Thrift/StringFunc/TStringFunc.php';
require_once $GLOBALS['THRIFT_ROOT'].'/Thrift/StringFunc/Core.php';
require_once $GLOBALS['THRIFT_ROOT'].'/Thrift/Type/TType.php';
require_once $GLOBALS['THRIFT_ROOT'].'/Thrift/Exception/TException.php';
require_once $GLOBALS['THRIFT_ROOT'].'/Thrift/Exception/TTransportException.php';
require_once $GLOBALS['THRIFT_ROOT'].'/Thrift/Exception/TProtocolException.php';

require_once $GLOBALS['THRIFT_ROOT'].'/Thrift/ffrpc_msg/Types.php';

class ffrpc_t {
	public $host = '';
	public $port = 0;
	public $timeout=0;
	public $err_info = '';
	static public function encode_msg($req)
	{
		$transport = new Thrift\Transport\TMemoryBuffer();
		$protocol  = new Thrift\Protocol\TBinaryProtocol($transport);
		$protocol->writeMessageBegin($req->getName(), 0, 0);
		$req->write($protocol);
		$protocol->writeMessageEnd();
		return $transport->getBuffer();
	}
	static public function decode_msg($ret, $data)
	{
	    try{
    		$transport = new Thrift\Transport\TMemoryBuffer($data);
    		$protocol  = new Thrift\Protocol\TBinaryProtocol($transport);
    		$name = '';
    		$type = 0;
    		$flag = 0;
    		$protocol->readMessageBegin($name, $type, $flag);
    		$ret->read($protocol);
    		$protocol->readMessageEnd();
    	}
        catch(Exception $e)
        {
            return false;
        }
		return true;
	}
	public function __construct($host, $port, $timeout = 0) {
		$this->host = $host;
		$this->port = $port;
		$this->timeout = $timeout;
	}
	public function ffsocket_connect()
    {
        $fp = fsockopen("tcp://".$this->host, $this->port, $errno, $errstr);
        if (!$fp) {
            $this->err_info =  "socket_connect() <$this->host:$this->port> failed." . $errstr;
        } 
        return $fp;
    }
    public function ffsocket_close($fp)
    {
        if ($fp) {
            fclose($fp);
        }
    }
    public function ffsocket_write($fp, $data)
    {
        if ($fp) {
            fwrite($fp, $data);
            return true;
        }
        return false;
    }
    public function ffsocket_read($fp, $len)
    {
        return fread($fp, $len);
    }

	public function call($service_name, $req, $ret, $namespace_ = "") {

		$socket = $this->ffsocket_connect();//socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
		if (!$socket) {
			return false;
		}
		
		$cmd  = 5;
		$dest_msg_name = '';
		if ($namespace_ != '')
		{
		    $dest_msg_name = $namespace_."::".$req->getName();
		}
		else
		{
		    $dest_msg_name = $req->getName();
		}
		
		$dest_msg_body = ffrpc_t::encode_msg($req);
		$ffmsg = new ffrpc_msg\broker_route_msg_in_t();
		
		$ffmsg->dest_namespace    = $namespace_;
		$ffmsg->dest_service_name = $service_name;
		$ffmsg->dest_msg_name     = $dest_msg_name;
		$ffmsg->dest_node_id      = 0;
		$ffmsg->from_namespace    = '';
		$ffmsg->from_node_id      = 0;
		$ffmsg->callback_id       = 0;
		$ffmsg->body              = $dest_msg_body;
		$ffmsg->err_info          = '';
		$body = ffrpc_t::encode_msg($ffmsg);

		$head = pack("Nnn", strlen($body), $cmd, 0);
		$data = $head . $body;
		
		if (false == $this->ffsocket_write($socket, $data))
		{
			$this->ffsocket_close($socket);
			return false;
		}

		$head_recv     = '';
		$body_recv     = '';
		while (strlen($head_recv) < 8)
		{
			$tmp_data = $this->ffsocket_read($socket, 8 - strlen($head_recv));
			if (false == $tmp_data)
			{
			    $this->ffsocket_close($socket);
				return false;
			}
			$head_recv .= $tmp_data;
		}

		$head_parse = unpack('Nlen/ncmd/nres', $head_recv);
		
		$body_len   = $head_parse['len'];
		$ret_cmd    = $head_parse['cmd'];

		while (strlen($body_recv) < $body_len)
		{
			$tmp_data = $this->ffsocket_read($socket, $body_len - strlen($body_recv));
			if (false == $tmp_data)
			{
			    $this->ffsocket_close($socket);
				return false;
			}
			$body_recv .= $tmp_data;
		}

		$recv_msg = new ffrpc_msg\broker_route_msg_in_t();
		if (false == ffrpc_t::decode_msg($recv_msg, $body_recv))
		{
		    $this->err_info = "recv data can't decode to broker_route_msg msg";
		    return false;
		}
		
		if ($recv_msg->err_info && $recv_msg->err_info != "")
		{
		    $this->err_info = $recv_msg->err_info;
		    return false;
		}
		if (false == ffrpc_t::decode_msg($ret, $recv_msg->body))
		{
		    $this->err_info = "recv data can't decode to msg";
		    return false;
		}

		$this->ffsocket_close($socket);

		if ($this->err_info != "")
		    return false;
		return true;
	}
	public function error_msg()
	{
	    return $this->err_info;
	}
}


function test()
{
	include_once  "ff/Types.php";

	$req   = new ff\echo_thrift_in_t();
	$ret   = new ff\echo_thrift_out_t();
	$req->data = 'OhNice!!!!';
	$ffrpc = new ffrpc_t('127.0.0.1', 40241);
	if ($ffrpc->call('scene@0', $req, $ret))
	{
	    var_dump($ret);
	}
	else{
	    echo "error_msg:".$ffrpc->error_msg()."\n";
	}
}

//test();

?>
