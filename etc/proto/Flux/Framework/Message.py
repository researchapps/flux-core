# automatically generated by the FlatBuffers compiler, do not modify

# namespace: Framework

import flatbuffers
from flatbuffers.compat import import_numpy
np = import_numpy()

class Message(object):
    __slots__ = ['_tab']

    @classmethod
    def GetRootAs(cls, buf, offset=0):
        n = flatbuffers.encode.Get(flatbuffers.packer.uoffset, buf, offset)
        x = Message()
        x.Init(buf, n + offset)
        return x

    @classmethod
    def GetRootAsMessage(cls, buf, offset=0):
        """This method is deprecated. Please switch to GetRootAs."""
        return cls.GetRootAs(buf, offset)

    # Message
    def Init(self, buf, pos):
        self._tab = flatbuffers.table.Table(buf, pos)

    # Message
    def Topic(self):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(4))
        if o != 0:
            return self._tab.String(o + self._tab.Pos)
        return None

    # Message
    def Payload(self):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(6))
        if o != 0:
            return self._tab.String(o + self._tab.Pos)
        return None

    # Message
    def Nodeid(self):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(8))
        if o != 0:
            return self._tab.Get(flatbuffers.number_types.Int16Flags, o + self._tab.Pos)
        return 0

    # Message
    def Flags(self):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(10))
        if o != 0:
            return self._tab.Get(flatbuffers.number_types.Int16Flags, o + self._tab.Pos)
        return 0

def MessageStart(builder): builder.StartObject(4)
def Start(builder):
    return MessageStart(builder)
def MessageAddTopic(builder, topic): builder.PrependUOffsetTRelativeSlot(0, flatbuffers.number_types.UOffsetTFlags.py_type(topic), 0)
def AddTopic(builder, topic):
    return MessageAddTopic(builder, topic)
def MessageAddPayload(builder, payload): builder.PrependUOffsetTRelativeSlot(1, flatbuffers.number_types.UOffsetTFlags.py_type(payload), 0)
def AddPayload(builder, payload):
    return MessageAddPayload(builder, payload)
def MessageAddNodeid(builder, nodeid): builder.PrependInt16Slot(2, nodeid, 0)
def AddNodeid(builder, nodeid):
    return MessageAddNodeid(builder, nodeid)
def MessageAddFlags(builder, flags): builder.PrependInt16Slot(3, flags, 0)
def AddFlags(builder, flags):
    return MessageAddFlags(builder, flags)
def MessageEnd(builder): return builder.EndObject()
def End(builder):
    return MessageEnd(builder)