/*
 * Copyright 2005 - 2012  Zarafa B.V.
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

#include <Python.h>
#include <mapi.h>
#include <mapix.h>
#include <mapiutil.h>
#include <mapidefs.h>
#include <mapiext.h>
#include <mapiguid.h>
#include "PyMapiPlugin.h"
#include "stringutil.h"
#include "frameobject.h"

/**
 * workaround compile issue with ASSERT macro in vs 2008
 *
 * It's not possible to compile in visual studio 2008 with 
 * ASSERT in macro using this function will fix it
 */
void assertbreak()
{
	ASSERT(FALSE);
}

#define NEW_SWIG_INTERFACE_POINTER_OBJ(pyswigobj, objpointer, typeobj) {\
	if (objpointer) {\
		pyswigobj = SWIG_NewPointerObj((void*)objpointer, typeobj, SWIG_POINTER_OWN | 0);\
		PY_HANDLE_ERROR(m_lpLogger, pyswigobj) \
		\
		objpointer->AddRef();\
	} else {\
		pyswigobj = Py_None;\
	    Py_INCREF(Py_None);\
	}\
}

#define NEW_SWIG_POINTER_OBJ(pyswigobj, objpointer, typeobj) {\
	if (objpointer) {\
		pyswigobj = SWIG_NewPointerObj((void*)objpointer, typeobj, SWIG_POINTER_OWN | 0);\
		PY_HANDLE_ERROR(m_lpLogger, pyswigobj) \
		\
	} else {\
		pyswigobj = Py_None;\
		Py_INCREF(Py_None);\
	}\
}

#define DECREF_PYOBJ(pyswigobj) { if (pyswigobj) { Py_DECREF(pyswigobj); pyswigobj = NULL;} }

#define BUILD_SWIG_TYPE(pyswigobj, type) {\
	pyswigobj = SWIG_TypeQuery(type);\
	if (!pyswigobj) {\
		assertbreak();\
		hr = S_FALSE;\
		goto exit;\
	}\
}

/**
 * Handle the python errors
 * 
 * note: The traceback doesn't work very well
 */
HRESULT PyHandleError(ECLogger *lpLogger, PyObject *pyobj)
{
	HRESULT hr = hrSuccess;

	if (!pyobj) 
	{ 
		PyObject *lpErr = PyErr_Occurred();
		if(lpErr) {
			PyObjectAPtr ptype, pvalue, ptraceback;
			PyErr_Fetch(&ptype, &pvalue, &ptraceback);
			
			PyTracebackObject* traceback = (PyTracebackObject*)(*ptraceback);
			
			char *pStrErrorMessage = "Unknown";
			char *pStrType = "Unknown";

			if (*pvalue) pStrErrorMessage = PyString_AsString(pvalue);
			if (*ptype) pStrType = PyString_AsString(ptype);
			
			if (lpLogger)
			{
				lpLogger->Log(EC_LOGLEVEL_ERROR, "  Python type: %s", pStrType);
				lpLogger->Log(EC_LOGLEVEL_ERROR, "  Python error: %s", pStrErrorMessage);
				
				while (traceback && traceback->tb_next != NULL) {
					PyFrameObject *frame = (PyFrameObject *)traceback->tb_frame;
					if (frame) {
						int line = frame->f_lineno;
						const char *filename = PyString_AsString(frame->f_code->co_filename); 
						const char *funcname = PyString_AsString(frame->f_code->co_name); 
						
						lpLogger->Log(EC_LOGLEVEL_ERROR, "  Python trace: %s(%d) %s", filename, line, funcname);
					} else { 
						lpLogger->Log(EC_LOGLEVEL_ERROR, "  Python trace: Unknown");
					}
					
					traceback = traceback->tb_next;
				}
			}

			PyErr_Clear();
		} 
		assertbreak(); 
		hr = S_FALSE; 
	}

	return hr;
}

#define PY_HANDLE_ERROR(logger, pyobj) { \
	hr = PyHandleError(logger, pyobj);\
	if (hr != hrSuccess) \
		goto exit; \
}

#define PY_CALL_METHOD(pluginmanager, functionname, returnmacro, format, ...) {\
	PyObjectAPtr ptrResult;\
	{\
		ptrResult = PyObject_CallMethod(pluginmanager, functionname, format, __VA_ARGS__);\
		PY_HANDLE_ERROR(m_lpLogger, ptrResult)\
		\
		returnmacro\
	}\
}

/** 
 * Helper macro to parse the python return values which work together 
 * with the macro PY_CALL_METHOD.
 * 
 */
#define PY_PARSE_TUPLE_HELPER(format, ...) {\
	if(!PyArg_ParseTuple(ptrResult, format, __VA_ARGS__)) { \
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "  Wrong return value from the pluginmanager or plugin"); \
		PY_HANDLE_ERROR(m_lpLogger, false) \
	} \
}

PyMapiPlugin::PyMapiPlugin(void)
{
	m_lpLogger = NULL;

	type_p_ECLogger = NULL;
	type_p_IAddrBook = NULL;
	type_p_IMAPIFolder = NULL;
	type_p_IMAPISession = NULL;
	type_p_IMsgStore = NULL;
	type_p_IMessage = NULL;
	type_p_IExchangeModifyTable = NULL;

	m_bEnablePlugin = false;
}

