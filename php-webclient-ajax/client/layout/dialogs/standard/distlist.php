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
require("client/layout/tabbar.class.php");

function getModuleName(){
	return "distlistmodule";
}

function getModuleType(){
	return "list";
}

function getDialogTitle(){
	return _("Distribution List");
}

function getIncludes(){
	return array(
			"client/layout/css/tabbar.css",
			"client/layout/css/distlist.css",
			"client/layout/js/tabbar.js",
			"client/layout/js/distlist.js",
			"client/modules/itemmodule.js",
			"client/modules/".getModuleName().".js"
		);
}

function initWindow(){
	global $tabbar, $tabs;

	$tabs = array("members" => _("Members"), "notes" => _("Notes"));
	$tabbar = new TabBar($tabs, key($tabs));
}

function getJavaScript_onload(){
	global $tabbar;
	
	$tabbar->initJavascript("tabbar", "\t\t\t\t\t"); 
?>
					var data = new Object;
					data.storeid = "<?=get("storeid", "", false, ID_REGEX)?>";
					data.parententryid = "<?=get("parententryid", "", false, ID_REGEX)?>";
					data.entryid = "<?=get("entryid", "", false, ID_REGEX)?>";
					data.has_no_menu = true; // hack
					module.init(moduleID, dhtml.getElementById("tableview"), false, data);
					module.setData(data);
					module.list();
				
					resizeBody();
					module.resize();
					
					dhtml.addEvent(false, dhtml.getElementById("fileas"), "contextmenu", forceDefaultActionEvent);
					dhtml.addEvent(false, dhtml.getElementById("html_body"), "contextmenu", forceDefaultActionEvent);
					dhtml.addEvent(module, dhtml.getElementById("categories"), "blur", eventFilterCategories);

					// Set focus on fileas field.
					dhtml.getElementById("fileas").focus();
<?php } // getJavaSctipt_onload						

function getJavaScript_onresize(){ ?>
					module.resize();
<?php } // getJavaScript_onresize	

function getBody() {
	global $tabbar, $tabs;
	
	$tabbar->createTabs();
	$tabbar->beginTab("members");
?>
	<input id="entryid" type="hidden">
	<input id="parent_entryid" type="hidden">
	<input id="message_class" type="hidden" value="IPM.DistList">
	<input id="icon_index" type="hidden" value="514">
	<input id="display_name" type="hidden">
	<input id="dl_name" type="hidden">
	<input id="sensitivity" type="hidden" value="0">
	<input id="private" type="hidden" value="-1">
	<input id="subject" type="hidden">
	
	<table border="0">
		<tr>
			<td><label for="fileas"><?=_("Name")?>:</label></td>
			<td id="fileas_container"><input type="text" id="fileas"></td>
		</tr>
	</table>
	
	<div id="distlist_actions">
		<button onclick="webclient.openModalDialog(module, 'addressbook', DIALOG_URL+'task=addressbook_modal&storeid='+module.storeid+'&type=all', 800, 500, distlist_addABCallback);"><?=_("Select Members")?>...</button>
		<button onclick="webclient.openModalDialog(-1, 'addemail', DIALOG_URL+'task=emailaddress_modal', 300,150, distlist_addNewCallback);"><?=_("Add new")?>...</button>
		<button onclick="module.removeItems();"><?=_("Remove")?>...</button>
	</div>
	
	<div id="tableview"></div>


		<div id="categoriesbar">
			<table width="100%" border="0" cellpadding="2" cellspacing="0">
				<tr>
					<td class="propertynormal propertywidth">
						<input class="button" type="button" value="<?=_("Categories")?>:" onclick="webclient.openModalDialog(module, 'categories', DIALOG_URL+'task=categories_modal', 350, 370, distlist_categoriesCallback);">
					</td>
					<td>
						<input id="categories" class="field" type="text">
					</td>
					<td width="20" nowrap>
						<label for="checkbox_private"><?=_("Private")?></label>
					</td>
					<td width="10">
						<input id="checkbox_private" type="checkbox">
					</td>
				</tr>
			</table>
		</div>

<?php 
	$tabbar->endTab();
	
	$tabbar->beginTab("notes");
?>
	<textarea id="html_body"></textarea>
<?php
	$tabbar->endTab();
} // getBody

function getMenuButtons(){
	return array(
			array(
				'id'=>"save",
				'name'=>_("Save"),
				'title'=>_("Save"),
				'callback'=>'saveDistList'
			),
		);
}

?>
