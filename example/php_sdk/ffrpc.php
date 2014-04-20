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


class ffrpc_t {
	public $host = '';
	public $port = 0;
	public $timeout=0;
	public $err_info = '';
	static public function encode_msg($req)
	{
		$transport = new Thrift\Transport\TMemoryBuffer();
		$protocol  = new Thrift\Protocol\TBinaryProtocol($transport);
		$req->write($protocol);
		return $transport->getBuffer();
	}
	static public function decode_msg($ret, $data)
	{
	    try{
    		$transport = new Thrift\Transport\TMemoryBuffer($data);
    		$protocol  = new Thrift\Protocol\TBinaryProtocol($transport);
    		$ret->read($protocol);
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
		//error_reporting(E_ALL);
		//echo "tcp/ip connection \n";

		$socket = $this->ffsocket_connect();//socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
		if (!$socket) {
			//echo "socket_create() failed: reason: " . socket_strerror(socket_last_error()) . "\n";
			//$this->err_info =  "socket_create() failed.";
			//socket_close($socket);
			return false;
		} else {
			//echo "OK. \n";
		}

		//echo "Attempting to connect to '$this->host' on port '$this->port'...\n";
		/*
		$result = socket_connect($socket, $this->host, $this->port);
		if($result === false) {
			$this->err_info =  "socket_connect() <$this->host:$this->port> failed." . socket_strerror(socket_last_error($socket));
			socket_close($socket);
			return false;
		}*/
		
		$cmd  = 5;
		$dest_msg_body = ffrpc_t::encode_msg($req);
		$body  = '';
		//string      dest_namespace;
		$body .= pack("N", 0);
		//string      dest_service_name;
		$body .= pack("N", strlen($service_name)) . $service_name;
		//string      dest_msg_name;
		if ($namespace_ != '')
		{
		    $dest_msg_name = $namespace_."::".$req->getName();
		}
		else
		{
		    $dest_msg_name = $req->getName();
		}
		
		echo "$dest_msg_name $namespace_  \n";
		//debub print('dest_msg_name', dest_msg_name)
		$body .= pack("N", strlen($dest_msg_name)) . $dest_msg_name;
		//uint64_t    dest_node_id;
		$body .= pack("N", 0) . pack("N", 0);
		//string      from_namespace;
		$body .= pack("N", 0);
		//uint64_t    from_node_id;
		$body .= pack("N", 0) . pack("N", 0);
		//int64_t     callback_id;
		$body .= pack("N", 0) . pack("N", 0);
		//string      body;
		$body .= pack("N", strlen($dest_msg_body)) . $dest_msg_body;
		//string      err_info;
		$body .= pack("N", 0);

		$head = pack("Nnn", strlen($body), $cmd, 0);
		$data = $head . $body;
		
		$out = "";
		//echo "sending http head request ...body_len=".strlen($body)."\n";
		if (false == /*socket_write*/$this->ffsocket_write($socket, $data))
		{
			//$this->err_info =  "socket_write() failed." . socket_strerror(socket_last_error($socket));
			//socket_close($socket);
			$this->ffsocket_close($socket);
			return false;
		}

		//echo "Reading response:\n";
		//先读取包头，在读取body
		$head_recv     = '';
		$body_recv     = '';
		while (strlen($head_recv) < 8)
		{
			$tmp_data = /*socket_read*/$this->ffsocket_read($socket, 8 - strlen($head_recv));
			if (false == $tmp_data)
			{
				//$this->err_info =  "socket_read() head failed." . socket_strerror(socket_last_error($socket));
				//socket_close($socket);
			    $this->ffsocket_close($socket);
				return false;
			}
			$head_recv .= $tmp_data;
		}

		$head_parse = unpack('Nlen/ncmd/nres', $head_recv);
		
		$body_len   = $head_parse['len'];
		$ret_cmd    = $head_parse['cmd'];
		//echo "Reading response head.len='$body_len', head.cmd='$ret_cmd'\n";
		//开始读取body
		while (strlen($body_recv) < $body_len)
		{
			$tmp_data = /*socket_read*/$this->ffsocket_read($socket, $body_len - strlen($body_recv));
			if (false == $tmp_data)
			{
				//$this->err_info =  "socket_read() head failed." . socket_strerror(socket_last_error($socket));
				//socket_close($socket);
			    $this->ffsocket_close($socket);
				return false;
			}
			$body_recv .= $tmp_data;
		}
		//echo "Reading response body_len=".strlen($body_recv)."\n";
		
		//解析body
		$dest_service_name_len_data = substr($body_recv, 4, 8);
		$dest_service_name_len      = unpack("Nlen", $dest_service_name_len_data);
		$dest_service_name_len		= $dest_service_name_len["len"];
		$dest_service_name = substr($body_recv, 8, $dest_service_name_len); //从dest_msg_name开始
		//echo "service_len='$dest_service_name_len', service_naem='$dest_service_name'\n";
		
		$dest_msg_name_field = substr($body_recv, 8 + $dest_service_name_len, 4); //从dest_msg_name开始
		$dest_msg_name_len   = unpack("Nlen", $dest_msg_name_field);
		$dest_msg_name_len   = $dest_msg_name_len["len"];
		
		$dest_msg_name_str   = '';
		$body_field_len_data = '';
		
		if ($dest_msg_name_len > 0)
		{
			$dest_msg_name_str   = substr($body_recv, $dest_service_name_len + 12, $dest_msg_name_len);
			$body_field_len_data = substr($body_recv, $dest_service_name_len + 12 + $dest_msg_name_len+28, 4);
		}
		else
		{
			$body_field_len_data = substr($body_recv, $dest_service_name_len + 12+28, 4);
		}
		//echo "dest_msg_name_len='$dest_msg_name_len', dest_msg_name_str='$dest_msg_name_str'\n";

		$body_field_len = unpack("Nlen", $body_field_len_data);
		$body_field_len = $body_field_len["len"];
		
		$body_field_data = '';
		//debub print('11111111111111111111 222222222')
		if ($body_field_len > 0)
		{
			$body_field_data= substr($body_recv, $dest_service_name_len + 12 + $dest_msg_name_len+28 + 4, $body_field_len);
		}
		//echo "body_field_len='$body_field_len'\n";
		
		if (false == ffrpc_t::decode_msg($ret, $body_field_data))
		{
		    $this->err_info = "recv data can't decode to msg";
		    return false;
		}
		//debub print('$body_field_data len=%d' % len($body_field_data), ret_msg)
		$this->err_info = substr($body_recv, $dest_service_name_len + 12 + $dest_msg_name_len + 28 + 4 + $body_field_len + 4);

		//echo "closeing socket..\n";
		//socket_close($socket);
		$this->ffsocket_close($socket);
		//echo "ok .\n\n";
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
	$ffrpc = new ffrpc_t('127.0.0.1', 1281);
	if ($ffrpc->call('scene@0', $req, $ret, 'ff'))
	{
	    var_dump($ret);
	}
	else{
	    echo "error_msg:".$ffrpc->error_msg()."\n";
	}
}

test();

?>
