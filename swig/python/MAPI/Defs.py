import struct
import Struct	# MAPI.Struct

def bin2hex(x):
    return ''.join('%02X' % ord(c) for c in x)

def PROP_TAG(type, id):
    return id << 16 | type
    
def PROP_TYPE(x):
    return x & 0xffff

def PROP_ID(x):
    return x >> 16

def CHANGE_PROP_TYPE(tag, type):
    return PROP_TAG(type, PROP_ID(tag))

def DEFINE_GUID(l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8):
    return struct.pack("I2H8B", l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8)
    
def DEFINE_OLEGUID(l,w1,w2):
    return DEFINE_GUID(l, w1, w2, 0xC0, 0, 0, 0, 0, 0, 0, 0x46)

def PpropFindProp(props, proptag):
    for prop in props:
        if prop.ulPropTag == proptag:
            return prop
    return None

def HrGetOneProp(pmp, proptag):
    props = pmp.GetProps([proptag], 0)
    if props[0].ulPropTag == proptag:
        return props[0]
    raise Struct.MAPIError(props[0].Value)
