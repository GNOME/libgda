<?php
class GdaException extends Exception {
    var $cnc_closed;
    // Constructor
    public function __construct($msg, $cnc_closed) {
        parent :: __construct($msg);
        $this->cnc_closed = $cnc_closed;
    }
}
