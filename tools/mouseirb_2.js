//
// Copyright (c) 2008 why the lucky stiff
// 
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge,
// publish, distribute, sublicense, and/or sell copies of the Software,
// and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
// ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
// TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
// PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
// SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
// OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

/* Irb running moush */
MouseApp.Irb = function(element, options) {
  this.element = $(element);
  this.setOptions(options);
  this.showHelp = this.options.showHelp;
  if ( this.options.showChapter ) {
      this.showChapter = this.options.showChapter;
  }
  if ( this.options.init ) {
      this.init = this.options.init;
  }
  this.initWindow();
  this.setup();
  this.helpPage = null;
  this.irbInit = false;
};

$.extend(MouseApp.Irb.prototype, MouseApp.Terminal.prototype, {
    cmdToQuery: function(cmd) {
        return "cmd=" + escape(cmd.replace(/&lt;/g, '<').replace(/&gt;/g, '>').
            replace(/&amp;/g, '&').replace(/\r?\n/g, "\n")).replace(/\+/g, "%2B") +
	    "&cid=" + this.options.gdaid;
    },

    fireOffCmd: function(cmd, func) {
      var irb = this;
          $.ajax({url: this.options.irbUrl + "?" + this.cmdToQuery(cmd), type: "GET", 
            complete: func});
    },

    reply: function(str) {
        var raw = str.replace(/\033\[(\d);(\d+)m/g, '');
        this.checkAnswer(raw);
        if (!str.match(/^(\.\.)+$/)) {
            if ( str[str.length - 1] != "\n" ) {
                str += "\n";
            }
            js_payload = /\033\[1;JSm(.*)\033\[m/;
            js_in = str.match(js_payload);
            if (js_in) {
                try {
                    js_in = eval(js_in[1]);
                } catch (e) {}
                str = str.replace(js_payload, '');
            }
            var pr_re = new RegExp("(^|\\n)=>");
            if ( str.match( pr_re ) ) {
              str = str.replace(new RegExp("(^|\\n)=>"), "$1\033[1;34m=>\033[m");
            } else {
              str = str.replace(new RegExp("(^|\\n)= (.+?) ="), "$1\033[1;33m$2\033[m");
            }
	    if (str.search(/^</) != -1) {
		    var irbdiv = $("#irb");
		    $("#irb div:last-child").hide();
		    var table = $("<div class=\"tcontents\">"+str+"</div>");
		    table.children(":first").children(":first").attr("class","ctable");
		    var folded = $("<div></div>");
		    var nbrows = table.children(":first").children(":last");
		    nbrows.appendTo(folded);
		    nbrows.css("margin","0");
		    irbdiv.append(table);
		    table.dblclick(function() {table.hide(); folded.show();});
		    
		    irbdiv.append(folded);
		    folded.hide();
		    folded.dblclick(function() {table.show(); folded.hide();});

		    this.cursorOff();
		    this.advanceLine();
		    this.cursorOn();
	    }
            else
            this.write(str);
            this.prompt();
        } else {
            this.prompt("\033[1;32m" + ".." + "\033[m", true);
            this.puts(str.replace(/\./g, ' '), 0);
        }
    },

    setHelpPage: function(n, page) {
        if (this.helpPage)
          $(this.helpPage.ele).hide('fast');
        this.helpPage = {index: n, ele: page};
        match = this.scanHelpPageFor('load');
        if (match != -1)
        {
          this.fireOffCmd(match, (function(r) {
            $(page).show('fast');
          }));
        }
        else
        {
          $(page).show('fast');
        }
    },

    scanHelpPageFor: function(eleClass) {
        match = $("div." + eleClass, this.helpPage.ele);
        if ( match[0] ) return match[0].innerHTML;
        else            return -1;
    },

    checkAnswer: function(str) {
        if ( this.helpPage ) {
            match = this.scanHelpPageFor('answer');
            if ( match != -1 ) {
                if ( str.match( new RegExp('^\s*=> ' + match + '\s*$', 'm') ) ) {
                    this.showHelp(this.helpPage.index + 1);
                }
            } else {
                match = this.scanHelpPageFor('stdout');
                if ( match != -1 ) {
                    if ( match == '' ) {
                        if ( str == '' || str == null ) this.showHelp(this.helpPage.index + 1);
                    } else if ( str.match( new RegExp('^\s*' + match + '$', 'm') ) ) {
                        this.showHelp(this.helpPage.index + 1);
                    }
                }
            }
        }
    },

    onKeyCtrld: function() {
        this.clearCommand();
        this.puts("reset");
        this.onKeyEnter();
    },

    onKeyEnter: function() {
        this.typingOff();
        var cmd = this.getCommand();
        if (cmd) {
            this.history[this.historyNum] = cmd;
            this.backupNum = ++this.historyNum;
        }
        this.commandNum++;
        this.advanceLine();
        if (cmd) {
            if ( cmd == "clear" ) {
                this.clear();
                this.prompt();
            } else if ( cmd.match(/^(back)$/) ) {
                if (this.helpPage && this.helpPage.index >= 1) {
                    this.showHelp(this.helpPage.index - 1);
                }
                this.prompt();
            } else if ( cmd.match(/^(next)$/) ) {
                if (this.helpPage) {
                    this.showHelp(this.helpPage.index + 1);
                }
                this.prompt();
            } else if ( cmd.match(/^(help|wtf\?*)$/) ) {
                this.showHelp(1);
                this.prompt();
            } else if ( regs = cmd.match(/^(help|wtf\?*)\s+#?(\d+)\s*$/) ) {
                this.showChapter(parseInt(regs[2]));
                this.prompt();
            } else {
                var term = this;
                this.fireOffCmd(cmd, (function(r) {
					//term.reply(r.responseText ? r.responseText : ''); 
					var xmlDoc=r.responseXML.documentElement;
					var txt;
					if (xmlDoc.getElementsByTagName("cmde")[0].childNodes[0]) {
						// it appears Firefox and some others split long text nodes into several
						// smaller chunks of 4kb, so we need to get all of them.
						txt="";
						for (i = 0; i < xmlDoc.getElementsByTagName("cmde")[0].childNodes.length; i++)
							txt += xmlDoc.getElementsByTagName("cmde")[0].childNodes[i].nodeValue
					}
 					var pt = xmlDoc.getElementsByTagName("prompt")[0].childNodes[0].nodeValue;
 					term.reply(txt ? txt: '', pt ? pt : null);
				}));
            }
        } else {
            this.prompt();
        }
    }
});

