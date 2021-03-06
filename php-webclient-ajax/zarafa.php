<?php
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

?>
<?php
	/**
	* This file is the dispatcher of the whole application, every request for data enters 
	* here. XML is received and send to the client.
	*/
	
	// Include files
	include("config.php");
	include("defaults.php");
	include("server/util.php");
	require("server/PEAR/JSON.php");
	
	require("mapi/mapi.util.php");
	require("mapi/mapicode.php");
	require("mapi/mapidefs.php");
	require("mapi/mapitags.php");
	require("mapi/mapiguid.php");

	require("server/core/class.conversion.php");
	require("server/core/class.mapisession.php");
	require("server/core/class.entryid.php");
	
	include("server/core/constants.php");
	
	include("server/core/class.state.php");
	include("server/core/class.attachmentstate.php");
	include("server/core/class.request.php");
	include("server/modules/class.module.php");
	include("server/modules/class.listmodule.php");
	include("server/modules/class.itemmodule.php");
	include("server/core/class.operations.php");
	include("server/core/class.properties.php");
	include("server/core/class.tablecolumns.php");
	include("server/core/class.bus.php");
	include("server/core/class.settings.php");
	include("server/core/class.language.php");
	include("server/core/class.pluginmanager.php");
	include("server/core/class.plugin.php");

	ob_start();
	setlocale(LC_CTYPE, "en_US.UTF-8");

	// Set timezone
	if(function_exists("date_default_timezone_set")) {
		if(defined('TIMEZONE') && TIMEZONE) {
			date_default_timezone_set(TIMEZONE);
		} else if(!ini_get('date.timezone')) {
			date_default_timezone_set('Europe/London');
		}
	}

	// Get the available modules
	$GLOBALS["availableModules"] = getAvailableModules();

	// Callback function for unserialize
	// Module objects of the previous request are stored in the session. With this
	// function they are restored to PHP objects.
	ini_set("unserialize_callback_func", "sessionModuleLoader");
	
	// Start session
	session_name(COOKIE_NAME);
	session_start();
	
	// Create global mapi object. This object is used in many other files
	$GLOBALS["mapisession"] = new MAPISession();
	// Logon, the username and password are set in the "index.php" file. So whenever
	// an user enters this file, the username and password whould be set in the $_SESSION
	// variable
	if (isset($_SESSION["username"]) && isset($_SESSION["password"])) {
		$sslcert_file = defined('SSLCERT_FILE') ? SSLCERT_FILE : null;
		$sslcert_pass = defined('SSLCERT_PASS') ? SSLCERT_PASS : null;
		$hresult = $GLOBALS["mapisession"]->logon($_SESSION["username"], $_SESSION["password"], DEFAULT_SERVER, $sslcert_file, $sslcert_pass);
	}else{
		$hresult = MAPI_E_UNCONFIGURED;
	}

	if(isset($_SESSION["lang"])) {
		$session_lang = $_SESSION["lang"];
	}else{
		$session_lang = LANG;
	}
	
	// Close the session now, so we're not blocking other clients
	session_write_close();

	// Set headers for XML
	header("Content-Type: text/xml; charset=utf-8");
	header("Expires: ".gmdate( "D, d M Y H:i:s")."GMT");
	header("Last-Modified: ".gmdate( "D, d M Y H:i:s")."GMT");
	header("Cache-Control: no-cache, must-revalidate");
	header("Pragma: no-cache");
	header("X-Zarafa: ".phpversion("mapi").(defined("SVN") ? "-".SVN:""));
		
	// Check is the user is authenticated
	if ($GLOBALS["mapisession"]->isLoggedOn()) {
		// Authenticated
		// Execute request
		
		// Instantiate Plugin Manager
		$GLOBALS['PluginManager'] = new PluginManager();
		$GLOBALS['PluginManager']->detectPlugins();
		$GLOBALS['PluginManager']->initPlugins();
		// Get the available plugin modules
		$GLOBALS["availablePluginModules"] = $GLOBALS['PluginManager']->getAvailablePluginModules();

		// Create global operations object
		$GLOBALS["operations"] = new Operations();
		// Create global properties object
		$GLOBALS["properties"] = new Properties();
		// Create global tablecolumns object
		$GLOBALS["TableColumns"] = new TableColumns();
		// Create global settings object
		$GLOBALS["settings"] = new Settings();

		// Create global language object
		$GLOBALS["language"] = new Language();
		// Set the correct language
		$GLOBALS["language"]->setLanguage($session_lang);

		// Get the state information for this subsystem
		if(isset($_GET["subsystem"]))
			$subsystem = $_GET["subsystem"];
		else
			$subsystem = "anonymous"; // Currently should never happen	

		$state = new State($subsystem);
		
		// Lock the state of this subsystem
		$state->open();
		
		// Get the bus object for this subsystem
		$bus = $state->read("bus");

		if(!$bus)
			// Create global bus object
			$bus = new Bus();
		
		// Make bus global
		$GLOBALS["bus"] = $bus;
		
		// Reset any spurious information in the bus state
		$GLOBALS["bus"]->reset();
		
		// Create new request object
		$request = new Request();
		
		// Get the XML from the client
		$xml = readXML();
		if (function_exists("dump_xml")) dump_xml($xml,"in"); // debugging
		
		// Execute the request
		$xml = $request->execute($xml);

		if (function_exists("dump_xml")) dump_xml($xml,"out"); // debugging

		// Check if we can use gzip compression
		if ((!defined("DEBUG_GZIP")||DEBUG_GZIP) && $GLOBALS["settings"]->get("global/use_gzip","true")=="true" && function_exists("gzencode") && isset($_SERVER["HTTP_ACCEPT_ENCODING"]) && strpos($_SERVER["HTTP_ACCEPT_ENCODING"], "gzip")!==false){
			// Set the correct header and compress the XML
			header("Content-Encoding: gzip");
			echo gzencode($xml);
		}else {
			echo $xml;
		}
		
		// Reset the BUS before saving to state information
		$GLOBALS["bus"]->reset();

		if(isset($GLOBALS["bus"]))
			$state->write("bus", $GLOBALS["bus"]);		

		// You can skip this as well because the lock is freed after the PHP script ends
		// anyway.
		$state->close();

	} else {
		echo "<zarafa>\n";
		echo "\t<error logon=\"false\" mapi=\"".get_mapi_error_name($hresult)."\">Logon failed</error>\n";
		echo "</zarafa>";
	}
?>
