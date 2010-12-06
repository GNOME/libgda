//
// Copyright (c) 2008 why the lucky stiff
// Copyright (c) 2010 Andrew McElroy
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
var allStretch;
var helpPages;
var chapPages;
var defaultPage;
var toot = window.location.search.substr(1)

//the main function, call to the effect object
function dumpAlert(obj) {
    props = [];
    for ( var i in obj ) {
        props.push( "" + i + ": " + obj[i] );
    }
    alert( props );
}
window.onload = function() {
	$.ajax({url: "/~irb?cmd=!INIT!IRB!", type: "GET", 
		complete: (function(r) {
				var xmlDoc=r.responseXML.documentElement;
				var cid = xmlDoc.getElementsByTagName("cid")[0].childNodes[0].nodeValue;
				window.irb.options.gdaid = cid;				
				var pt = xmlDoc.getElementsByTagName("prompt")[0].childNodes[0].nodeValue;
				var trimmed = pt.replace(/^\s+|\s+$/g, '') ;
				window.irb.options.ps = "\033[1;31m" + trimmed + "\033[m";
				window.irb.prompt();
			}), type:"xml"});

	window.irb = new MouseApp.Irb('#irb', {
        rows: 25,
        name: 'IRB',
        greeting: "Use .? to get help\n",
        ps: '',
        user: 'guest',
        host: 'tryruby',
        // original: irbUrl: '/irb',
        irbUrl: '/~irb',
        init: function () {
        },
    });
}
