import _gda_common


## Gda-Log
def gda_log_enable ():
    _gda_common.gda_log_enable()

def gda_log_disable ():
    _gda_common.gda_log_disable()

def gda_log_is_enable ():
    return _gda_common.gda_log_is_enable()

def gda_log_message (msg):
    _gda_common.gda_log_message (msg)

def gda_log_error (msg):
    _gda_common.gda_log_error (msg)


# Data sources
def gda_list_datasources():
    None

def gda_list_datasources_for_provider (provider):
    None



## Gda-Server
class gda_server:
    def __init__ (self):
        self._o = _gda_common.gda_server_new()
        
    def list (self):
        # FIXME: TODO.. :)
        None
            
def find_by_name (provider):
    return _gda_common.gda_server_find_by_name (provider)



## Gda-Dsn        
class gda_dsn:

    attrs = {
        "is_global"   : _gda_common.gda_dsn_get_is_global,
        "dsn"         : _gda_common.gda_dsn_get_dsn,
        "provider"    : _gda_common.gda_dsn_get_provider,
        "description" : _gda_common.gda_dsn_get_description,
        "username"    : _gda_common.gda_dsn_get_username,
        "config"      : _gda_common.gda_dsn_get_config,
        }
    
    def __init__ (self):
        self._o = _gda_common.gda_dsn_new()

    def __getattr__ (self, attr):
        if self.attrs.has_key (attr):
            return self.attrs[attr](self._o)
        
    def set_name (self, name):
        _gda_common.gda_dsn_set_name (self._o, name)
        
    def set_provider (self, provider):
        _gda_common.gda_dsn_set_provider (self._o, provider)
        
    def set_dsn (self, dsn):
        _gda_common.gda_dsn_set_dsn (self._o, dsn)

    def set_description (self, description):
        _gda_common.gda_dsn_set_description (self._o, description)

    def set_username (self, username):
        _gda_common.gda_dsn_set_username (self._o, username)

    def set_config (self, config):
        _gda_common.gda_dsn_set_config (self._o, config)

    def set_global (self, is_global):
        _gda_common.gda_dsn_set_global (self._o, is_global)

    def save (self):
        return _gda_common.gda_dsn_save (self._o) 

    def remove (self):
        return _gda_common.gda_dsn_remove (self._o) 

    def list (self):
        # FIXME: TODO.. :)
        None
        
def find_by_name (self, name):
    return _gda_common.gda_dsn_find_by_name (name)