PyMapiPlugin::~PyMapiPlugin(void)
{
    m_ptrModMapiPlugin = NULL;
    m_ptrPyLogger = NULL;
	m_ptrMapiPluginManager = NULL;

	if(m_bEnablePlugin)
		Py_Finalize();
}

/**
 * Initialize the PyMapiPlugin.
 *
 * @param[in]	lpConfig Pointer to the configuration class
 * @param[in]	lpLogger Pointer to the logger
 * @param[in]	lpPluginManagerClassName The class name of the plugin handler
 *
 * @return Standard mapi errorcodes
 */
HRESULT PyMapiPlugin::Init(ECConfig* lpConfig, ECLogger *lpLogger, const char* lpPluginManagerClassName)
{
	HRESULT hr = S_OK;
	char *lpPluginPath = NULL;
	char *lpEnvPython = NULL;
	PyObjectAPtr	ptrName;
	PyObjectAPtr	ptrModule;
	PyObject*	lpMainmod = NULL;
	PyObjectAPtr	ptrClass;
	PyObjectAPtr	ptrArgs;
	std::string		strEnvPython;

	m_lpLogger = lpLogger;

	m_bEnablePlugin = parseBool(lpConfig->GetSetting("plugin_enabled", NULL, "no"));
	if (!m_bEnablePlugin)
		goto exit;

	strEnvPython = lpConfig->GetSetting("plugin_manager_path");

	lpEnvPython = getenv("PYTHONPATH");
	if (lpEnvPython) 
		strEnvPython += std::string(":") + lpEnvPython;

	setenv("PYTHONPATH", strEnvPython.c_str(), 1);

	lpLogger->Log(EC_LOGLEVEL_DEBUG, "PYTHONPATH = %s", strEnvPython.c_str());

	Py_Initialize();

	lpMainmod = PyImport_AddModule("__main__");
	
	ptrModule = PyImport_ImportModule("MAPI");
	PY_HANDLE_ERROR(m_lpLogger, ptrModule);

	// Init MAPI-swig types
	BUILD_SWIG_TYPE(type_p_IMessage, "_p_IMessage");
	BUILD_SWIG_TYPE(type_p_IMAPISession, "_p_IMAPISession");
	BUILD_SWIG_TYPE(type_p_IMsgStore, "_p_IMsgStore");
	BUILD_SWIG_TYPE(type_p_IAddrBook, "_p_IAddrBook");
	BUILD_SWIG_TYPE(type_p_IMAPIFolder, "_p_IMAPIFolder");
	BUILD_SWIG_TYPE(type_p_ECLogger, "_p_ECLogger");
	BUILD_SWIG_TYPE(type_p_IExchangeModifyTable, "_p_IExchangeModifyTable");

	lpPluginPath = lpConfig->GetSetting("plugin_path");

	// Import python plugin framework
	// @todo error unable to find file xxx
	ptrName = PyString_FromString("mapiplugin");

	m_ptrModMapiPlugin = PyImport_Import(ptrName);
	PY_HANDLE_ERROR(m_lpLogger, m_ptrModMapiPlugin);

	// Init logger swig object
	NEW_SWIG_INTERFACE_POINTER_OBJ(m_ptrPyLogger, m_lpLogger,  type_p_ECLogger);

	// Init plugin class	
	ptrClass = PyObject_GetAttrString(m_ptrModMapiPlugin, (char*)lpPluginManagerClassName);
	PY_HANDLE_ERROR(m_lpLogger, ptrClass);

	ptrArgs  = Py_BuildValue("(sO)", lpPluginPath, *m_ptrPyLogger);
	PY_HANDLE_ERROR(m_lpLogger, ptrArgs);

	m_ptrMapiPluginManager = PyObject_CallObject(ptrClass, ptrArgs);
	PY_HANDLE_ERROR(m_lpLogger, m_ptrMapiPluginManager);
exit:
	return hr;
}

/**
 * Plugin python call between MAPI and python.
 *
 * @param[in]	lpFunctionName	Python function name to call in the plugin framework. 
 * 								 The function must be exist the lpPluginManagerClassName defined in the init function.
 * @param[in] lpMapiSession		Pointer to a mapi session. Not NULL.
 * @param[in] lpAdrBook			Pointer to a mapi Addressbook. Not NULL.
 * @param[in] lpMsgStore		Pointer to a mapi mailbox. Can be NULL.
 * @param[in] lpInbox
 * @param[in] lpMessage			Pointer to a mapi message.
 *
 * @return Default mapi error codes
 *
 * @todo something with exit codes
 */
