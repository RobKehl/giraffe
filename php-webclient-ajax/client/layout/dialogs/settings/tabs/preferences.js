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

function preferences_loadSettings(settings){
	var field;
	var value;

	// startup folder
	value = settings.get("global/startup/folder","inbox");
	field = dhtml.getElementById("preferences_startupfolder");
	field.value = value;

	// row count
	value = settings.get("global/rowcount", 50);
	field = dhtml.getElementById("preferences_rowcount");
	for(var i = 0; i<field.length; i++){
		if (field.options[i].value == value){
			field.options[i].selected = true;
		}
	}

	// language
	value = settings.get("global/language", "en_EN");
	field = dhtml.getElementById("preferences_language");
	for(var i = 0; i<field.length; i++){
		if (field.options[i].value == value){
			field.options[i].selected = true;
		}
	}
	
	// theme
	value = settings.get("global/theme_color", THEME_COLOR);
	field = dhtml.getElementById("preferences_theme");
	for(var i = 0; i<field.length; i++){
		if (field.options[i].value == value){
			field.options[i].selected = true;
		}
	}

	// mail check timeout
	value = settings.get("global/mail_check_timeout","default");
	field = dhtml.getElementById("preferences_mailcheck");
	for(var i = 0; i<field.length; i++){
		if (field.options[i].value == value){
			field.options[i].selected = true;
		}
	}

	// mail read flag timer
	value = settings.get("global/mail_readflagtime", "0");
	field = dhtml.getElementById("preferences_mail_readflagtimer");
	field.value = value;

	// automatic logout
	value = settings.get("global/auto_logout","0");
	field = dhtml.getElementById("preferences_autologout");
	for(var i = 0; i<field.length; i++){
		if (field.options[i].value == value){
			field.options[i].selected = true;
		}
	}

	// previewpane
	value = settings.get("global/previewpane","right");
	field = dhtml.getElementById("preferences_previewpane");
	field.value = value;

	// read receipt handling
	value = settings.get("global/readreceipt_handling","brak");
	switch(value){
		case "always":
			field = dhtml.getElementById("preferences_readreceipt_always");
			break;
		case "never":
			field = dhtml.getElementById("preferences_readreceipt_never");
			break;
		case "ask":
		default:
			field = dhtml.getElementById("preferences_readreceipt_ask");
			break;
	}
	field.checked = true;

	// advanced find refresh frequency
	value = settings.get("advancedfind/refresh_time", 0);
	field = dhtml.getElementById("preferences_refresh_searchresults");
	field.value = value;
}
function preferences_saveSettings(settings){
	var field;
	var old_value;

	// startup folder
	field = dhtml.getElementById("preferences_startupfolder");
	settings.set("global/startup/folder",field.value);

	if (settings.get("global/startup/folder","inbox")=="today"){	
		settings.set("global/startup/folder_lastopened", parentWebclient.hierarchy.defaultstore.subtree_entryid);
	}
	
	// row count
	old_value = settings.get("global/rowcount", 50);
	field = dhtml.getElementById("preferences_rowcount");
	settings.set("global/rowcount",field.options[field.selectedIndex].value);
	if(old_value != field.options[field.selectedIndex].value) {
		reloadNeeded = true;
	}

	// language (check if changed => reload)
	old_value = settings.get("global/language", "en_EN");
	field = dhtml.getElementById("preferences_language");
	settings.set("global/language",field.options[field.selectedIndex].value);
	if (old_value != field.options[field.selectedIndex].value){
		reloadNeeded = true;
	}

	// theme (check if changed => reload)
	old_value = settings.get("global/theme_color", "white");
	field = dhtml.getElementById("preferences_theme");
	settings.set("global/theme_color",field.options[field.selectedIndex].value);
	if (old_value != field.options[field.selectedIndex].value){
		reloadNeeded = true;
	}
	
/*	
	// compression
	field = dhtml.getElementById("preferences_gzip");
	settings.set("global/use_gzip",field.checked?"true":"false");
*/

	// mail check timeout
	field = dhtml.getElementById("preferences_mailcheck");
	if (field.value != "default"){
		settings.set("global/mail_check_timeout",field.options[field.selectedIndex].value);
	}else{
		settings.deleteSetting("global/mail_check_timeout");
	}

	// mail read flag timer
	old_value = settings.get("global/mail_readflagtime", "0");
	field = dhtml.getElementById("preferences_mail_readflagtimer");
	settings.set("global/mail_readflagtime", field.value);
	if(old_value != field.value) {
		reloadNeeded = true;
	}

	// auto logout
	old_value = settings.get("global/auto_logout", "0");
	field = dhtml.getElementById("preferences_autologout");
	if (field.value != "0"){
		settings.set("global/auto_logout",field.options[field.selectedIndex].value);
	}else{
		settings.deleteSetting("global/auto_logout");
	}
	if (old_value != field.options[field.selectedIndex].value){
		reloadNeeded = true;
	}

	// previewpane
	old_value = settings.get("global/previewpane", "right");
	field = dhtml.getElementById("preferences_previewpane");
	settings.set("global/previewpane",field.options[field.selectedIndex].value);
	if (old_value != field.options[field.selectedIndex].value){
		reloadNeeded = true;

		// Remove all other previewpane settings
		var folderData = settings.get('folders');
		for(var i in folderData){
			settings.deleteSetting('folders/'+i+'/previewpane');
		}
	}

	// read receipt handling
	field = document.getElementsByName("preferences_readreceipt");
	for(var i=0;i<field.length;i++){
		if (field[i].checked == true){
			settings.set("global/readreceipt_handling", field[i].id.substring(field[i].id.lastIndexOf("_")+1));
		}
	}

	// advanced find refresh results
	field = dhtml.getElementById("preferences_refresh_searchresults");
	settings.set("advancedfind/refresh_time", field.value);
}
