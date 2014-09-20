function refreshCncList() {
$.ajax({
	url: "~cnclist",
	cache: false,
	success: function(html){
			$("#cnclist").replaceWith(html);
			setTimeout("refreshCncList()",10000);
		}
	});
}

$(document).ready(function(){		
		setTimeout("refreshCncList()",10000);
});
