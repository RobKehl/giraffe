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

function getDialogTitle(){
	return _("Check Names"); // use gettext here
}

function getJavaScript_onload(){ ?>
					var keyword = "<?=get("keyword","", false, STRING_REGEX)?>";
					var type = "<?=get("type","", false, STRING_REGEX)?>";
					var module_id = "<?=get("module_id","", false, STRING_REGEX)?>";
					var resolveList = dhtml.getElementById("resolve_list");
					
					module = window.opener.webclient.getModule(module_id);
					
					var names = module.resolveQue[module.fieldCounter]["names"][module.keywordCounter]["items"];
					for(var i=0;i<names.length;i++){
						var fullname = names[i]["fullname"];
						var email = names[i]["emailaddress"];
						if(names[i].objecttype == MAPI_DISTLIST || names[i].message_class == "IPM.DistList"){
							fullname = "[" + fullname + "]";
						}else{
							fullname +=" <"+email+">";
						}
						var optionElement = dhtml.addElement(resolveList,"option","","",fullname);
						
						optionElement.value = i;
					}
<?php } // getJavaScript_onload						
		
function getJavaScript_onresize(){ ?>
	
<?php } // getJavaScript_onresize						

function getJavaScript_other(){ ?>
			function submitCheckNames(){
				var resolveList = dhtml.getElementById("resolve_list");
				if(!resolveList.value){
					resolveList.value = 0;
				}
				var item = module.resolveQue[module.fieldCounter]["names"][module.keywordCounter]["items"][resolveList.value];
				module.verifiedName(item["nametype"],item["nameid"],item);
			}
<?php } // getJavaScript_other
	
function getBody() { ?>
	<span><?= _("The WebAccess") ?> <?= _("found more than one") ?> 
		"<span id="search_key"><?=isset($_GET["keyword"])?htmlentities(get("keyword", false, false, STRING_REGEX)):''; ?></span>".
	</span>
	<br>
	<br>
	<span><?= _("Select the address to use") ?>:</span>
	<br>
	<select size="8" id="resolve_list">
	</select>
	<br>
	<br>
	<?=createConfirmButtons("submitCheckNames()")?>
<?php } // getBody
?>
