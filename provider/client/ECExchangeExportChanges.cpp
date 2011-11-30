/*
 * Copyright 2005 - 2009  Zarafa B.V.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3, 
 * as published by the Free Software Foundation with the following additional 
 * term according to sec. 7:
 *  
 * According to sec. 7 of the GNU Affero General Public License, version
 * 3, the terms of the AGPL are supplemented with the following terms:
 * 
 * "Zarafa" is a registered trademark of Zarafa B.V. The licensing of
 * the Program under the AGPL does not imply a trademark license.
 * Therefore any rights, title and interest in our trademarks remain
 * entirely with us.
 * 
 * However, if you propagate an unmodified version of the Program you are
 * allowed to use the term "Zarafa" to indicate that you distribute the
 * Program. Furthermore you may use our trademarks where it is necessary
 * to indicate the intended purpose of a product or service provided you
 * use it in accordance with honest practices in industrial or commercial
 * matters.  If you want to propagate modified versions of the Program
 * under the name "Zarafa" or "Zarafa Server", you may only do so if you
 * have a written permission by Zarafa B.V. (to acquire a permission
 * please contact Zarafa at trademark@zarafa.com).
 * 
 * The interactive user interface of the software displays an attribution
 * notice containing the term "Zarafa" and/or the logo of Zarafa.
 * Interactive user interfaces of unmodified and modified versions must
 * display Appropriate Legal Notices according to sec. 5 of the GNU
 * Affero General Public License, version 3, when you propagate
 * unmodified or modified versions of the Program. In accordance with
 * sec. 7 b) of the GNU Affero General Public License, version 3, these
 * Appropriate Legal Notices must retain the logo of Zarafa or display
 * the words "Initial Development by Zarafa" if the display of the logo
 * is not reasonably feasible for technical reasons. The use of the logo
 * of Zarafa in Legal Notices is allowed for unmodified and modified
 * versions of the software.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *  
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#include "platform.h"

#include "ECExchangeExportChanges.h"

#include <set>

#include "Util.h"
#include "ECGuid.h"
#include "edkguid.h"
#include "mapiguid.h"
#include "mapiext.h"

#include <mapiutil.h>
#include "ZarafaICS.h"
#include "ECDebug.h"

#include "Mem.h"
#include "ECMessage.h"
#include "stringutil.h"
#include "WSStreamOps.h"
#include <ECSyncLog.h>
#include <ECSyncUtil.h>
#include <ECSyncSettings.h>
#include "EntryPoint.h"

// We use ntohl/htonl for network-order conversion
#include <arpa/inet.h>

#include <charset/convert.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

ECExchangeExportChanges::ECExchangeExportChanges(ECMAPIFolder *lpFolder, unsigned int ulSyncType)
: m_iidMessage(IID_IMessage)
{
	ECSyncLog::GetLogger(&m_lpLogger);

	m_lpFolder = lpFolder;
	m_ulSyncType = ulSyncType;

	m_bConfiged = false;
	m_lpStream = NULL;
	m_lpsStreamOps = NULL;
	m_lpImportContents = NULL;
	m_lpImportStreamedContents = NULL;
	m_lpImportHierarchy = NULL;
	m_ulFlags = 0;
	m_ulSyncId = 0;
	m_ulChangeId = 0;
	m_ulStep = 0;
	m_ulStepOffset = 0;
	m_ulBatchSize = 256;
	m_ulBatchEnd = 0;
	m_ulBatchNextStart = 0;
	m_ulBufferSize = 0;
	m_ulChanges = 0;
	m_lpChanges = NULL;
	m_lpRestrict = NULL;
	m_lpChangePropTagArray = NULL;
	m_ulMaxChangeId = 0;
	
	m_clkStart = 0;
	memset(&m_tmsStart, 0, sizeof(m_tmsStart));

	m_lpFolder->AddRef();
}

ECExchangeExportChanges::~ECExchangeExportChanges(){
	if (m_lpsStreamOps)
		m_lpsStreamOps->Release();

	if(m_lpChanges)
		MAPIFreeBuffer(m_lpChanges);

	if(m_lpChangePropTagArray)
		MAPIFreeBuffer(m_lpChangePropTagArray);

	if(m_lpFolder)
		m_lpFolder->Release();

	if(m_lpStream)
		m_lpStream->Release();
	
	if(m_lpImportContents)
		m_lpImportContents->Release();

	if(m_lpImportStreamedContents)
		m_lpImportStreamedContents->Release();

	if(m_lpImportHierarchy)
		m_lpImportHierarchy->Release();
		
	if(m_lpRestrict)
		MAPIFreeBuffer(m_lpRestrict);
}

HRESULT ECExchangeExportChanges::Create(ECMAPIFolder *lpFolder, unsigned int ulSyncType, LPEXCHANGEEXPORTCHANGES* lppExchangeExportChanges){
	HRESULT hr = hrSuccess;
	ECExchangeExportChanges *lpEEC = NULL;
	
	if(!lpFolder || (ulSyncType != ICS_SYNC_CONTENTS && ulSyncType != ICS_SYNC_HIERARCHY)){
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	lpEEC = new ECExchangeExportChanges(lpFolder, ulSyncType);

	hr = lpEEC->QueryInterface(IID_IExchangeExportChanges, (void **)lppExchangeExportChanges);

exit:
	return hr;
}

HRESULT	ECExchangeExportChanges::QueryInterface(REFIID refiid, void **lppInterface)
{
	REGISTER_INTERFACE(IID_ECExchangeExportChanges, this);
	REGISTER_INTERFACE(IID_ECUnknown, this);

	REGISTER_INTERFACE(IID_IExchangeExportChanges, &this->m_xExchangeExportChanges);
	REGISTER_INTERFACE(IID_IUnknown, &this->m_xExchangeExportChanges);

	REGISTER_INTERFACE(IID_IECExportChanges, &this->m_xECExportChanges);

	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

HRESULT ECExchangeExportChanges::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError){
	HRESULT		hr = hrSuccess;
	LPMAPIERROR	lpMapiError = NULL;
	LPCTSTR		lpszErrorMsg = NULL;
	
	//FIXME: give synchronization errors messages
	lpszErrorMsg = Util::HrMAPIErrorToText((hResult == hrSuccess)?MAPI_E_NO_ACCESS : hResult);

	hr = ECAllocateBuffer(sizeof(MAPIERROR),(void **)&lpMapiError);
	if(hr != hrSuccess)
		goto exit;

	if ((ulFlags & MAPI_UNICODE) == MAPI_UNICODE) {
		std::wstring wstrErrorMsg = convert_to<std::wstring>(lpszErrorMsg);
		std::wstring wstrCompName = convert_to<std::wstring>(g_strProductName.c_str());

		MAPIAllocateMore(sizeof(std::wstring::value_type) * (wstrErrorMsg.size() + 1), lpMapiError, (void**)&lpMapiError->lpszError);
		wcscpy((wchar_t*)lpMapiError->lpszError, wstrErrorMsg.c_str());

		MAPIAllocateMore(sizeof(std::wstring::value_type) * (wstrCompName.size() + 1), lpMapiError, (void**)&lpMapiError->lpszComponent);
		wcscpy((wchar_t*)lpMapiError->lpszComponent, wstrCompName.c_str()); 

	} else {
		std::string strErrorMsg = convert_to<std::string>(lpszErrorMsg);
		std::string strCompName = convert_to<std::string>(g_strProductName.c_str());

		MAPIAllocateMore(strErrorMsg.size() + 1, lpMapiError, (void**)&lpMapiError->lpszError);
		strcpy((char*)lpMapiError->lpszError, strErrorMsg.c_str());

		MAPIAllocateMore(strCompName.size() + 1, lpMapiError, (void**)&lpMapiError->lpszComponent);
		strcpy((char*)lpMapiError->lpszComponent, strCompName.c_str());
	}

	lpMapiError->ulContext		= 0;
	lpMapiError->ulLowLevelError= 0;
	lpMapiError->ulVersion		= 0;

	*lppMAPIError = lpMapiError;

exit:
	if( hr != hrSuccess && lpMapiError)
		ECFreeBuffer(lpMapiError);

	return hr;
}

HRESULT ECExchangeExportChanges::Config(LPSTREAM lpStream, ULONG ulFlags, LPUNKNOWN lpCollector, LPSRestriction lpRestriction, LPSPropTagArray lpIncludeProps, LPSPropTagArray lpExcludeProps, ULONG ulBufferSize){
	HRESULT		hr = hrSuccess;

	ULONG		ulSyncId = 0;
	ULONG		ulChangeId = 0;
	ULONG		ulStep = 0;
	ULONG		ulCount = 0;
	//PR_PARENT_SOURCE_KEY, PR_SOURCE_KEY, PR_CHANGE_KEY, PR_PREDECESSOR_CHANGE_LIST, PR_DISPLAY_NAME, PR_LAST_MODIFICATION_TIME
	ULONG		ulDefaultContentProps[] = {PR_SOURCE_KEY, PR_LAST_MODIFICATION_TIME, PR_PARENT_SOURCE_KEY, PR_CHANGE_KEY, PR_MESSAGE_FLAGS, PR_PREDECESSOR_CHANGE_LIST, PR_CREATION_TIME};
	ULONG		ulDefaultHierarchyProps[] = {PR_SOURCE_KEY, PR_LAST_MODIFICATION_TIME, PR_PARENT_SOURCE_KEY, PR_CHANGE_KEY, PR_PREDECESSOR_CHANGE_LIST, PR_DISPLAY_NAME, PR_FOLDER_TYPE, PR_ATTR_HIDDEN};
	ULONG		ulDefaultSize = 0;
	ULONG 		* ulDefaultProps = ulDefaultContentProps;
	BOOL		bCanStream = FALSE;
	bool		bFound = false;
	LPSPropValue lpPropSourceKey = NULL;
	LPSPropValue lpPropDisplayName = NULL;

	SBinary		sbNull = {0};
	bool		bForceImplicitStateUpdate = false;
	ECSyncSettings *lpSyncSettings = ECSyncSettings::GetInstance();

	typedef std::map<SBinary, ChangeListIter, Util::SBinaryLess>	ChangeMap;
	typedef ChangeMap::iterator					ChangeMapIter;
	ChangeMap		mapChanges;
	ChangeMapIter	iterLastChange;
	ChangeList		lstChange;
	
	if(m_bConfiged){
		hr = MAPI_E_UNCONFIGURED;
		goto exit;
	}
	
	if(lpRestriction) {
		hr = Util::HrCopySRestriction(&m_lpRestrict, lpRestriction);
		if(hr != hrSuccess)
	    	goto exit;
	} else {
		m_lpRestrict = NULL;
	}
	
	m_ulFlags = ulFlags;
	
	if(! (ulFlags & SYNC_CATCHUP)) {
		if(lpCollector == NULL) {
			hr = MAPI_E_INVALID_PARAMETER;
			goto exit;
		}
		
		// We don't need the importer when doing SYNC_CATCHUP
		if(m_ulSyncType == ICS_SYNC_CONTENTS){
			ulDefaultProps = ulDefaultContentProps;
			ulDefaultSize = sizeof(ulDefaultContentProps) / sizeof(*ulDefaultContentProps);

			hr = lpCollector->QueryInterface(IID_IExchangeImportContentsChanges, (LPVOID*) &m_lpImportContents);
			if (hr == hrSuccess && lpSyncSettings->SyncStreamEnabled()) {
				m_lpFolder->GetMsgStore()->lpTransport->HrCheckCapabilityFlags(ZARAFA_CAP_ENHANCED_ICS, &bCanStream);
				if (bCanStream == TRUE) {
					LOG_DEBUG(m_lpLogger, "%s", "Exporter supports enhanced ICS, checking importer...");
					hr = lpCollector->QueryInterface(IID_IECImportContentsChanges, (LPVOID*) &m_lpImportStreamedContents);
					if (hr == MAPI_E_INTERFACE_NOT_SUPPORTED) {
						ASSERT(m_lpImportStreamedContents == NULL);
						hr = hrSuccess;
						LOG_DEBUG(m_lpLogger, "%s", "Importer doesn't support enhanced ICS");
					} else
						LOG_DEBUG(m_lpLogger, "%s", "Importer supports enhanced ICS");
				} else
					LOG_DEBUG(m_lpLogger, "%s", "Exporter doesn't support enhanced ICS");
			}
		}else if(m_ulSyncType == ICS_SYNC_HIERARCHY){
			ulDefaultProps = ulDefaultHierarchyProps;
			ulDefaultSize = sizeof(ulDefaultHierarchyProps) / sizeof(*ulDefaultHierarchyProps);
			hr = lpCollector->QueryInterface(IID_IExchangeImportHierarchyChanges, (LPVOID*) &m_lpImportHierarchy);
		}else{
			hr = MAPI_E_INVALID_PARAMETER;
		}
		if(hr != hrSuccess)
			goto exit;
	}
	
	if (lpStream == NULL){
		LARGE_INTEGER lint = {{ 0, 0 }};
		ULONG tmp[2] = { 0, 0 };
		ULONG ulSize = 0;

		LOG_DEBUG(m_lpLogger, "%s", "Creating new exporter stream");
		hr = CreateStreamOnHGlobal(GlobalAlloc(GPTR, sizeof(tmp)), true, &m_lpStream);
		if (hr != hrSuccess)
			goto exit;

		m_lpStream->Seek(lint, STREAM_SEEK_SET, NULL);
		m_lpStream->Write(tmp, sizeof(tmp), &ulSize);
	} else {
		hr = lpStream->QueryInterface(IID_IStream, (LPVOID*)&m_lpStream);
		if (hr != hrSuccess)
			goto exit;
	}


	//include PR_MESSAGE_SIZE or MAPI PR_ENTRYID
	if(lpIncludeProps){
		hr = MAPIAllocateBuffer(CbNewSPropTagArray(lpIncludeProps->cValues + ulDefaultSize), (LPVOID *) &m_lpChangePropTagArray);
		if(hr != hrSuccess)
			goto exit;
		m_lpChangePropTagArray->cValues = 0;
		for(ulStep = 0; ulStep < lpIncludeProps->cValues; ulStep++){
			m_lpChangePropTagArray->aulPropTag[m_lpChangePropTagArray->cValues++] = lpIncludeProps->aulPropTag[ulStep];
		}
	}else if(!lpExcludeProps && m_ulSyncType == ICS_SYNC_HIERARCHY){
		m_lpChangePropTagArray = NULL;
	}else{
		hr = MAPIAllocateBuffer(CbNewSPropTagArray(ulDefaultSize + 1), (LPVOID *) &m_lpChangePropTagArray);
		if(hr != hrSuccess)
			goto exit;
		m_lpChangePropTagArray->cValues = 0;
	}

	if(m_lpChangePropTagArray){
		for(ulCount = 0; ulCount < ulDefaultSize; ulCount++){
			bFound = false;
			if(lpExcludeProps){
				for(ulStep = 0; !bFound && ulStep < lpExcludeProps->cValues; ulStep++){
					if(lpExcludeProps->aulPropTag[ulStep] == ulDefaultProps[ulCount]){
						bFound = true;
					}
				}
			}
			if(!bFound){
				m_lpChangePropTagArray->aulPropTag[m_lpChangePropTagArray->cValues++] = ulDefaultProps[ulCount];
			}
		}
	}


	hr = HrDecodeSyncStateStream(m_lpStream, &ulSyncId, &ulChangeId, &m_setProcessedChanges);
	if(hr != hrSuccess)
		goto exit;

	LOG_DEBUG(m_lpLogger, "Decoded state stream: syncid=%u, changeid=%u, processed changes=%lu", ulSyncId, ulChangeId, m_setProcessedChanges.size());

	if (m_lpLogger->Log(EC_LOGLEVEL_INFO))
		HrGetOneProp(&m_lpFolder->m_xMAPIFolder, PR_DISPLAY_NAME_W, &lpPropDisplayName);// ignore error, used for logging

	if(ulSyncId == 0) {
		LOG_DEBUG(m_lpLogger, "Getting new sync id for %s folder '%ls'...", (m_lpFolder->GetMsgStore()->IsOfflineStore() ? "offline" : "online"), (lpPropDisplayName)?lpPropDisplayName->Value.lpszW:L"<Unknown>");

		// Ignore any trailing garbage in the stream
		hr = HrGetOneProp(&m_lpFolder->m_xMAPIFolder, PR_SOURCE_KEY, &lpPropSourceKey);
		if(hr != hrSuccess)
			goto exit;

		if((lpPropSourceKey->Value.bin.cb < 24) && (lpPropSourceKey->Value.bin.lpb[lpPropSourceKey->Value.bin.cb-1] & 0x80)) {
			// If we're getting a truncated sourcekey, untruncate it now so we can pass it to GetChanges()

			LPSPropValue lpPropUntruncated;
			MAPIAllocateBuffer(sizeof(SPropValue), (void **)&lpPropUntruncated);
			lpPropUntruncated->ulPropTag = PR_SOURCE_KEY;
			MAPIAllocateMore(24, lpPropUntruncated, (void **)&lpPropUntruncated->Value.bin.lpb);
			memset(lpPropUntruncated->Value.bin.lpb, 0, 24);
			lpPropUntruncated->Value.bin.cb = 24;
			memcpy(lpPropUntruncated->Value.bin.lpb, lpPropSourceKey->Value.bin.lpb, lpPropSourceKey->Value.bin.cb);
			lpPropUntruncated->Value.bin.lpb[lpPropSourceKey->Value.bin.cb-1] &= ~0x80; // Clear truncation bit

			MAPIFreeBuffer(lpPropSourceKey);
			lpPropSourceKey = lpPropUntruncated;
		}

		// Register our sync with the server, get a sync ID
		hr = m_lpFolder->GetMsgStore()->lpTransport->HrSetSyncStatus(lpPropSourceKey->Value.bin , 0, 0, m_ulSyncType, 0, &ulSyncId);
		if(hr != hrSuccess)
			goto exit;

		LOG_DEBUG(m_lpLogger, "New sync id for %s folder '%ls': %u", (m_lpFolder->GetMsgStore()->IsOfflineStore() ? "offline" : "online"), (lpPropDisplayName)?lpPropDisplayName->Value.lpszW:L"<Unknown>", ulSyncId);

		bForceImplicitStateUpdate = true;
	}

	if(m_lpChanges) {
		MAPIFreeBuffer(m_lpChanges);
		m_lpChanges = NULL;
	}

	hr = m_lpFolder->GetMsgStore()->lpTransport->HrGetChanges(lpPropSourceKey ? lpPropSourceKey->Value.bin : sbNull, ulSyncId, ulChangeId, m_ulSyncType, ulFlags, m_lpRestrict, &m_ulMaxChangeId, &m_ulChanges, &m_lpChanges);
	if(hr != hrSuccess)
		goto exit;

	m_ulSyncId = ulSyncId;
	m_ulChangeId = ulChangeId;

	m_lpLogger->Log(EC_LOGLEVEL_INFO, "%s folder: %ls changes: %u syncid: %u changeid: %u", (m_lpFolder->GetMsgStore()->IsOfflineStore() ? "offline" : "online"), (lpPropDisplayName)?lpPropDisplayName->Value.lpszW:L"<Unknown>", m_ulChanges, m_ulSyncId, m_ulChangeId);

	/**
	 * Filter the changes.
	 * How this works:
	 * If no previous change has been seen for a particular sourcekey, the change will just be added to
	 * the correct list. The iterator into that list is stored in a map, keyed by the sourcekey.
	 * If a previous change is found, action is taken based on the type of that change. The action can
	 * involve removing the change from a change list and/or adding it to another list.
	 *
	 * Note that lstChange is a local list, which is copied into m_lstChange after filtering. This is
	 * done because m_lstChange is not a list but a vector, which is not well suited for removing
	 * items in the middle of the data.
	 */
	for (ulStep = 0; ulStep < m_ulChanges; ++ulStep) {
		// First check if this change hasn't been processed yet
		if (m_setProcessedChanges.find(std::pair<unsigned int, std::string>(m_lpChanges[ulStep].ulChangeId, std::string((char *)m_lpChanges[ulStep].sSourceKey.lpb, m_lpChanges[ulStep].sSourceKey.cb))) != m_setProcessedChanges.end())
			continue;

		iterLastChange = mapChanges.find(m_lpChanges[ulStep].sSourceKey);
		if (iterLastChange == mapChanges.end()) {
			switch (ICS_ACTION(m_lpChanges[ulStep].ulChangeType)) {
				case ICS_NEW:
				case ICS_CHANGE:
					mapChanges.insert(ChangeMap::value_type(m_lpChanges[ulStep].sSourceKey, lstChange.insert(lstChange.end(), m_lpChanges[ulStep])));
					break;

				case ICS_FLAG:
					mapChanges.insert(ChangeMap::value_type(m_lpChanges[ulStep].sSourceKey, m_lstFlag.insert(m_lstFlag.end(), m_lpChanges[ulStep])));
					break;

				case ICS_SOFT_DELETE:
					mapChanges.insert(ChangeMap::value_type(m_lpChanges[ulStep].sSourceKey, m_lstSoftDelete.insert(m_lstSoftDelete.end(), m_lpChanges[ulStep])));
					break;

				case ICS_HARD_DELETE:
					mapChanges.insert(ChangeMap::value_type(m_lpChanges[ulStep].sSourceKey, m_lstHardDelete.insert(m_lstHardDelete.end(), m_lpChanges[ulStep])));
					break;

				default:
					break;
			}
		} else {
			switch (ICS_ACTION(m_lpChanges[ulStep].ulChangeType)) {
				case ICS_NEW:
					// This shouldn't happen since apparently we have another change for the same object.
					// However, if an object gets moved to another folder and back, we'll get a delete followed by an add. If that happens
					// we skip the delete and morph the current add to a change.
					if (ICS_ACTION(iterLastChange->second->ulChangeType) == ICS_SOFT_DELETE || ICS_ACTION(iterLastChange->second->ulChangeType) == ICS_HARD_DELETE) {
						ChangeListIter iterNewChange = lstChange.insert(lstChange.end(), *iterLastChange->second);
						iterNewChange->ulChangeType = (iterNewChange->ulChangeType & ~ICS_ACTION_MASK) | ICS_CHANGE;
						if (ICS_ACTION(iterLastChange->second->ulChangeType) == ICS_SOFT_DELETE)
							m_lstSoftDelete.erase(iterLastChange->second);
						else
							m_lstHardDelete.erase(iterLastChange->second);
						iterLastChange->second = iterNewChange;

						LOG_DEBUG(m_lpLogger, "Got an ICS_NEW change for a previously deleted object. I converted it to a change. sourcekey=%s", bin2hex(m_lpChanges[ulStep].sSourceKey.cb, m_lpChanges[ulStep].sSourceKey.lpb).c_str());
					} else
						LOG_DEBUG(m_lpLogger, "Got an ICS_NEW change for an object we've seen before. prev_change=%04x, sourcekey=%s", iterLastChange->second->ulChangeType, bin2hex(m_lpChanges[ulStep].sSourceKey.cb, m_lpChanges[ulStep].sSourceKey.lpb).c_str());
					break;

				case ICS_CHANGE:
					// Any change is allowed as the previous change except a change.
					if (ICS_ACTION(iterLastChange->second->ulChangeType) == ICS_CHANGE)
						LOG_DEBUG(m_lpLogger, "Got an ICS_CHANGE on an object for which we just saw an ICS_CHANGE. sourcekey=%s", bin2hex(m_lpChanges[ulStep].sSourceKey.cb, m_lpChanges[ulStep].sSourceKey.lpb).c_str());
					// A previous delete is allowed for the same reason as in ICS_NEW.
					if (ICS_ACTION(iterLastChange->second->ulChangeType) == ICS_SOFT_DELETE || ICS_ACTION(iterLastChange->second->ulChangeType) == ICS_HARD_DELETE) {
						if (ICS_ACTION(iterLastChange->second->ulChangeType) == ICS_SOFT_DELETE)
							m_lstSoftDelete.erase(iterLastChange->second);
						else
							m_lstHardDelete.erase(iterLastChange->second);
						iterLastChange->second = lstChange.insert(lstChange.end(), m_lpChanges[ulStep]);
						LOG_DEBUG(m_lpLogger, "Got an ICS_CHANGE change for a previously deleted object. sourcekey=%s", bin2hex(m_lpChanges[ulStep].sSourceKey.cb, m_lpChanges[ulStep].sSourceKey.lpb).c_str());
					} else if (ICS_ACTION(iterLastChange->second->ulChangeType) == ICS_FLAG) {
						m_lstFlag.erase(iterLastChange->second);
						iterLastChange->second = lstChange.insert(lstChange.end(), m_lpChanges[ulStep]);
						LOG_DEBUG(m_lpLogger, "Upgraded a previous ICS_FLAG to ICS_CHANGED. sourcekey=%s", bin2hex(m_lpChanges[ulStep].sSourceKey.cb, m_lpChanges[ulStep].sSourceKey.lpb).c_str());
					} else 
						LOG_DEBUG(m_lpLogger, "Ignoring ICS_CHANGE due to a previous change. prev_change=%04x, sourcekey=%s", iterLastChange->second->ulChangeType, bin2hex(m_lpChanges[ulStep].sSourceKey.cb, m_lpChanges[ulStep].sSourceKey.lpb).c_str());
					break;

				case ICS_FLAG:
					// This is only allowed after an ICS_NEW and ICS_CHANGE. It will be ignored in any case.
					if (ICS_ACTION(iterLastChange->second->ulChangeType) != ICS_NEW && ICS_ACTION(iterLastChange->second->ulChangeType) != ICS_CHANGE)
						LOG_DEBUG(m_lpLogger, "Got an ICS_FLAG with something else than a ICS_NEW or ICS_CHANGE as the previous changes. prev_change=%04x, sourcekey=%s", iterLastChange->second->ulChangeType, bin2hex(m_lpChanges[ulStep].sSourceKey.cb, m_lpChanges[ulStep].sSourceKey.lpb).c_str());
					LOG_DEBUG(m_lpLogger, "Ignoring ICS_FLAG due to previous ICS_NEW or ICS_CHANGE. prev_change=%04x, sourcekey=%s", iterLastChange->second->ulChangeType, bin2hex(m_lpChanges[ulStep].sSourceKey.cb, m_lpChanges[ulStep].sSourceKey.lpb).c_str());
					break;

				case ICS_SOFT_DELETE:
				case ICS_HARD_DELETE:
					if (ICS_ACTION(iterLastChange->second->ulChangeType) == ICS_NEW) {
						LOG_DEBUG(m_lpLogger, "Ignoring previous ICS_NEW due to current ICS_xxxx_DELETE. sourcekey=%s", bin2hex(m_lpChanges[ulStep].sSourceKey.cb, m_lpChanges[ulStep].sSourceKey.lpb).c_str());
						// An add and a delete results in nothing.
						lstChange.erase(iterLastChange->second);
						mapChanges.erase(iterLastChange);
					}

					else {
						// We'll ignore the previous change and replace it with this delete. We won't write it now as
						// we could get an add for the same object. But because of the reordering (deletes after all adds/changes) we would delete
						// the new object. Therefore we'll make a change out of a delete - add/change.
						LOG_DEBUG(m_lpLogger, "Replacing previous change with current ICS_xxxx_DELETE. prev_change=%04x, sourcekey=%s", iterLastChange->second->ulChangeType, bin2hex(m_lpChanges[ulStep].sSourceKey.cb, m_lpChanges[ulStep].sSourceKey.lpb).c_str());
						if (ICS_ACTION(iterLastChange->second->ulChangeType) == ICS_CHANGE)
							lstChange.erase(iterLastChange->second);
						else if (ICS_ACTION(iterLastChange->second->ulChangeType) == ICS_FLAG)
							m_lstFlag.erase(iterLastChange->second);
						else if (ICS_ACTION(iterLastChange->second->ulChangeType) == ICS_SOFT_DELETE)
							m_lstSoftDelete.erase(iterLastChange->second);
						else if (ICS_ACTION(iterLastChange->second->ulChangeType) == ICS_HARD_DELETE)
							m_lstHardDelete.erase(iterLastChange->second);

						if (m_lpChanges[ulStep].ulChangeType == ICS_SOFT_DELETE)
							iterLastChange->second = m_lstSoftDelete.insert(m_lstSoftDelete.end(), m_lpChanges[ulStep]);
						else
							iterLastChange->second = m_lstHardDelete.insert(m_lstHardDelete.end(), m_lpChanges[ulStep]);
					}
					break;

				default:
					LOG_DEBUG(m_lpLogger, "Got an unknown change. change=%04x, sourcekey=%s", m_lpChanges[ulStep].ulChangeType, bin2hex(m_lpChanges[ulStep].sSourceKey.cb, m_lpChanges[ulStep].sSourceKey.lpb).c_str());
					break;
			}
		}
	}

	m_lstChange.assign(lstChange.begin(), lstChange.end());

	if(ulBufferSize != 0){
		m_ulBufferSize = ulBufferSize;
	}else{
		m_ulBufferSize = 10;
	}

	m_bConfiged = true;

	if (bForceImplicitStateUpdate) {
		LOG_DEBUG(m_lpLogger, "Forcing state update for %s folder '%ls'", (m_lpFolder->GetMsgStore()->IsOfflineStore() ? "offline" : "online"), (lpPropDisplayName)?lpPropDisplayName->Value.lpszW:L"<Unknown>");
		/** 
		 * bForceImplicitStateUpdate is only set to true when the StateStream contained a ulSyncId of 0. In that
		 * case the ulChangeId can't be enything other then 0. As ulChangeId is assigned to m_ulChangeId and neither
		 * of them changes in this function, m_ulChangeId can't be anything other than 0 when we reach this point.
		 **/
		ASSERT(m_ulChangeId == 0);
		UpdateState(NULL);
	}

