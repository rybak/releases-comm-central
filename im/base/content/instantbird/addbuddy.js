/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Instantbird messenging client, released
 * 2007.
 *
 * The Initial Developer of the Original Code is
 * Florian QUEZE <florian@instantbird.org>.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


var addBuddy = {
  onload: function ab_onload() {
    this.pcs = Components.classes["@instantbird.org/purple/core;1"]
                         .getService(Ci.purpleICoreService);
    this.buildAccountList();
    this.buildTagList();
  },

  buildAccountList: function ab_buildAccountList() {
    var accountList = document.getElementById("accountlist");
    for (let acc in this.getAccounts()) {
      if (!acc.connected)
        continue;
      var proto = acc.protocol;
      var item = accountList.appendItem(acc.name, acc.id, proto.name);
      item.setAttribute("image", "chrome://instantbird/skin/prpl/" + proto.id + ".png");
      item.setAttribute("class", "menuitem-iconic");
    }
    if (!accountList.itemCount) {
      document.getElementById("addBuddyDialog").cancelDialog();
      throw "No connected account!";
    }
    accountList.selectedIndex = 0;
  },

  buildTagList: function ab_buildTagList() {
    var tagList = document.getElementById("taglist");
    for (let tag in this.getTags())
      tagList.appendItem(tag.name, tag.id);
    tagList.selectedIndex = 0;
  },

  getValue: function ab_getValue(aId) {
    var elt = document.getElementById(aId);
    return elt.value;
  },

  create: function ab_create() {
    var account = this.pcs.getAccountById(this.getValue("accountlist"));
    var tag = this.pcs.getTagById(this.getValue("taglist"));
    var name = this.getValue("name")

    // For now this will allow to join an IRC chan. It should be removed later
    if (name[0] == "#" && account.protocol.id == "prpl-irc") {
      var conv = account.joinChat(name);
      if (!conv)
        return;
      var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                         .getService(Components.interfaces.nsIWindowMediator);
      var convWindow = wm.getMostRecentWindow("Messenger:convs");
      if (convWindow) {
        convWindow.msgObserver.focusConv(conv);
        convWindow.focus();
      }
    }
    else
      this.pcs.addBuddy(account, tag, name);
  },

  getAccounts: function ab_getAccounts() {
    return getIter(this.pcs.getAccounts, Ci.purpleIAccount);
  },
  getTags: function ab_getTags() {
    var DBConn = this.pcs.storageConnection;
    var statement = DBConn.createStatement("SELECT id, name FROM tags");
    while (statement.executeStep())
      yield { id: statement.getInt32(0),
              name: statement.getUTF8String(1) };
  }
};
