# -*- indent-tabs-mode: nil -*-

import MAPICore
from MAPI.Defs import *

class MAPIStruct:
    def __init__(self): pass
    def __eq__(self, other):
        if other is None:
            return False
        return self.__dict__ == other.__dict__
    def __repr__(self):
        return repr(self.__dict__)

class SPropValue(MAPIStruct):
    def __init__(self, ulPropTag, Value):
        self.ulPropTag = ulPropTag
        self.Value = Value
    def __repr__(self):
        return "SPropValue(0x%08X, %s)" % (self.ulPropTag, repr(self.Value))
    def __cmp__(self, other):
        if other is None:
            return 1
        if self.ulPropTag != other.ulPropTag:
            return cmp(self.ulPropTag,other.ulPropTag)
        return cmp(self.Value,other.Value)
    def __hash__(self):
        return hash(self.ulPropTag) + hash(self.Value)

class SSort(MAPIStruct):
    def __init__(self, ulPropTag, ulOrder):
        self.ulPropTag = ulPropTag
        self.ulOrder = ulOrder
        
    def __repr__(self):
        sorts = [ 'ASC', 'DESC' ]
        return "{ %08X %s }" % (self.ulPropTag, sorts[self.ulOrder])

class SSortOrderSet(MAPIStruct):
    def __init__(self, sorts, cCategories, cExpanded):
        self.aSort = sorts
        self.cCategories = cCategories
        self.cExpanded = cExpanded
      
    def __repr__(self):
        return "{ %s, %d, %d }" % (repr(self.aSort), self.cCategories, self.cExpanded)

class MAPINAMEID(MAPIStruct):
    def __init__(self, guid, kind, id):
        self.guid = guid
        self.kind = kind
        self.id = id
        
    def __repr__(self):
        if (self.kind == MAPICore.MNID_ID):
            return "{ %s MNID_ID %s }" % (bin2hex(self.guid), self.id)
        else:
            return "{ %s MNID_STRING %s }" % (bin2hex(self.guid), self.id)

class SPropProblem(MAPIStruct):
    def __init__(self, index, tag, scode):
        self.index = index
        self.tag = tag
        self.scode = scode

class SAndRestriction(MAPIStruct):
    def __init__(self, sub):
        self.rt = MAPICore.RES_AND
        self.lpRes = sub
    def __repr__(self):
        return 'SAndRestriction(%s)' % (str(self.lpRes))
            
class SOrRestriction(MAPIStruct):
    def __init__(self, sub):
        self.rt = MAPICore.RES_OR
        self.lpRes = sub
    def __repr__(self):
        return 'SOrRestriction(%s)' % (str(self.lpRes))
        
class SNotRestriction(MAPIStruct):
    def __init__(self, sub):
        self.rt = MAPICore.RES_NOT
        self.lpRes = sub
    def __repr__(self):
        return 'SNotRestriction(%s)' % (str(self.lpRes))
        
class SContentRestriction(MAPIStruct):
    def __init__(self, fuzzy, proptag, prop):
        self.rt = MAPICore.RES_CONTENT
        self.ulFuzzyLevel = fuzzy
        self.ulPropTag = proptag
        self.lpProp = prop
    def __repr__(self):
        return 'SContentRestriction(%d,0x%x,%s)' % (self.ulFuzzyLevel, self.ulPropTag, str(self.lpProp))
        
class SBitMaskRestriction(MAPIStruct):
    def __init__(self, relBMR, ulPropTag, ulMask):
        self.rt = MAPICore.RES_BITMASK
        self.relBMR = relBMR
        self.ulPropTag = ulPropTag
        self.ulMask = ulMask
    def __repr__(self):
        return 'SBitMaskRestriction(%d,0x%x,%d)' % (self.relBMR, self.ulPropTag, self.ulMask)

class SPropertyRestriction(MAPIStruct):
    def __init__(self, relop, ulPropTag, prop):
        self.rt = MAPICore.RES_PROPERTY
        self.ulPropTag = ulPropTag
        self.relop = relop
        self.lpProp = prop
    def __repr__(self):
        return 'SPropertyRestriction(%d,0x%x,%s)' %(self.relop, self.ulPropTag, str(self.lpProp))

