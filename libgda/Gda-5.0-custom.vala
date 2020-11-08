namespace Gda {
    public class ServerOperation : GLib.Object {
        [CCode (cname = "gda_server_operation_get_value_at")]
        public unowned GLib.Value? get_value_at_format (string path_format, ...);
    }
    public class DataModelArray : GLib.Object, Gda.DataModel {
        [CCode (cname = "gda_data_model_array_new_with_g_types")]
        public static Gda.DataModel new_with_g_types (int cols, ...);
    }
    public class Set : GLib.Object {
        [CCode (cname = "gda_set_new_inline")]
        public static Gda.Set new_inline (int nb, ...);
    }
    public class MetaStore : GLib.Object {
        [CCode (cname = "gda_meta_store_extract")]
        public Gda.DataModel? extract_v (Gda.MetaStore store, string select_sql, ...) throws GLib.Error;
    }
}
