--TEST--
pdo_lake: driver registration, Pdo\Lake class and connection attributes
--EXTENSIONS--
pdo_lake
--SKIPIF--
<?php require __DIR__.'/skipif.inc'; ?>
--FILE--
<?php
require __DIR__.'/config.inc';

var_dump(in_array('lake', PDO::getAvailableDrivers(), true));
var_dump(get_parent_class(Pdo\Lake::class));

$db = lake_connect();
var_dump($db instanceof Pdo\Lake);

$plain = new PDO(lake_dsn(), lake_user(), lake_password());
var_dump(get_class($plain), $plain->getAttribute(PDO::ATTR_DRIVER_NAME));

var_dump($db->getAttribute(PDO::ATTR_CLIENT_VERSION));
var_dump(str_contains($db->getAttribute(PDO::ATTR_SERVER_VERSION), 'Query'));
var_dump(json_decode($db->getAttribute(PDO::ATTR_SERVER_INFO), true)['handler']);
var_dump($db->getAttribute(Pdo\Lake::ATTR_DATABASE));
var_dump(is_bool($db->getAttribute(Pdo\Lake::ATTR_SERVER_SIDE_PARAMS)));
var_dump(is_string($db->getAttribute(Pdo\Lake::ATTR_ROLE)));

$info = $db->info();
var_dump($info['user'] === lake_user(), $info['database']);

$url = PDO::connect(getenv('PDO_LAKE_TEST_URL_DSN') ?: 'lake:lake://'.lake_user().':'.lake_password().'@127.0.0.1:8000/?sslmode=disable');
var_dump($url->query('SELECT 1')->fetchColumn());

try {
    $db->lastInsertId();
} catch (PDOException $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
bool(true)
string(3) "PDO"
bool(true)
string(3) "PDO"
string(4) "lake"
string(6) "0.34.0"
bool(true)
string(7) "RestAPI"
string(13) "pdo_lake_test"
bool(true)
bool(true)
bool(true)
string(13) "pdo_lake_test"
int(1)
SQLSTATE[IM001]: Driver does not support this function: driver does not support lastInsertId()
