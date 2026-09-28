<?php

if (!extension_loaded('pdo_lake')) {
    die('skip pdo_lake not loaded');
}

require __DIR__.'/config.inc';

try {
    lake_connect();
} catch (PDOException $e) {
    die('skip Lake server unavailable: '.$e->getMessage());
}
