--TEST--
pdo_lake: presigned URLs and GET of stage files on storage that supports presigning
--EXTENSIONS--
pdo_lake
--SKIPIF--
<?php
require __DIR__.'/skipif.inc';
require_once __DIR__.'/config.inc';
$db = lake_connect();
$db->exec('CREATE STAGE IF NOT EXISTS pdo_lake_presign');
try {
    $db->presign('DOWNLOAD', '@pdo_lake_presign/probe.csv');
} catch (PDOException $e) {
    die('skip stage storage cannot presign: '.$e->errorInfo[2]);
}
?>
--FILE--
<?php
require __DIR__.'/config.inc';

$db = lake_connect();
$db->exec('CREATE OR REPLACE STAGE pdo_lake_presign');

$upload = $db->presign('upload', '@pdo_lake_presign/p.csv');
var_dump(array_keys($upload), $upload['method'], is_array($upload['headers']), str_starts_with($upload['url'], 'http'));

$db->uploadToStage('@pdo_lake_presign/get/g.csv', "1,x\n");
$dir = sys_get_temp_dir().'/pdo_lake_get_'.getmypid();
@mkdir($dir);
$got = $db->getFiles('@pdo_lake_presign/get/', 'file://'.$dir.'/');
var_dump(count($got), $got[0]['status'], file_get_contents($dir.'/g.csv'));

try {
    $db->presign('DELETE', '@pdo_lake_presign/p.csv');
} catch (PDOException $e) {
    echo $e->errorInfo[0], "\n";
}

array_map('unlink', glob($dir.'/*'));
rmdir($dir);
$db->exec('DROP STAGE pdo_lake_presign');
?>
--EXPECT--
array(3) {
  [0]=>
  string(6) "method"
  [1]=>
  string(7) "headers"
  [2]=>
  string(3) "url"
}
string(3) "PUT"
bool(true)
bool(true)
int(1)
string(7) "SUCCESS"
string(4) "1,x
"
HY000
