
import thrift.Thrift as Thrift
import thrift.protocol.TBinaryProtocol as TBinaryProtocol
import thrift.protocol.TCompactProtocol as TCompactProtocol

import thrift.transport.TTransport as TTransport
import gen.ff.ttypes as ttypes

mb = TTransport.TMemoryBuffer()
#bp = TBinaryProtocol.TBinaryProtocol(mb)
bp = TCompactProtocol.TCompactProtocol(mb)

foo= ttypes.foo_t()
print(foo)
foo.write(bp)


ret = mb.getvalue()
print(len(ret))

mb2 = TTransport.TMemoryBuffer(ret)
#bp2 = TBinaryProtocol.TBinaryProtocol(mb2)
bp2 = TCompactProtocol.TCompactProtocol(mb2)
foo2= ttypes.foo_t()
foo2.read(bp2);

print(foo2)


