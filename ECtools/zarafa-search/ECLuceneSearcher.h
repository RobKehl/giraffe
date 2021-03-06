/*
 * Copyright 2005 - 2013  Zarafa B.V.
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

#ifndef ECLUCENESEARCHER_H
#define ECLUCENESEARCHER_H

#include <string>
#include <vector>

#include <pthread.h>

#include <CLucene.h>
#include <CLucene/search/QueryFilter.h>

#include <ECUnknown.h>

#include "zarafa-search.h"

class ECIndexDB;

typedef struct {
	std::set<unsigned int> setFields;
	std::string strTerm;
} SIndexedTerm;

class ECLuceneAccess;

/**
 * Main class to perform searching by CLucene
 */
class ECLuceneSearcher : public ECUnknown {
private:
	/**
	 * Constructor
	 *
	 * @note Objects of ECLuceneSearcher must only be created using the Create() function.
	 *
	 * @param[in]	lpThreadData
	 * @param[in]	lpLuceneAccess
	 * @param[in]	listFolder
	 */
	ECLuceneSearcher(ECThreadData *lpThreadData, GUID *lpServerGuid, GUID *lpStoreGuid, std::list<unsigned int> &lstFolders, unsigned int ulMaxResults);

public:
	/**
	 * Create new ECLuceneSearcher object.
	 *
	 * @note Creating a new ECLuceneSearcher object must always occur through this function.
	 *
	 * @param[in]	lpThreadData
	 *					Reference to the ECThreadData object.
	 * @param[in]	lpServerGuid
	 *					Guid of the server to search in
	 * @param[in]	lpStoreGuid
	 *					Guid of the store to search in
	 * @param[in]	lstFolders
	 *					List of folders to search in
	 * @param[in]	ulMaxResults
	 *					Maximum  number of results to produce
	 * @param[out]	lppSearcher
	 *					The created ECLuceneSearcher object.
	 * @return HRESULT
	 */
	static HRESULT Create(ECThreadData *lpThreadData, GUID *lpServerGuid, GUID *lpStoreGuid, std::list<unsigned int> &lstFolders, unsigned int ulMaxResults, ECLuceneSearcher **lppSearcher);

	/**
	 * Add a search term to be searched for
	 *
	 * Note that items returned by SearchEntries() must match all terms passed to AddTerm()
	 *
	 * @param[in]	setFields
	 *					Fields to search in
	 * @param[in]	strTerm
	 *					Term to search (utf-8 encoded)
	 * @return HRESULT
	 */
	HRESULT AddTerm(std::set<unsigned int> &setFields, std::string &strTerm);

	/**
	 * Destructor
	 */
	~ECLuceneSearcher();

	/**
	 * Execute query to search for messages
	 *
	 * @param[out]	lplistResults
	 *					List of search results
	 * @return HRESULT
	 */
	HRESULT SearchEntries(std::list<unsigned int> *lplistResults);

private:
	ECThreadData *m_lpThreadData;
	ECLuceneAccess *m_lpLuceneAccess;

	std::list<unsigned int> m_lstFolders;
	std::string m_strStoreGuid;
	std::string m_strServerGuid;
	unsigned int m_ulMaxResults;
	std::list<SIndexedTerm> m_lstSearches;
	ECIndexDB *m_lpIndex;
};

#endif /* ECLUCENESEARCHER_H */
