
from socket import *
import datetime
HOST = 'localhost'
PORT = 21564
BUFSIZ = 1024
ADDR = (HOST, PORT)
serversock = socket(AF_INET, SOCK_STREAM)
serversock.bind(ADDR)
serversock.listen(2)

sDomian = '<?xml version="1.0"?>  <cross-domain-policy>  <allow-access-from domain="*" />  </cross-domain-policy>'

while 1:
    print 'waiting for connection'
    clientsock, addr = serversock.accept()
    print 'connected from:', addr

    data = ''
    while 1:
        try:
            data = clientsock.recv(BUFSIZ)
        except:
            print('error')
            break
        if not data: break
        print('*'*20, datetime.datetime.now())
        print('recv len:', len(data))
        clientsock.send(data)

    clientsock.close()

serversock.close()