exit:
	if(lpPropDisplayName)
		MAPIFreeBuffer(lpPropDisplayName);

	if(lpPropSourceKey)
		ECFreeBuffer(lpPropSourceKey);
		
	return hr;
}

HRESULT ECExchangeExportChanges::Synchronize(ULONG FAR * lpulSteps, ULONG FAR * lpulProgress){
	HRESULT			hr = hrSuccess;
	PROCESSEDCHANGESSET::iterator iterProcessedChange;
	LPSPropValue lpPropSourceKey = NULL;

	if(!m_bConfiged){
		hr = MAPI_E_UNCONFIGURED;
		goto exit;
	}

	if(m_ulFlags & SYNC_CATCHUP){
		m_ulChangeId = m_ulMaxChangeId > m_ulChangeId ? m_ulMaxChangeId : m_ulChangeId;
		hr = UpdateStream(m_lpStream);
		if (hr == hrSuccess)
			*lpulProgress = *lpulSteps = 0;
		goto exit;
	}

	if (*lpulProgress == 0 && m_lpLogger->Log(EC_LOGLEVEL_DEBUG))
		m_clkStart = times(&m_tmsStart);

	if(m_ulSyncType == ICS_SYNC_CONTENTS){
		hr = ExportMessageChanges();
		if(hr == SYNC_W_PROGRESS)
			goto progress;
			
		if(hr != hrSuccess)
			goto exit;

		hr = ExportMessageDeletes();
		if(hr != hrSuccess)
			goto exit;

		hr = ExportMessageFlags();
		if(hr != hrSuccess)
			goto exit;

	}else if(m_ulSyncType == ICS_SYNC_HIERARCHY){
		hr = ExportFolderChanges();
		if(hr == SYNC_W_PROGRESS)
			goto progress;
		if(hr != hrSuccess)
			goto exit;

		hr = ExportFolderDeletes();
		if(hr != hrSuccess)
			goto exit;

	}else{
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = UpdateStream(m_lpStream);
	if(hr != hrSuccess)
		goto exit;
		
	if(! (m_ulFlags & SYNC_CATCHUP)) {
		if(m_ulSyncType == ICS_SYNC_CONTENTS){
			hr = m_lpImportContents->UpdateState(NULL);
		}else{
			hr = m_lpImportHierarchy->UpdateState(NULL);
		}
		if(hr != hrSuccess)
			goto exit;
	}

progress:
	if(hr == hrSuccess) {
		// The sync has completed. This means we can move to the next change ID on the server, and clear our
		// processed change list. If we can't save the new state the the server, we just keep the previous
		// change ID and the (large) change list. This allows us always to have a state, even when we can't
		// communicate with the server.
		hr = HrGetOneProp(&m_lpFolder->m_xMAPIFolder, PR_SOURCE_KEY, &lpPropSourceKey);
		if(hr != hrSuccess)
			goto exit;

		if(m_lpFolder->GetMsgStore()->lpTransport->HrSetSyncStatus(lpPropSourceKey->Value.bin , m_ulSyncId, m_ulMaxChangeId, m_ulSyncType, 0, &m_ulSyncId) == hrSuccess) {
			LOG_DEBUG(m_lpLogger, "Done: syncid=%u, changeid=%u/%u", m_ulSyncId, m_ulChangeId, m_ulMaxChangeId);

			m_ulChangeId = m_ulMaxChangeId;
			m_setProcessedChanges.clear();

			if(m_ulChanges) {
				if (m_lpLogger->Log(EC_LOGLEVEL_DEBUG)) {
					struct tms	tmsEnd = {0};
					clock_t		clkEnd = times(&tmsEnd);
					double		dblDuration = 0;
					char		szDuration[64] = {0};

					// Calculate diff
					dblDuration = (double)(clkEnd - m_clkStart) / TICKS_PER_SEC;
					if (dblDuration >= 60)
						_snprintf(szDuration, sizeof(szDuration), "%u:%02u.%03u min.", (unsigned)(dblDuration / 60), (unsigned)dblDuration % 60, (unsigned)(dblDuration * 1000 + .5) % 1000);
					else
						_snprintf(szDuration, sizeof(szDuration), "%u.%03u s.", (unsigned)dblDuration % 60, (unsigned)(dblDuration * 1000 + .5) % 1000);

					m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "folder changes synchronized in %s", szDuration);
				} else 
					m_lpLogger->Log(EC_LOGLEVEL_INFO, "folder changes synchronized");
			}
		}
	}

	*lpulSteps = m_lstChange.size();
	*lpulProgress = m_ulStep;

exit:
	if(lpPropSourceKey)
		MAPIFreeBuffer(lpPropSourceKey);
		
	return hr;
}

