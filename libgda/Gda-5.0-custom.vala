namespace Gda {
    public class ServerOperation : GLib.Object {
	    [Version (since = "4.2.3")]
        public static Gda.ServerOperation? prepare_create_table (Gda.Connection cnc, string table_name, ...) throws GLib.Error;
    }
}
