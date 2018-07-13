namespace Gdaui {
	[CCode (cheader_filename = "libgda-ui/libgda-ui.h", type_id = "gdaui_data_cell_renderer_info_get_type ()")]
	public class DataCellRendererInfo : Gtk.CellRenderer {
		public virtual signal void status_changed (string path, Gda.ValueAttribute requested_action);
	}
}