class SComparePropsRestriction(MAPIStruct):
    def __init__(self, relop, ulPropTag1, ulPropTag2):
        self.rt = MAPICore.RES_COMPAREPROPS
        self.relop = relop
        self.ulPropTag1 = ulPropTag1
        self.ulPropTag2 = ulPropTag2
    def __repr__(self):
        return 'SComparePropsRestriction(%d,0x%x,0x%x)' % (self.relop, self.ulPropTag1, self.ulPropTag2)
        
class SSizeRestriction(MAPIStruct):
    def __init__(self, relop, ulPropTag, cb):
        self.rt = MAPICore.RES_SIZE
        self.relop = relop
        self.ulPropTag = ulPropTag
        self.cb = cb
    def __repr__(self):
        return 'SSizeRestriction(%d,0x%x,%d)' % (self.relop, self.ulPropTag, self.cb)
        
class SExistRestriction(MAPIStruct):
    def __init__(self, proptag):
        self.rt = MAPICore.RES_EXIST
        self.ulPropTag = proptag
    def __repr__(self):
        return 'SExistRestriction(0x%x)' % (self.ulPropTag)
        
class SSubRestriction(MAPIStruct):
    def __init__(self, ulSubObject, res):
        self.rt = MAPICore.RES_SUBRESTRICTION
        self.ulSubObject = ulSubObject
        self.lpRes = res
    def __repr__(self):
        return 'SSubRestriction(0x%x,%s)' % (self.ulSubObject, str(self.lpRes))
        
class SCommentRestriction(MAPIStruct):
    def __init__(self, res, prop):
        self.rt = MAPICore.RES_COMMENT
        self.lpRes = res
        self.lpProp = prop
    def __repr__(self):
        return 'SCommentRestriction(%s,%s)' %(str(self.lpRes), str(self.lpProp))

class actMoveCopy(MAPIStruct):
    def __init__(self, store, folder):
        self.StoreEntryId = store
        self.FldEntryId = folder
    def __repr__(self):
        return 'actMoveCopy(0x%s,0x%s)' % (self.StoreEntryId.encode('hex'), self.FldEntryId.encode('hex'))

class actReply(MAPIStruct):
    def __init__(self, entryid, guid):
        self.EntryId = entryid
        self.guidReplyTemplate = guid
    def __repr__(self):
        return 'actReply(0x%s,0x%s)' % (self.EntryId.encode('hex'), self.guidReplyTemplate.encode('hex'))

class actDeferAction(MAPIStruct):
    def __init__(self, deferMsg):
        self.data = deferMsg
    def __repr__(self):
        return 'actDeferMsg(0x%s)' % (self.data.encode('hex'))

class actBounce(MAPIStruct):
    def __init__(self, code):
        self.scBounceCode = code
    def __repr__(self):
        return 'actBounce(%d)' % (len(self.scBounceCode))

class actFwdDelegate(MAPIStruct):
    def __init__(self, adrlist):
        self.lpadrlist = adrlist
    def __repr__(self):
        return 'actFwdDelegate(%s)' % (str(self.lpadrlist))

class actTag(MAPIStruct):
    def __init__(self, tag):
        self.propTag = tag
    def __repr__(self):
        return 'actTag(%s)' % (str(self.propTag))

class ACTION(MAPIStruct):
    def __init__(self, acttype, flavor, res, proptagarray, flags, actobj):
        self.acttype = acttype
        self.ulActionFlavor = flavor
        self.lpRes = res
        self.lpPropTagArray = proptagarray
        self.ulFlags = flags
        # any of the above action
        self.actobj = actobj
    def __repr__(self):
        return 'ACTION(%d,%d,%s,%s,0x%x,%s)' % (self.acttype,self.ulActionFlavor,str(self.lpRes),str(self.lpPropTagArray),self.ulFlags,str(self.actobj))

class ACTIONS(MAPIStruct):
    def __init__(self, version, actions):
        self.ulVersion = version
        self.lpAction = actions
    def __repr__(self):
        return 'ACTIONS(%d,%s)' % (self.ulVersion, str(self.lpAction))
    
