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

#ifndef ECSCHEDULER_H
#define ECSCHEDULER_H

#include "ECLogger.h"

#include <pthread.h>
#include <list>

enum eSchedulerType{
	SCHEDULE_NONE,
	SCHEDULE_SECONDS,
	SCHEDULE_MINUTES,
	SCHEDULE_HOUR,
	SCHEDULE_DAY,
	SCHEDULE_MONTH
};

typedef struct tagSchedule
{
	eSchedulerType	eType;
	unsigned int	ulBeginCycle;
	time_t			tLastRunTime;
	void*			(*lpFunction)(void*);
	void*			lpData;
}ECSCHEDULE, *LPECSCHEDULE;

typedef std::list<ECSCHEDULE> ECScheduleList;

class ECScheduler
{
public:
	ECScheduler(ECLogger *lpLogger);
	~ECScheduler(void);

	HRESULT AddSchedule(eSchedulerType eType, unsigned int ulBeginCycle, void* (*lpFunction)(void*), void* lpData = NULL);

private:
	static bool hasExpired(time_t ttime, LPECSCHEDULE lpSchedule);
	static void* ScheduleThread(void* lpTmpScheduler);

private:
	ECScheduleList		m_listScheduler;
	ECLogger *			m_lpLogger;

	bool				m_bExit;
	pthread_mutex_t     m_hExitMutex;			// Mutex needed for the release signal
	pthread_cond_t		m_hExitSignal;			// Signal that should be send to the Scheduler when to exit 
	pthread_mutex_t		m_hSchedulerMutex;		// Mutex for the locking of the Scheduler
	pthread_t			m_hMainThread;			// Thread that is used for the Scheduler
};

#endif