HRESULT PyMapiPlugin::MessageProcessing(const char *lpFunctionName, IMAPISession *lpMapiSession, IAddrBook *lpAdrBook, IMsgStore *lpMsgStore, IMAPIFolder *lpInbox, IMessage *lpMessage, ULONG *lpulResult)
{
	HRESULT hr = hrSuccess;
	PyObjectAPtr ptrPySession, ptrPyAddrBook, ptrPyStore, ptrPyMessage, ptrPyFolderInbox, ptrPyLogger;

	if (!m_bEnablePlugin)
		goto exit;

	if (!lpFunctionName || !lpMapiSession || !lpAdrBook) {
		hr = MAPI_E_INVALID_PARAMETER; 
		goto exit;
	}

	if (!m_ptrMapiPluginManager) {
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	NEW_SWIG_INTERFACE_POINTER_OBJ(ptrPySession, lpMapiSession, type_p_IMAPISession)
	NEW_SWIG_INTERFACE_POINTER_OBJ(ptrPyAddrBook, lpAdrBook, type_p_IAddrBook)
	NEW_SWIG_INTERFACE_POINTER_OBJ(ptrPyStore, lpMsgStore, type_p_IMsgStore)
	NEW_SWIG_INTERFACE_POINTER_OBJ(ptrPyFolderInbox, lpInbox, type_p_IMAPIFolder)
	NEW_SWIG_INTERFACE_POINTER_OBJ(ptrPyMessage, lpMessage, type_p_IMessage)

	// Call the python function and get the (hr) return code back
	PY_CALL_METHOD(m_ptrMapiPluginManager, (char*)lpFunctionName, PY_PARSE_TUPLE_HELPER("I", lpulResult), "OOOOO", *ptrPySession, *ptrPyAddrBook, *ptrPyStore, *ptrPyFolderInbox, *ptrPyMessage);


exit:
	return hr;
}

/**
 * Hook for change the rules.
 *
 * @param[in] lpFunctionName	Python function name to hook the rules in the plugin framework. 
 * 								 The function must be exist the lpPluginManagerClassName defined in the init function.
 * @param[in] lpEMTRules		Pointer to a mapi IExchangeModifyTable object
 */
HRESULT PyMapiPlugin::RulesProcessing(const char *lpFunctionName, IMAPISession *lpMapiSession, IAddrBook *lpAdrBook, IMsgStore *lpMsgStore, IExchangeModifyTable *lpEMTRules, ULONG *lpulResult)
{
	HRESULT hr = hrSuccess;
    PyObjectAPtr  ptrPySession, ptrPyAddrBook, ptrPyStore, ptrEMTIn;

	if (!m_bEnablePlugin)
		goto exit;

	if (!lpFunctionName || !lpMapiSession || !lpAdrBook || !lpEMTRules) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (!m_ptrMapiPluginManager) {
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}
	
	NEW_SWIG_INTERFACE_POINTER_OBJ(ptrPySession, lpMapiSession, type_p_IMAPISession)
	NEW_SWIG_INTERFACE_POINTER_OBJ(ptrPyAddrBook, lpAdrBook, type_p_IAddrBook)
	NEW_SWIG_INTERFACE_POINTER_OBJ(ptrPyStore, lpMsgStore, type_p_IMsgStore)
	NEW_SWIG_INTERFACE_POINTER_OBJ(ptrEMTIn, lpEMTRules, type_p_IExchangeModifyTable)

	PY_CALL_METHOD(m_ptrMapiPluginManager, ((char*)lpFunctionName), PY_PARSE_TUPLE_HELPER("I", lpulResult), "OOOO", *ptrPySession, *ptrPyAddrBook, *ptrPyStore, *ptrEMTIn);

exit:
	return hr;
}

HRESULT PyMapiPlugin::RequestCallExecution(const char *lpFunctionName, IMAPISession *lpMapiSession, IAddrBook *lpAdrBook, IMsgStore *lpMsgStore,  IMAPIFolder *lpFolder, IMessage *lpMessage, ULONG *lpulDoCallexe, ULONG *lpulResult)
{
	HRESULT hr = hrSuccess;
	PyObjectAPtr  ptrPySession, ptrPyAddrBook, ptrPyStore, ptrFolder, ptrMessage;

	if (!m_bEnablePlugin)
		goto exit;

	if (!lpFunctionName || !lpMapiSession || !lpAdrBook || !lpulDoCallexe) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (!m_ptrMapiPluginManager) {
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	NEW_SWIG_INTERFACE_POINTER_OBJ(ptrPySession, lpMapiSession, type_p_IMAPISession)
	NEW_SWIG_INTERFACE_POINTER_OBJ(ptrPyAddrBook, lpAdrBook, type_p_IAddrBook)
	NEW_SWIG_INTERFACE_POINTER_OBJ(ptrPyStore, lpMsgStore, type_p_IMsgStore)
	NEW_SWIG_INTERFACE_POINTER_OBJ(ptrFolder, lpFolder, type_p_IMAPIFolder)
	NEW_SWIG_INTERFACE_POINTER_OBJ(ptrMessage, lpMessage, type_p_IMessage)

	PY_CALL_METHOD(m_ptrMapiPluginManager, ((char*)lpFunctionName), PY_PARSE_TUPLE_HELPER("I|I", lpulResult, lpulDoCallexe), "OOOOO", *ptrPySession, *ptrPyAddrBook, *ptrPyStore, *ptrFolder, *ptrMessage);

exit:
	return hr;
}