class MAPIError(Exception):
    def __init__(self, hr):
        self.hr = hr
        
    def __repr__(self):
        return "MAPI error %X" % (self.hr)

    def __str__(self):
        return self.__repr__()

class READSTATE(MAPIStruct):
    def __init__(self, SourceKey, ulFlags):
        self.SourceKey = SourceKey
        self.ulFlags = ulFlags

# @todo sUserId ECENTRYID?
# @todo propmap?
class ECUSER(MAPIStruct):
    def __init__(self, Username, Password, Email, FullName, Servername = None, Class = 0x10001, IsAdmin = False, IsHidden = False, Capacity = 0, UserID = None):
        self.Username = Username
        self.Password = Password
        self.Email = Email
        self.FullName = FullName
        self.Servername = Servername
        self.Class = Class
        self.IsAdmin = IsAdmin
        self.IsHidden = IsHidden
        self.Capacity = Capacity
        self.UserID = UserID
    def __str__(self):
        return 'ECUSER(%s,%s,%s)' % (self.Username, self.Email, self.FullName)

# @todo sGroupId ECENTRYID?
# @todo propmap?
class ECGROUP(MAPIStruct):
    def __init__(self, Groupname, Fullname, Email, IsHidden = False, GroupID = None):
        self.Groupname = Groupname
        self.Fullname = Fullname
        self.Email = Email
        self.IsHidden = IsHidden
        self.GroupID = GroupID

# @todo sCompanyId ECENTRYID?
# @todo sAdministrator ECENTRYID?
# @todo propmap?
class ECCOMPANY(MAPIStruct):
    def __init__(self, Companyname, Servername, IsHidden = False, CompanyID = None):
        self.Companyname = Companyname
        self.Servername = Servername
        self.IsHidden = IsHidden
        self.CompanyID = CompanyID

class ERROR_NOTIFICATION(MAPIStruct):
    def __init__(self, lpEntryID, scode, ulFlags, lpMAPIError):
        self.lpEntryID = lpEntryID
        self.scode = scode
        self.ulFlags = ulFlags
        self.lpMAPIError = lpMAPIError
        
class NEWMAIL_NOTIFICATION(MAPIStruct):
    def __init__(self, lpEntryID, lpParentID, ulFlags, lpszMessageClass, ulMessageFlags):
        self.lpEntryID = lpEntryID
        self.lpParentID = lpParentID
        self.ulFlags = ulFlags
        self.lpszMessageClass = lpszMessageClass
        self.ulMessageFlags = ulMessageFlags
        
class OBJECT_NOTIFICATION(MAPIStruct):
    def __init__(self, ulEventType, lpEntryID, ulObjType, lpParentID, lpOldID, lpOldParentID, lpPropTagArray):
        self.ulEventType = ulEventType
        self.lpEntryID = lpEntryID
        self.ulObjType = ulObjType
        self.lpParentID = lpParentID
        self.lpOldID = lpOldID
        self.lpOldParentID = lpOldParentID
        self.lpPropTagArray = lpPropTagArray
        
class TABLE_NOTIFICATION(MAPIStruct):
    def __init__(self, ulTableEvent, hResult, propIndex, propPrior, row):
        self.ulTableEvent = ulTableEvent
        self.hResult = hResult
        self.propIndex = propIndex
        self.propPrior = propPrior
        self.row = row
     
class ROWENTRY(MAPIStruct):
    def __init__(self, ulRowFlags, rgPropVals):
        self.ulRowFlags = ulRowFlags
        self.rgPropVals = rgPropVals

class ECQUOTA(MAPIStruct):
    def __init__(self, bUseDefaultQuota, bIsUserDefaultQuota, llWarnSize, llSoftSize, llHardSize):
        self.bUseDefaultQuota = bUseDefaultQuota
        self.bIsUserDefaultQuota = bIsUserDefaultQuota
        self.llWarnSize = llWarnSize
        self.llSoftSize = llSoftSize
        self.llHardSize = llHardSize
        