HRESULT ECExchangeExportChanges::UpdateState(LPSTREAM lpStream){
	HRESULT hr = hrSuccess;
	
	if(!m_bConfiged){
		hr = MAPI_E_UNCONFIGURED;
		goto exit;
	}
	
	if(lpStream == NULL) {
		lpStream = m_lpStream;
	}

	hr = UpdateStream(lpStream);
	if(hr != hrSuccess)
		goto exit;


exit:
	return hr;
}

HRESULT ECExchangeExportChanges::GetChangeCount(ULONG *lpcChanges) {
	HRESULT hr = hrSuccess;
	ULONG cChanges = 0;
	
	if(!m_bConfiged){
		hr = MAPI_E_UNCONFIGURED;
		goto exit;
	}

	// Changes in flags or deletions are only one step all together
	if(!m_lstHardDelete.empty() || !m_lstSoftDelete.empty() || !m_lstFlag.empty())
	    cChanges++;
	cChanges += m_lstChange.size();
	
	*lpcChanges = cChanges;
exit:
	return hr;
}

HRESULT ECExchangeExportChanges::SetMessageInterface(REFIID refiid) {
	m_iidMessage = refiid;
	return hrSuccess;
}

ULONG ECExchangeExportChanges::xExchangeExportChanges::AddRef(){
	TRACE_MAPI(TRACE_ENTRY, "IExchangeExportChanges::AddRef", "");
	METHOD_PROLOGUE_(ECExchangeExportChanges, ExchangeExportChanges);
	return pThis->AddRef();
}

