import _gda_clnt

class gda_batch:
    def __init__ (self):
        self._o = _gda_clnt.gda_batch_new()

    def free (self):
        None

    def load_file (self, filename, clean):
        return _gda_clnt.gda_batch_load_file (self._o, filename, clean)

    def add_command (self, cmd):
        _gda_clnt.gda_batch_add_command (self._o, cmd)

    def clear (self):
        _gda_clnt.gda_batch_clear (self._o)

    def start (self):
        return _gda_clnt.gda_batch_start (self._o)

    def stop (self):
        _gda_clnt.gda_batch_stop (self._o)

    def is_running (self):
        return _gda_clnt.gda_batch_is_running (self._o)

    def get_connection (self):
        return _gda_clnt.gda_batch_get_connection (self._o)

    def set_connection (self, cnc):
        _gda_clnt.set_connection (self._o, cnc)

    def get_transaction_mode (self):
        return _gda_clnt.get_transaction_mode (self._o)

    def set_transaction_mode (self, mode):
        _gda_clnt.set_transaction_mode (self._o, mode)



class gda_command:
    def __init__ (self):
        self._o = _gda_clnt.gda_command_new()

    def get_connection (self):
        return _gda_clnt.gda_command_get_connection (self._o)

    def set_connection (self, cnc):
        return _gda_clnt.gda_command_set_connection (self._o, cnc)

    def get_text (self):
        return _gda_clnt.gda_command_get_text (self._o)

    def set_text (self, text):
        _gda_clnt.gda_command_set_text (self._o, text)

    def get_cmd_type (self):
        return _gda_clnt.gda_command_get_cmd_type (self._o)

    def set_cmd_type (self, flags):
        _gda_clnt.gda_command_set_cmd_type (self._o, flags)

    def execute (self, reccount, flags):
        return _gda_clnt.gda_command_execute (self._o, reccount, flags)

    def create_parameter (self, name, p, value):
        _gda_clnt.gda_command_create_parameter (self._o, name, p, value)

    def get_timeout (self):
        return _gda_clnt.gda_command_get_timeout (self._o)

    def set_timeout (self, timeout):
        _gda_clnt.gda_command_set_timeout (self._o, timeout)

        
class gda_connection:
    def __init__ (self):
        self._o = _warp_gda_connection_new()

class gda_xml:
    def __init__ (self):
        None

class gda_recordset:
    def __init__ (self):
        None

class gda_field:
    def __init__ (self):
        None

class gda_error:
    def __init__ (self):
        None

class gda_connection_pool:
    def __init__ (self):
        None

        











