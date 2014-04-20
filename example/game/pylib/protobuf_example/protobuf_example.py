#-*- coding:utf-8 -*-
import google.protobuf
import msg_pb2
import time

#—πÀı
test = msg_pb2.TestMsg()
test.id=1
test.time=int(time.time())
test.note="asdftest"
print test
test_str = test.SerializeToString()
print test_str

#Ω‚—π
test1 = msg_pb2.TestMsg()
test1.ParseFromString(test_str)
print test1
