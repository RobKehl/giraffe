/*
 * Copyright 2005 - 2011  Zarafa B.V.
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

function initdistlist()
{
	// Private
	if(dhtml.getElementById("sensitivity").value == "2") {
		dhtml.setValue(dhtml.getElementById("checkbox_private"), true);
	}

	// loop through all distlist items to check if any of them must be deleted
	module.checkMissing();


}

function saveDistList()
{
	// Private
	var checkbox_private = dhtml.getElementById("checkbox_private");
	if(checkbox_private.checked) {
		dhtml.getElementById("sensitivity").value = "2";
		dhtml.getElementById("private").value = "1";
	} else {
		dhtml.getElementById("sensitivity").value = "0";
		dhtml.getElementById("private").value = "-1";
	}

	var fileas = dhtml.getElementById("fileas").value;
	if(fileas === "") {
	    alert(_('You must specify a name for the distribution list before saving it.'));
	    return;
	}
	dhtml.getElementById("dl_name").value = fileas;
	dhtml.getElementById("subject").value = fileas;
	dhtml.getElementById("display_name").value = fileas;
	module.save();
	window.close();
}

function distlist_categoriesCallback(categories){
	dhtml.getElementById("categories").value = categories;
}

function distlist_addABCallback(result) 
{
	if (result.multiple){
		for(var key in result){
			if (key!="multiple" && key!="value"){
				module.addItem(result[key]);
			}
		}
	}else{
		module.addItem(result);
	}
}

function distlist_addNewCallback(result) 
{
	var name = result.name;
	var email = result.email;

	if (name.length<1)
		name = email;

	name = name.replace("<", "");
	name = name.replace(">", "");
	
	var item = new Object;
	item["addrtype"] = "SMTP";
	item["display_name"] = name;
	item["email_address"] = email;
	item["entryid"] = false;

	module.addItem(item);
}
/**
 * Keycontrol function which saves distribution list.
 */
function eventDistListKeyCtrlSave(moduleObject, element, event, keys)
{
	switch(event.keyCombination)
	{
		case keys["save"]:
			saveDistList();
			break;
	}
}