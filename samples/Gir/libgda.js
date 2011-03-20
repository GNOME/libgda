imports.gi.versions.Gda = "5.0";

const GLib = imports.gi.GLib;
const Gtk = imports.gi.Gtk;
const Gda = imports.gi.Gda;
const Lang = imports.lang;

function Demo () {
    this._init ();
}

Demo.prototype = {
    
    _init: function () {
	this.setupWindow ();
	this.setupDatabase ();
	this.selectData ();
    },

    setupDatabase: function () {
	this.connection = new Gda.Connection ({provider: Gda.Config.get_provider("SQLite"),
                                               cnc_string:"DB_DIR=.;DB_NAME=Libgda_demo"});
	this.connection.open ();

	try {
	    var dm = this.connection.execute_select_command ("select * from demo");
	} catch (e) {
	    this.connection.execute_non_select_command ("create table demo (id integer, name varchar(100))");
	}
    },

    selectData: function () {
	var dm = this.connection.execute_select_command ("select * from demo order by 1, 2");
	var iter = dm.create_iter ();

	var text = "";

	while (iter.move_next ()) {
	    var id_field = Gda.value_stringify (iter.get_value_at (0));
	    var name_field = Gda.value_stringify (iter.get_value_at (1));

	    text += id_field + "\t=>\t" + name_field + '\n';
	}

	this.text.buffer.text = text;
	this.count_label.label = "<i>" + dm.get_n_rows () + " record(s)</i>";
    },

    _clearFields: function () {
	this.id_entry.text = "";
	this.name_entry.text = ""; 
    },

    _insertClicked: function () {
	//if (!this._validateFields ())
	//return;

	var b = new Gda.SqlBuilder ({stmt_type:Gda.SqlStatementType.INSERT});
	b.set_table ("demo");
	b.add_field_value_as_gvalue ("id", this.id_entry.text);
	b.add_field_value_as_gvalue ("name", this.name_entry.text);
	var stmt = b.get_statement ();
	this.connection.statement_execute_non_select (stmt, null);

	this._clearFields ();
	this.selectData ();
    },

    setupWindow: function () {
	this.window = new Gtk.Window ({title: "Data Access Demo", height_request: 350});
	this.window.connect ("delete-event", function () { 
	    Gtk.main_quit();
	    return true;
	});

	// main box
	var main_box = new Gtk.Box ({orientation: Gtk.Orientation.VERTICAL, spacing: 5});
	this.window.add (main_box);

	// first label
	var info1 = new Gtk.Label ({label: "<b>Insert a record</b>", xalign: 0, use_markup: true});
	main_box.pack_start (info1, false, false, 5);

	// "insert a record" horizontal box
	var insert_box = new Gtk.Box ({orientation: Gtk.Orientation.HORIZONTAL, spacing: 5});
	main_box.pack_start (insert_box, false, false, 5);

	// ID field
	insert_box.pack_start (new Gtk.Label ({label: "ID:"}), false, false, 5);
	this.id_entry = new Gtk.Entry ();
	insert_box.pack_start (this.id_entry, false, false, 5);

	// Name field
	insert_box.pack_start (new Gtk.Label ({label: "Name:"}), false, false, 5);
	this.name_entry = new Gtk.Entry ({activates_default: true});
	insert_box.pack_start (this.name_entry, true, true, 5);

	// Insert button
	var insert_button = new Gtk.Button ({label: "Insert", can_default: true});
	insert_button.connect ("clicked", Lang.bind (this, this._insertClicked));
	insert_box.pack_start (insert_button, false, false, 5);
	insert_button.grab_default ();

	// Browse textview
	var info2 = new Gtk.Label ({label: "<b>Browse the table</b>", xalign: 0, use_markup: true});
	main_box.pack_start (info2, false, false, 5);
	this.text = new Gtk.TextView ({editable: false});
	var sw = new Gtk.ScrolledWindow ({shadow_type:Gtk.ShadowType.IN});
	sw.add (this.text);
	main_box.pack_start (sw, true, true, 5);

	this.count_label = new Gtk.Label ({label: "", xalign: 0, use_markup: true});
	main_box.pack_start (this.count_label, false, false, 0);

	this.window.show_all ();
    }
}


Gtk.init (0, null);

var demo = new Demo ();

Gtk.main ();