ULONG ECExchangeExportChanges::xExchangeExportChanges::Release()
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeExportChanges::Release", "");
	METHOD_PROLOGUE_(ECExchangeExportChanges, ExchangeExportChanges);
	return pThis->Release();
}

HRESULT ECExchangeExportChanges::xExchangeExportChanges::QueryInterface(REFIID refiid, void **lppInterface)
{
	HRESULT hr;
	TRACE_MAPI(TRACE_ENTRY, "IExchangeExportChanges::QueryInterface", "%s", DBGGUIDToString(refiid).c_str());
	METHOD_PROLOGUE_(ECExchangeExportChanges, ExchangeExportChanges);
	hr = pThis->QueryInterface(refiid, lppInterface);
	TRACE_MAPI(TRACE_RETURN, "IExchangeExportChanges::QueryInterface", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECExchangeExportChanges::xExchangeExportChanges::GetLastError(HRESULT hError, ULONG ulFlags, LPMAPIERROR * lppMapiError)
{
	HRESULT hr;
	TRACE_MAPI(TRACE_ENTRY, "IExchangeExportChanges::GetLastError", "");
	METHOD_PROLOGUE_(ECExchangeExportChanges, ExchangeExportChanges);
	hr = pThis->GetLastError(hError, ulFlags, lppMapiError);
	TRACE_MAPI(TRACE_RETURN, "IExchangeExportChanges::GetLastError", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECExchangeExportChanges::xExchangeExportChanges::Config(LPSTREAM lpStream, ULONG ulFlags, LPUNKNOWN lpCollector, LPSRestriction lpRestriction, LPSPropTagArray lpIncludeProps, LPSPropTagArray lpExcludeProps, ULONG ulBufferSize){
	HRESULT hr;
	TRACE_MAPI(TRACE_ENTRY, "IExchangeExportChanges::Config", "lpStream=%08x, ulFlags=%d, lpCollector=0x%08x, lpRestriction = %08x, lpIncludeProps = %08x, lpExcludeProps = %08x, ulBufferSize = %d", lpStream, ulFlags, lpCollector, lpRestriction, lpIncludeProps, lpExcludeProps, ulBufferSize);
	METHOD_PROLOGUE_(ECExchangeExportChanges, ExchangeExportChanges);
	hr =  pThis->Config(lpStream, ulFlags, lpCollector, lpRestriction, lpIncludeProps, lpExcludeProps, ulBufferSize);
	TRACE_MAPI(TRACE_RETURN, "IExchangeExportChanges::Config", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECExchangeExportChanges::xExchangeExportChanges::Synchronize(ULONG FAR * pulSteps, ULONG FAR * pulProgress){
	HRESULT hr;
	TRACE_MAPI(TRACE_ENTRY, "IExchangeExportChanges::Synchronize", "");
	METHOD_PROLOGUE_(ECExchangeExportChanges, ExchangeExportChanges);
	hr = pThis->Synchronize(pulSteps, pulProgress);
	TRACE_MAPI(TRACE_RETURN, "IExchangeExportChanges::Synchronize", "%s", (hr != SYNC_W_PROGRESS) ? GetMAPIErrorDescription(hr).c_str() : "SYNC_W_PROGRESS");
	return hr;
}

HRESULT ECExchangeExportChanges::xExchangeExportChanges::UpdateState(LPSTREAM lpStream){
	HRESULT hr;
	TRACE_MAPI(TRACE_ENTRY, "IExchangeExportChanges::UpdateState", "");
	METHOD_PROLOGUE_(ECExchangeExportChanges, ExchangeExportChanges);
	hr = pThis->UpdateState(lpStream);
	TRACE_MAPI(TRACE_RETURN, "IExchangeExportChanges::UpdateState", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

ULONG ECExchangeExportChanges::xECExportChanges::AddRef() {
	TRACE_MAPI(TRACE_ENTRY, "IECExportChanges::AddRef", "");
	METHOD_PROLOGUE_(ECExchangeExportChanges, ECExportChanges);
	return pThis->AddRef();
}

ULONG ECExchangeExportChanges::xECExportChanges::Release() {
	TRACE_MAPI(TRACE_ENTRY, "IECExportChanges::Release", "");
	METHOD_PROLOGUE_(ECExchangeExportChanges, ECExportChanges);
	return pThis->Release();
}

HRESULT ECExchangeExportChanges::xECExportChanges::QueryInterface(REFIID refiid, void **lppInterface) {
	TRACE_MAPI(TRACE_ENTRY, "IECExportChanges::QueryInterface", "");
	METHOD_PROLOGUE_(ECExchangeExportChanges, ECExportChanges);
	return pThis->QueryInterface(refiid, lppInterface);
}

HRESULT ECExchangeExportChanges::xECExportChanges::GetChangeCount(ULONG *lpcChanges) {
	TRACE_MAPI(TRACE_ENTRY, "IECExportChanges::GetChangeCount", "");
	METHOD_PROLOGUE_(ECExchangeExportChanges, ECExportChanges);
	return pThis->GetChangeCount(lpcChanges);
}

HRESULT ECExchangeExportChanges::xECExportChanges::SetMessageInterface(REFIID refiid)
{
	HRESULT hr;
	TRACE_MAPI(TRACE_ENTRY, "IExchangeExportChanges::SetMessageInterface", "%s", DBGGUIDToString(refiid).c_str());
	METHOD_PROLOGUE_(ECExchangeExportChanges, ECExportChanges);
	hr = pThis->SetMessageInterface(refiid);
	return hr;
}


HRESULT ECExchangeExportChanges::ExportMessageChanges() {
	ASSERT(m_lpImportContents != NULL);
	if (m_lpImportStreamedContents != NULL)
		return ExportMessageChangesFast();
	else
		return ExportMessageChangesSlow();
}

HRESULT ECExchangeExportChanges::ExportMessageChangesSlow() {
	HRESULT			hr = hrSuccess;
	
	LPMESSAGE		lpSourceMessage = NULL;
	LPATTACH		lpSourceAttach = NULL;

	LPMESSAGE		lpDestMessage = NULL;
	LPATTACH		lpDestAttach = NULL;

	LPMAPITABLE		lpTable = NULL;
	LPSRowSet		lpRows = NULL;

	LPSPropValue	lpPropArray = NULL;
	LPSPropValue	lpPropVal = NULL;

	LPSPropTagArray lpPropTagArray = NULL;

	ULONG			ulObjType;
	ULONG			ulCount;
	ULONG			ulFlags;
	ULONG			ulSteps = 0;
	
	ULONG			cbEntryID = 0;
	LPENTRYID		lpEntryID = NULL;

	SizedSPropTagArray(5, sptMessageExcludes) = { 5, {
		PR_MESSAGE_SIZE, 
		PR_MESSAGE_RECIPIENTS, 
		PR_MESSAGE_ATTACHMENTS, 
		PR_ATTACH_SIZE, 
		PR_PARENT_SOURCE_KEY
	} };
	SizedSPropTagArray(1, sptAttach) = { 1, {PR_ATTACH_NUM} };

	while(m_ulStep < m_lstChange.size() && (m_ulBufferSize == 0 || ulSteps < m_ulBufferSize)){
		hr = m_lpFolder->GetMsgStore()->EntryIDFromSourceKey(
			m_lstChange.at(m_ulStep).sParentSourceKey.cb, 
			m_lstChange.at(m_ulStep).sParentSourceKey.lpb,
			m_lstChange.at(m_ulStep).sSourceKey.cb,
			m_lstChange.at(m_ulStep).sSourceKey.lpb,
			&cbEntryID, &lpEntryID);
		if(hr == MAPI_E_NOT_FOUND){
			hr = hrSuccess;
			goto next;
		}
		m_lpLogger->Log(EC_LOGLEVEL_INFO, "change sourcekey: %s", bin2hex(m_lstChange.at(m_ulStep).sSourceKey.cb, m_lstChange.at(m_ulStep).sSourceKey.lpb).c_str());
		
		if(hr != hrSuccess)
			goto exit;
		
		hr = m_lpFolder->OpenEntry(cbEntryID, lpEntryID, &m_iidMessage, MAPI_MODIFY, &ulObjType, (LPUNKNOWN*) &lpSourceMessage);
		if(hr == MAPI_E_NOT_FOUND){
			hr = hrSuccess;
			goto next;
		}
		if(hr != hrSuccess)
			goto exit;

		hr = lpSourceMessage->GetProps((LPSPropTagArray) m_lpChangePropTagArray, 0, &ulCount, &lpPropArray);
		if(hr == MAPI_W_ERRORS_RETURNED){
			hr = hrSuccess;
		}else if(hr != hrSuccess)
			goto exit;

		ulFlags = 0;
		lpPropVal = PpropFindProp(lpPropArray, ulCount, PR_MESSAGE_FLAGS);
		if(lpPropVal != NULL && (lpPropVal->Value.ul & MSGFLAG_ASSOCIATED)){
			ulFlags |= SYNC_ASSOCIATED;
		}
		if((m_lstChange.at(m_ulStep).ulChangeType & ICS_ACTION_MASK) == ICS_NEW){
			ulFlags |= SYNC_NEW_MESSAGE;
		}

		hr = m_lpImportContents->ImportMessageChange(ulCount, lpPropArray, ulFlags, &lpDestMessage);
		if (hr == SYNC_E_IGNORE) {
			m_lpLogger->Log(EC_LOGLEVEL_INFO, "ignored change");
			// Mark this change as processed
			
			hr = hrSuccess;
			goto next;
		}else if(hr == SYNC_E_OBJECT_DELETED){
			m_lpLogger->Log(EC_LOGLEVEL_INFO, "ignored change for deleted item");

			// Mark this change as processed
			hr = hrSuccess;
			goto next;
		}else if(hr == SYNC_E_INVALID_PARAMETER){
			//exchange doesn't like our input sometimes
			m_lpLogger->Log(EC_LOGLEVEL_INFO, "ignored change parameter");
			hr = hrSuccess;
			goto next;
		// TODO handle SYNC_E_OBJECT_DELETED, SYNC_E_CONFLICT, SYNC_E_NO_PARENT, SYNC_E_INCEST, SYNC_E_UNSYNCHRONIZED
		}else if(hr != hrSuccess){
			//m_lpLogger->Log(EC_LOGLEVEL_INFO, "change error: %s", stringify(hr, true).c_str());
			goto exit;
		} 

		if(lpDestMessage == NULL) {
			// Import succeeded, but we have not message to export to. Treat this the same as
			// SYNC_E_IGNORE. This is required for the BES ICS exporter
			goto next;
		}

		hr = lpSourceMessage->CopyTo(0, NULL, (LPSPropTagArray)&sptMessageExcludes, 0, NULL, &IID_IMessage, lpDestMessage, 0, NULL);
		if(hr != hrSuccess)
			goto exit;

		hr = lpSourceMessage->GetRecipientTable(0, &lpTable);
		if(hr !=  hrSuccess)
			goto exit;

		hr = lpTable->QueryColumns(TBL_ALL_COLUMNS, &lpPropTagArray);
		if (hr != hrSuccess)
			goto exit;

		hr = lpTable->SetColumns(lpPropTagArray, 0);
		if (hr != hrSuccess)
			goto exit;

		hr = lpTable->QueryRows(0xFFFF, 0, &lpRows);
		if(hr !=  hrSuccess)
			goto exit;
		
		//FIXME: named property in the recipienttable ?

		hr = lpDestMessage->ModifyRecipients(0, (LPADRLIST)lpRows);
		if(hr !=  hrSuccess)
			hr = hrSuccess;
			//goto exit;

		if(lpRows){
			FreeProws(lpRows);
			lpRows = NULL;
		}
		
		if(lpTable){
			lpTable->Release();
			lpTable = NULL;
		}

		//delete every attachment
		hr = lpDestMessage->GetAttachmentTable(0, &lpTable);
		if(hr !=  hrSuccess)
			goto exit;

		hr = lpTable->SetColumns((LPSPropTagArray)&sptAttach, 0);
		if(hr !=  hrSuccess)
			goto exit;

		hr = lpTable->QueryRows(0xFFFF, 0, &lpRows);
		if(hr !=  hrSuccess)
			goto exit;

		for(ulCount = 0; ulCount < lpRows->cRows; ulCount++){
			hr = lpDestMessage->DeleteAttach(lpRows->aRow[ulCount].lpProps[0].Value.ul, 0, NULL, 0);
			if(hr != hrSuccess)
				goto exit;
		}

		if(lpRows){
			FreeProws(lpRows);
			lpRows = NULL;
		}
		
		if(lpTable){
			lpTable->Release();
			lpTable = NULL;
		}

		//add every attachment
		hr = lpSourceMessage->GetAttachmentTable(0, &lpTable);
		if(hr !=  hrSuccess)
			goto exit;

		hr = lpTable->SetColumns((LPSPropTagArray)&sptAttach, 0);
		if(hr !=  hrSuccess)
			goto exit;

		hr = lpTable->QueryRows(0xFFFF, 0, &lpRows);
		if(hr !=  hrSuccess)
			goto exit;

		for(ulCount = 0; ulCount < lpRows->cRows; ulCount++){
			hr = lpSourceMessage->OpenAttach(lpRows->aRow[ulCount].lpProps[0].Value.ul, &IID_IAttachment, 0, &lpSourceAttach);
			if(hr !=  hrSuccess)
				goto exit;

			hr = lpDestMessage->CreateAttach(&IID_IAttachment, 0, &ulObjType, &lpDestAttach);
			if(hr !=  hrSuccess)
				goto exit;

			hr = lpSourceAttach->CopyTo(0, NULL, (LPSPropTagArray)&sptAttach, 0, NULL, &IID_IAttachment, lpDestAttach, 0, NULL);
			if(hr !=  hrSuccess)
				goto exit;
			
			hr = lpDestAttach->SaveChanges(0);
			if(hr !=  hrSuccess)
				goto exit;

			if(lpSourceAttach){
				lpSourceAttach->Release();
				lpSourceAttach = NULL;
			}

			if(lpDestAttach){
				lpDestAttach->Release();
				lpDestAttach = NULL;
			}
		}
		
		if(lpRows){
			FreeProws(lpRows);
			lpRows = NULL;
		}
		
		if(lpTable){
			lpTable->Release();
			lpTable = NULL;
		}

		if(lpPropTagArray){
			MAPIFreeBuffer(lpPropTagArray);
			lpPropTagArray = NULL;
		}

		hr = lpSourceMessage->GetPropList(0, &lpPropTagArray);
		if (hr != hrSuccess)
			goto exit;

		hr = Util::HrDeleteResidualProps(lpDestMessage, lpSourceMessage, lpPropTagArray);
		if (hr != hrSuccess)
			goto exit;

		hr = lpDestMessage->SaveChanges(0);
		if(hr != hrSuccess)
			goto exit;


next:
		// Mark this change as processed, even if we skipped it due to SYNC_E_IGNORE or because the item was deleted on the source server
	    
		m_setProcessedChanges.insert(std::pair<unsigned int, std::string>(m_lstChange.at(m_ulStep).ulChangeId, std::string((char *)m_lstChange.at(m_ulStep).sSourceKey.lpb, m_lstChange.at(m_ulStep).sSourceKey.cb)));
	    
		if(lpEntryID) {
			MAPIFreeBuffer(lpEntryID);
			lpEntryID = NULL;
		}
		
		if(lpRows){
			FreeProws(lpRows);
			lpRows = NULL;
		}
		
		if(lpTable){
			lpTable->Release();
			lpTable = NULL;
		}

		if(lpPropArray){
			MAPIFreeBuffer(lpPropArray);
			lpPropArray = NULL;
		}

		if(lpSourceAttach){
			lpSourceAttach->Release();
			lpSourceAttach = NULL;
		}

		if(lpDestAttach){
			lpDestAttach->Release();
			lpDestAttach = NULL;
		}
		
		if(lpSourceMessage){
			lpSourceMessage->Release();
			lpSourceMessage = NULL;
		}
		
		if(lpDestMessage){
			lpDestMessage->Release();
			lpDestMessage = NULL;
		}

		if(lpPropTagArray){
			MAPIFreeBuffer(lpPropTagArray);
			lpPropTagArray = NULL;
		}

		m_ulStep++;
		ulSteps++;
	}

	if(m_ulStep < m_lstChange.size())
		hr = SYNC_W_PROGRESS;
exit:
	if(hr != hrSuccess && hr != SYNC_W_PROGRESS)
		m_lpLogger->Log(EC_LOGLEVEL_INFO, "change error: %s", stringify(hr, true).c_str());

	if(lpEntryID)
		MAPIFreeBuffer(lpEntryID);
		
	if(lpRows)
		FreeProws(lpRows);
	
	if(lpTable)
		lpTable->Release();

	if(lpDestAttach)
		lpDestAttach->Release();

	if(lpDestMessage)
		lpDestMessage->Release();

	if(lpSourceAttach)
		lpSourceAttach->Release();

	if(lpSourceMessage)
		lpSourceMessage->Release();

	if(lpPropArray)
		MAPIFreeBuffer(lpPropArray);

	if(lpPropTagArray)
			MAPIFreeBuffer(lpPropTagArray);

	return hr;
}

HRESULT ECExchangeExportChanges::ExportMessageChangesFast() 
{
	HRESULT				hr = hrSuccess;
	HRESULT				hrAsync = hrSuccess;
	LPSTREAM			lpDstStream = NULL;
	ECStreamInfo		sStreamInfo = {0};
	LPSPropValue		lpPropVal = NULL;
	LPSPropValue		lpsPropArray = NULL;
	ULONG				ulFlags = 0;
	ULONG				cbRead = 0;
	unsigned char		idLen = 0;
	char				id[256] = {0};

	// The very first time, we need to get the source stream.
	if (m_ulStep == 0 || m_lpsStreamOps == NULL) {
		hr = GetMessageStream();
		if (hr == MAPI_E_UNABLE_TO_COMPLETE) {
			hr = hrSuccess;
			goto exit;
		}

		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to get message stream. hr=0x%08x", hr);
			goto exit;
		}
	}
	

	// Determine the id of the current stream, blocks until data is written on the other side. Once this
	// happens the StreamInfo map is updated with information about all streams that will be returned.
	hr = m_lpsStreamOps->Read(&idLen, sizeof(idLen), &cbRead);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to read size of identifier from stream. hr=0x%08x", hr);
		goto exit;
	}

	if (cbRead != sizeof(idLen)) {
		// cbRead == 0 --> End of stream
		if (cbRead == 0) {
			// End of stream while we were expecting some data. This can mean two things:
			// - stream broken due to sending error on the source
			// - messages deleted just before sending
			
			if (m_ulStep + m_ulBatchSize < m_lstChange.size()) {
				// Not last batch
				
				// Normally, ulStep should be equal to m_ulBatchEnd when we get here. However,
				// when a network error has occurred, we may read 0 bytes before we reach m_ulBatchEnd. Since
				// this error will be detected later through CloseAndGetAsyncResult(), we just force the current
				// step to the last step in this batch so that that we can process the result for the stream.
				m_ulStep = m_ulBatchEnd;
				hr = SYNC_W_PROGRESS;
			} else
				// Last batch
				m_ulStep = m_lstChange.size();
		}
		else {
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Read invalid size of identifier from stream. size=%u, expected=%u", cbRead, sizeof(idLen));
			hr = MAPI_E_INVALID_PARAMETER;
		}
		goto exit;
	}

	hr = m_lpsStreamOps->Read(id, idLen, &cbRead);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to read identifier from stream. hr=0x%08x", hr);
		goto exit;
	}
	if (cbRead != idLen) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Read unexpected amount of data for identifier from stream. size=%u, expected=%u", cbRead, idLen);
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Processing frame '%s'", id);
	hr = m_lpsStreamOps->GetStreamInfo(id, &sStreamInfo);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to obtain information about frame '%s'. hr=0x%08x", id, hr);
		goto exit;
	}

	m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Frame info: id='%s', step=%u, props=%u", id, sStreamInfo.ulStep, sStreamInfo.cbPropVals);

	ulFlags = 0;
	lpPropVal = PpropFindProp(sStreamInfo.lpsPropVals, sStreamInfo.cbPropVals, PR_MESSAGE_FLAGS);
	if (lpPropVal != NULL && (lpPropVal->Value.ul & MSGFLAG_ASSOCIATED))
		ulFlags |= SYNC_ASSOCIATED;
	if ((m_lstChange.at(m_ulStep).ulChangeType & ICS_ACTION_MASK) == ICS_NEW)
		ulFlags |= SYNC_NEW_MESSAGE;

	hr = m_lpImportStreamedContents->ImportMessageChangeAsAStream(sStreamInfo.cbPropVals, sStreamInfo.lpsPropVals, ulFlags, &lpDstStream);
	switch (hr) {
	case erSuccess:
		// Copy the message data
		hr = m_lpsStreamOps->CopyFrameTo(lpDstStream, NULL);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to copy frame to importer. hr=0x%08x", hr);
			goto exit;	
		}

		hr = CloseAndGetAsyncResult(lpDstStream, &hrAsync);
		if (hr != MAPI_E_INTERFACE_NOT_SUPPORTED) {
			if (hrAsync != hrSuccess) {	// Async operation failed.
				m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Async import operation failed. hr=0x%08x", hrAsync); 
				hr = hrAsync;
				goto exit;
			}
		} else if (hr != hrSuccess)	{ // CloseAndGetAsyncResult failed.
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to get async import result. hr=0x%08x", hr);
			goto exit;
		}
		m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Frame '%s' copied to importer", id);
		break;

	case SYNC_E_IGNORE:
	case SYNC_E_OBJECT_DELETED:
		m_lpLogger->Log(EC_LOGLEVEL_INFO, "ignored change");
		// Flush the message data
		hr = m_lpsStreamOps->FlushFrame();
		if (hr != hrSuccess)
			goto exit;
		break;

	default:
		goto exit;
	}

	UpdateProgress(sStreamInfo.ulStep + m_ulStepOffset + 1);
	if (m_ulStep < m_ulChanges)
		hr = SYNC_W_PROGRESS;
	
exit:
	if (hr == SYNC_W_PROGRESS && m_ulStep == m_ulBatchEnd) {
		ASSERT(m_lpsStreamOps != NULL);

		// See if the async operation completed successfully
		if (m_lpsStreamOps->CloseAndGetAsyncResult(&hrAsync) == hrSuccess && hrAsync != hrSuccess)
			hr = hrAsync;

		m_lpsStreamOps->Release();
		m_lpsStreamOps = NULL;;

		m_ulStepOffset += m_ulBatchSize;

		if (hr == SYNC_W_PROGRESS) {
			hr = GetMessageStream();
			if (hr == MAPI_E_UNABLE_TO_COMPLETE)
				hr = hrSuccess;
			else if (hr == hrSuccess) {
				// Restore original error code
				hr = SYNC_W_PROGRESS;
			}
		}
	}

	// Cleanup when we're done
	if (hr != SYNC_W_PROGRESS) {
		if (m_lpsStreamOps) {
			if (hr == hrSuccess) {
				// See if the async operation also completed successfully
				if (m_lpsStreamOps->CloseAndGetAsyncResult(&hrAsync) == hrSuccess && hrAsync != hrSuccess)
					hr = hrAsync;
			}

			m_lpsStreamOps->Release();
			m_lpsStreamOps = NULL;
		}
	}

	if (lpDstStream)
		lpDstStream->Release();
		
	if (lpsPropArray)
		MAPIFreeBuffer(lpsPropArray);

	return hr;
}

HRESULT ECExchangeExportChanges::ExportMessageFlags(){
	HRESULT			hr = hrSuccess;
	LPREADSTATE		lpReadState = NULL;
	ULONG			ulCount;
	std::list<ICSCHANGE>::iterator lpChange;
	
	if(m_lstFlag.empty())
		goto exit;

	MAPIAllocateBuffer(sizeof(READSTATE) * m_lstFlag.size(), (LPVOID *)&lpReadState);

	ulCount = 0;
	for(lpChange = m_lstFlag.begin(); lpChange != m_lstFlag.end(); lpChange++){

		MAPIAllocateMore(lpChange->sSourceKey.cb, lpReadState, (LPVOID *)&lpReadState[ulCount].pbSourceKey);
		lpReadState[ulCount].cbSourceKey = lpChange->sSourceKey.cb;
		memcpy(lpReadState[ulCount].pbSourceKey, lpChange->sSourceKey.lpb, lpChange->sSourceKey.cb );
		lpReadState[ulCount].ulFlags = lpChange->ulFlags;

		ulCount++;
	}

	if(ulCount > 0){
		hr = m_lpImportContents->ImportPerUserReadStateChange(ulCount, lpReadState);
		if (hr == SYNC_E_IGNORE){
			hr = hrSuccess;
		}
		if(hr != hrSuccess)
			goto exit;

		// Mark the flag changes as processed
		for(lpChange = m_lstFlag.begin(); lpChange != m_lstFlag.end(); lpChange++){
			m_setProcessedChanges.insert(std::pair<unsigned int, std::string>(lpChange->ulChangeId, std::string((char *)lpChange->sSourceKey.lpb, lpChange->sSourceKey.cb)));
		}
	}

exit:
	if (hr != hrSuccess)
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to sync message flags, 0x%08X", hr);

	if(lpReadState)
		MAPIFreeBuffer(lpReadState);

	return hr;
}

HRESULT ECExchangeExportChanges::ExportMessageDeletes(){
	HRESULT			hr = hrSuccess;
	
	LPENTRYLIST		lpEntryList = NULL;

	if(!m_lstSoftDelete.empty()){
		hr = ChangesToEntrylist(&m_lstSoftDelete, &lpEntryList);
		if(hr != hrSuccess)
			goto exit;

		hr = m_lpImportContents->ImportMessageDeletion(SYNC_SOFT_DELETE, lpEntryList);
		if (hr == SYNC_E_IGNORE){
			hr = hrSuccess;	
		}
		if(hr != hrSuccess)
			goto exit;

		hr = AddProcessedChanges(m_lstSoftDelete);
		if (hr != hrSuccess)
			goto exit;
	}
	
	if(lpEntryList){
		MAPIFreeBuffer(lpEntryList);
		lpEntryList = NULL;
	}

	if(!m_lstHardDelete.empty()){
		hr = ChangesToEntrylist(&m_lstHardDelete, &lpEntryList);
		if(hr != hrSuccess)
			goto exit;

		hr = m_lpImportContents->ImportMessageDeletion(0, lpEntryList);
		if (hr == SYNC_E_IGNORE){
			hr = hrSuccess;	
		}
		if(hr != hrSuccess)
			goto exit;

		hr = AddProcessedChanges(m_lstHardDelete);
		if (hr != hrSuccess)
			goto exit;
	}
	
exit:
	if(lpEntryList)
		MAPIFreeBuffer(lpEntryList);
	return hr;
}

HRESULT ECExchangeExportChanges::ExportFolderChanges(){
	HRESULT			hr = hrSuccess;

	LPMAPIFOLDER	lpFolder = NULL;
	LPSPropValue	lpPropArray = NULL;
	LPSPropValue	lpPropSK = NULL;
	LPSPropValue	lpPropVal = NULL;
	LPSPropValue	lpPropFolderType = NULL;

	ULONG			ulObjType = 0;
	ULONG			ulCount = 0;
	ULONG			ulSteps = 0;
	
	ULONG			cbEntryID = 0;
	LPENTRYID		lpEntryID = NULL;
	LPSTREAM		lpStream = NULL;
	STATSTG			sStat;
	ULONG			ulProp = 0;
	ULONG			ulRead = 0;

	SizedSPropTagArray(1, sptSK) = { 1, {PR_SOURCE_KEY} };
	
	//PR_LAST_MODIFICATION_TIME missing in documentation
	//The array must contain at least the PR_PARENT_SOURCE_KEY, PR_SOURCE_KEY, PR_CHANGE_KEY, PR_PREDECESSOR_CHANGE_LIST, and MAPI PR_DISPLAY_NAME properties.
	//SizedSPropTagArray(6, sptFolder) = { 6, {PR_PARENT_SOURCE_KEY, PR_SOURCE_KEY, PR_CHANGE_KEY, PR_PREDECESSOR_CHANGE_LIST, PR_DISPLAY_NAME, PR_LAST_MODIFICATION_TIME} };

	hr = m_lpFolder->GetProps((LPSPropTagArray)&sptSK, 0, &ulCount, &lpPropSK);
	if(hr != hrSuccess)
		goto exit;

	if(!lpPropSK){
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	while(m_ulStep < m_lstChange.size() && (m_ulBufferSize == 0 || ulSteps < m_ulBufferSize)){

		hr = m_lpFolder->GetMsgStore()->EntryIDFromSourceKey(
			m_lstChange.at(m_ulStep).sSourceKey.cb, 
			m_lstChange.at(m_ulStep).sSourceKey.lpb,
			0, NULL,
			&cbEntryID, &lpEntryID);

		if(hr != hrSuccess){
			m_lpLogger->Log(EC_LOGLEVEL_INFO, "change sourcekey not found");
			hr = hrSuccess;
			goto next;
		}
		m_lpLogger->Log(EC_LOGLEVEL_INFO, "change sourcekey: %s", bin2hex(m_lstChange.at(m_ulStep).sSourceKey.cb, m_lstChange.at(m_ulStep).sSourceKey.lpb).c_str());
		
		hr = m_lpFolder->OpenEntry(cbEntryID, lpEntryID, &IID_IMAPIFolder, MAPI_MODIFY, &ulObjType, (LPUNKNOWN*) &lpFolder);
		if(hr != hrSuccess){
			hr = hrSuccess;
			goto next;
		}

		hr = lpFolder->GetProps(m_lpChangePropTagArray /*NULL*/, 0, &ulCount, &lpPropArray);
		if(hr == MAPI_W_ERRORS_RETURNED){
			hr = hrSuccess;
		}else if(hr != hrSuccess)
			goto exit;

		// if NULL is given for include props in config function, we don't support large properties for now
		if(m_lpChangePropTagArray){
			for(ulProp = 0; ulProp < ulCount; ulProp++){
				if (PROP_TYPE(lpPropArray[ulProp].ulPropTag) != PT_ERROR || lpPropArray[ulProp].Value.err != MAPI_E_NOT_ENOUGH_MEMORY)
					goto nextprop;

				hr = lpFolder->OpenProperty(m_lpChangePropTagArray->aulPropTag[ulProp], &IID_IStream, 0, 0, (IUnknown **)&lpStream);
				if(hr != hrSuccess)
					goto nextprop;

				hr = lpStream->Stat(&sStat, 0);
				if(hr != hrSuccess)
					goto nextprop;

				switch(PROP_TYPE(m_lpChangePropTagArray->aulPropTag[ulProp])){
					case PT_STRING8:
						hr = MAPIAllocateMore(sStat.cbSize.QuadPart, lpPropArray, (LPVOID*)&lpPropArray[ulProp].Value.lpszA);
						if(hr != hrSuccess)
							goto nextprop;

						hr = lpStream->Read(lpPropArray[ulProp].Value.lpszA, sStat.cbSize.QuadPart, &ulRead);
						if(hr != hrSuccess)
							goto nextprop;
						lpPropArray[ulProp].ulPropTag = m_lpChangePropTagArray->aulPropTag[ulProp];

						break;
					case PT_BINARY:
						lpPropArray[ulProp].Value.bin.cb = 0;
						hr = MAPIAllocateMore(sStat.cbSize.QuadPart, lpPropArray, (LPVOID*)&lpPropArray[ulProp].Value.bin.lpb);
						if(hr != hrSuccess)
							goto nextprop;

						hr = lpStream->Read(lpPropArray[ulProp].Value.bin.lpb, sStat.cbSize.QuadPart, &ulRead);
						if(hr != hrSuccess)
							goto nextprop;

						lpPropArray[ulProp].ulPropTag = m_lpChangePropTagArray->aulPropTag[ulProp];
						lpPropArray[ulProp].Value.bin.cb = sStat.cbSize.QuadPart;

						break;
					default:
						break;
				}
nextprop:
				if(lpStream){
					lpStream->Release();
					lpStream = NULL;
				}
			}
		}

		lpPropFolderType = PpropFindProp(lpPropArray, ulCount, PR_FOLDER_TYPE);
		if(lpPropFolderType && lpPropFolderType->Value.ul == FOLDER_SEARCH){
			//skipping search folder
			goto next;
		}
		
		lpPropVal = PpropFindProp(lpPropArray, ulCount, PR_SOURCE_KEY);
		if(!lpPropVal){
			goto next;
		}

		//for folders directly under m_lpFolder PR_PARENT_SOURCE_KEY must be NULL
		//this protects against recursive problems during syncing when the PR_PARENT_SOURCE_KEY
		//equals the sourcekey of the folder which we are already syncing.
		//If the exporter sends an empty PR_PARENT_SOURCE_KEY to the importer the importer can
		//assume the parent is the folder which it is syncing.

		lpPropVal = PpropFindProp(lpPropArray, ulCount, PR_PARENT_SOURCE_KEY);
		if(lpPropVal && lpPropSK->Value.bin.cb == lpPropVal->Value.bin.cb && memcmp(lpPropVal->Value.bin.lpb, lpPropSK->Value.bin.lpb, lpPropSK->Value.bin.cb)==0){
			lpPropVal->Value.bin.cb = 0;
		}

		hr = m_lpImportHierarchy->ImportFolderChange(ulCount, lpPropArray);
		if (hr == SYNC_E_IGNORE){
			m_lpLogger->Log(EC_LOGLEVEL_INFO, "change ignored");
			hr = hrSuccess;
			goto next;
		}else if (hr == MAPI_E_INVALID_PARAMETER){
			m_lpLogger->Log(EC_LOGLEVEL_INFO, "change invalid parameter");
			hr = hrSuccess;
			goto next;
		}else if(hr == MAPI_E_NOT_FOUND){
			m_lpLogger->Log(EC_LOGLEVEL_INFO, "change not found");
			hr = hrSuccess;
			goto next;
		}else if(FAILED(hr)) {
			m_lpLogger->Log(EC_LOGLEVEL_INFO, "change error: %s", stringify(hr, true).c_str());
			goto exit;
		}else if(hr != hrSuccess){
			m_lpLogger->Log(EC_LOGLEVEL_WARNING, "change warning: %s", stringify(hr, true).c_str());
		}
next:
		// Mark this change as processed
		m_setProcessedChanges.insert(std::pair<unsigned int, std::string>(m_lstChange.at(m_ulStep).ulChangeId, std::string((char *)m_lstChange.at(m_ulStep).sSourceKey.lpb, m_lstChange.at(m_ulStep).sSourceKey.cb)));

		if(lpFolder){
			lpFolder->Release();
			lpFolder = NULL;
		}

		if(lpPropArray){
			MAPIFreeBuffer(lpPropArray);
			lpPropArray = NULL;
		}

		if(lpEntryID) {
			MAPIFreeBuffer(lpEntryID);
			lpEntryID = NULL;
		}
		
		ulSteps++;
		m_ulStep++;
	}
	
	if(m_ulStep < m_lstChange.size())
		hr = SYNC_W_PROGRESS;

exit:
	if(lpStream)
		lpStream->Release();

	if(lpEntryID)
		MAPIFreeBuffer(lpEntryID);

	if(lpPropSK)
		MAPIFreeBuffer(lpPropSK);

	if(lpFolder)
		lpFolder->Release();

	if(lpPropArray)
		MAPIFreeBuffer(lpPropArray);

	return hr;
}

HRESULT ECExchangeExportChanges::ExportFolderDeletes(){
	HRESULT			hr = hrSuccess;
	
	LPENTRYLIST		lpEntryList = NULL;

	if(!m_lstSoftDelete.empty()){
		hr = ChangesToEntrylist(&m_lstSoftDelete, &lpEntryList);
		if(hr != hrSuccess)
			goto exit;

		hr = m_lpImportHierarchy->ImportFolderDeletion(SYNC_SOFT_DELETE, lpEntryList);
		if (hr == SYNC_E_IGNORE){
			hr = hrSuccess;	
		}
		if(hr != hrSuccess)
			goto exit;

		hr = AddProcessedChanges(m_lstSoftDelete);
		if (hr != hrSuccess)
			goto exit;
	}
	
	if(lpEntryList){
		MAPIFreeBuffer(lpEntryList);
		lpEntryList = NULL;
	}

	if(!m_lstHardDelete.empty()){
		hr = ChangesToEntrylist(&m_lstHardDelete, &lpEntryList);
		if(hr != hrSuccess)
			goto exit;

		hr = m_lpImportHierarchy->ImportFolderDeletion(0, lpEntryList);
		if (hr == SYNC_E_IGNORE){
			hr = hrSuccess;	
		}
		if(hr != hrSuccess)
			goto exit;

		hr = AddProcessedChanges(m_lstHardDelete);
		if (hr != hrSuccess)
			goto exit;
	}
	
exit:
	if(lpEntryList)
		MAPIFreeBuffer(lpEntryList);

	return hr;
}

//write in the stream 4 bytes syncid, 4 bytes changeid, 4 bytes changecount, {4 bytes changeid, 4 bytes sourcekeysize, sourcekey}
HRESULT ECExchangeExportChanges::UpdateStream(LPSTREAM lpStream){
	HRESULT hr;
	LARGE_INTEGER liPos = {{0, 0}};
	ULARGE_INTEGER liZero = {{0, 0}};
	ULONG ulSize;
	ULONG ulChangeCount = 0;
	ULONG ulChangeId = 0;
	ULONG ulSourceKeySize = 0;
	PROCESSEDCHANGESSET::iterator iterProcessedChange;

	hr = lpStream->SetSize(liZero);
	if(hr != hrSuccess)
		goto exit;

	hr = lpStream->Seek(liPos, STREAM_SEEK_SET, NULL);
	if(hr != hrSuccess)
		goto exit;

	hr = lpStream->Write(&m_ulSyncId, 4, &ulSize);
	if(hr != hrSuccess)
		goto exit;

	if(m_ulSyncId == 0){
		m_ulChangeId = 0;
	}

	hr = lpStream->Write(&m_ulChangeId, 4, &ulSize);
	if(hr != hrSuccess)
		goto exit;

	if(!m_setProcessedChanges.empty()) {
		ulChangeCount = m_setProcessedChanges.size();
		
		hr = lpStream->Write(&ulChangeCount, 4, &ulSize);
		if(hr != hrSuccess)
			goto exit;
		
		for(iterProcessedChange = m_setProcessedChanges.begin(); iterProcessedChange != m_setProcessedChanges.end(); iterProcessedChange++) {
			ulChangeId = iterProcessedChange->first;
			hr = lpStream->Write(&ulChangeId, 4, &ulSize);
			if(hr != hrSuccess)
				goto exit;
			
			ulSourceKeySize = iterProcessedChange->second.size();
			
			hr = lpStream->Write(&ulSourceKeySize, 4, &ulSize);
			if(hr != hrSuccess)
				goto exit;
				
			hr = lpStream->Write(iterProcessedChange->second.c_str(), iterProcessedChange->second.size(), &ulSize);
			if(hr != hrSuccess)
				goto exit;
		}
	}

	// Seek back to the beginning after we've finished
	lpStream->Seek(liPos, STREAM_SEEK_SET, NULL);

exit:
	return hr;
}

//convert (delete) changes to entrylist for message and folder deletion.
HRESULT ECExchangeExportChanges::ChangesToEntrylist(std::list<ICSCHANGE> * lpLstChanges, LPENTRYLIST * lppEntryList){
	HRESULT 		hr = hrSuccess;
	LPENTRYLIST		lpEntryList = NULL;
	ULONG			ulCount = 0;
	std::list<ICSCHANGE>::iterator lpChange;
	
	MAPIAllocateBuffer(sizeof(ENTRYLIST), (LPVOID *)&lpEntryList);
	lpEntryList->cValues = lpLstChanges->size();
	if(lpEntryList->cValues > 0){
		MAPIAllocateMore(sizeof(SBinary) * lpEntryList->cValues, lpEntryList, (LPVOID *)&lpEntryList->lpbin);
	}else{
		lpEntryList->lpbin = NULL;
	}
	ulCount = 0;
	for(lpChange = lpLstChanges->begin(); lpChange != lpLstChanges->end(); lpChange++){

		lpEntryList->lpbin[ulCount].cb = lpChange->sSourceKey.cb;
		MAPIAllocateMore(lpChange->sSourceKey.cb, lpEntryList, (void **)&lpEntryList->lpbin[ulCount].lpb);
		memcpy(lpEntryList->lpbin[ulCount].lpb, lpChange->sSourceKey.lpb, lpChange->sSourceKey.cb);
		ulCount++;
	}
	
	lpEntryList->cValues = ulCount;

	*lppEntryList = lpEntryList;

	if(hr != hrSuccess && lpEntryList)
		MAPIFreeBuffer(lpEntryList);

	return hr;
}

/**
 * Add processed changes to the precessed changes list
 *
 * @param[in] lstChanges	List with changes
 *
 */
HRESULT ECExchangeExportChanges::AddProcessedChanges(ChangeList &lstChanges)
{
	ChangeListIter iterChange;

	for(iterChange = lstChanges.begin(); iterChange != lstChanges.end(); iterChange++) 
		m_setProcessedChanges.insert(std::pair<unsigned int, std::string>(iterChange->ulChangeId, std::string((char *)iterChange->sSourceKey.lpb, iterChange->sSourceKey.cb)));

	return hrSuccess;
}

HRESULT ECExchangeExportChanges::UpdateProgress(ULONG ulNewStep)
{
	// Mark message as processed.
	m_setProcessedChanges.insert(std::pair<unsigned int, std::string>(m_lstChange.at(m_ulStep).ulChangeId, std::string((char *)m_lstChange.at(m_ulStep).sSourceKey.lpb, m_lstChange.at(m_ulStep).sSourceKey.cb)));

	// Update progress
	m_ulStep = ulNewStep;

	return hrSuccess;
}

HRESULT ECExchangeExportChanges::GetMessageStream()
{
	HRESULT hr = hrSuccess;
	std::set<unsigned long> setSteps;

	// Construct a subset of m_lstChange
	m_ulStep = m_ulBatchNextStart;
	std::vector<ICSCHANGE>::const_iterator iterStart(m_lstChange.begin() + m_ulStep);
	std::vector<ICSCHANGE>::const_iterator iterLast;
	if (m_lstChange.size() > m_ulStep + m_ulBatchSize)
		iterLast = iterStart + m_ulBatchSize;
	else
		iterLast = m_lstChange.end();
	m_ulBatchNextStart = m_ulStep + std::distance(iterStart, iterLast);

	std::vector<ICSCHANGE> lstBatch(iterStart, iterLast);
	if (m_lpLogger->Log(EC_LOGLEVEL_DEBUG)) {
		m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Requesting stream for batch of %u message(s)", lstBatch.size());
		for (unsigned i = 0; i < lstBatch.size(); ++i)
			m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "  %3u: changeid=%u, type=%u, sourcekey=%s", i + m_ulStep, lstBatch[i].ulChangeId, lstBatch[i].ulChangeType, bin2hex(lstBatch[i].sSourceKey.cb, lstBatch[i].sSourceKey.lpb).c_str());
	}

	hr = m_lpFolder->ExportMessageChangesAsStream((m_ulFlags & (SYNC_BEST_BODY | SYNC_LIMITED_IMESSAGE)), lstBatch, m_lpChangePropTagArray, &m_lpsStreamOps);
	if (hr != hrSuccess)
		goto exit;

	hr = m_lpsStreamOps->GetSteps(&setSteps);
	if (hr != hrSuccess)
		goto exit;

	if (m_lpLogger->Log(EC_LOGLEVEL_DEBUG))
		m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Got a stream for %u message(s) from the server", setSteps.size());

	// All steps that are in the requested range, but not in setSteps are deleted on the exporter side. So they're hereby processed.
	// If a request is made for range A:B from m_lstChange, the server will return range 0:(B-A)
	m_ulBatchEnd = m_ulStep;
	for (ULONG ulStep = 0; ulStep < std::distance(iterStart, iterLast); ++ulStep) {
		const ULONG ulLocalStep = ulStep + m_ulStep;
		if (setSteps.find(ulStep) == setSteps.end()) {
			m_setProcessedChanges.insert(std::pair<unsigned int, std::string>(m_lstChange.at(ulLocalStep).ulChangeId, std::string((char *)m_lstChange.at(ulLocalStep).sSourceKey.lpb, m_lstChange.at(ulLocalStep).sSourceKey.cb)));
			m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "  %3u: DELETED changeid=%u, type=%u, sourcekey=%s", ulLocalStep, m_lstChange[ulLocalStep].ulChangeId, m_lstChange[ulLocalStep].ulChangeType, bin2hex(m_lstChange[ulLocalStep].sSourceKey.cb, m_lstChange[ulLocalStep].sSourceKey.lpb).c_str());
		} else
			m_ulBatchEnd = ulLocalStep + 1;
	}

exit:
	return hr;
}

/**
 * Obtain the WSStreamOps interface of a stream, stop it's async operation and get the result. Note that
 * the operation should already be completed, but an implicit end is normally signalled through Release().
 *
 * @param[in]	lpStream
 *					The Stream object that should be stopped and inspected.
 * @param[out]	lphrResult
 *					Pointer to a HRESULT value that will be set to the result of the async operation.
 *
 * @return	HRESULT
 * @retval	MAPI_E_INVALID_PARAMETER		One of the provided arguments is NULL.
 * @retval	MAPI_E_UNCONFIGURED				There was no async operation bound to the WSStreamOps object.
 */
HRESULT ECExchangeExportChanges::CloseAndGetAsyncResult(IStream *lpStream, HRESULT *lphrResult)
{
	HRESULT hr = hrSuccess;
	WSStreamOps *lpStreamOps = NULL;

	if (lpStream == NULL || lphrResult == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}
	
	hr = lpStream->QueryInterface(IID_ECStreamOps, (void**)&lpStreamOps);
	if (hr != hrSuccess) {
		// It's possible to stream to something else than an ECStreamOps object (ie. The indexer streams to a memstream).
		if (hr == MAPI_E_INTERFACE_NOT_SUPPORTED)
			hr = hrSuccess;
		goto exit;
	}

	hr = lpStreamOps->CloseAndGetAsyncResult(lphrResult);

exit:
	if (lpStreamOps)
		lpStreamOps->Release();

	return hr;
}
