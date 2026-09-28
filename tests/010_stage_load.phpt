--TEST--
pdo_lake: stage upload, bulk loading and PUT of local files
--EXTENSIONS--
pdo_lake
--SKIPIF--
<?php require __DIR__.'/skipif.inc'; ?>
--FILE--
<?php
require __DIR__.'/config.inc';

$db = lake_connect();
$db->exec('CREATE OR REPLACE STAGE pdo_lake_stage');
$db->exec('CREATE OR REPLACE TABLE t_load (id INT, v STRING NULL)');
$count = fn () => $db->query('SELECT count(*) FROM t_load')->fetchColumn();

var_dump($db->uploadToStage('@pdo_lake_stage/raw/a.csv', "1,a\n2,b\n"));
var_dump($db->query("SELECT count($1) FROM @pdo_lake_stage/raw/a.csv (file_format => 'csv')")->fetchColumn());

$dir = sys_get_temp_dir().'/pdo_lake_'.getmypid();
@mkdir($dir);
file_put_contents($dir.'/b.csv', "3,c\n4,d\n5,e\n");
var_dump($db->uploadFileToStage('@pdo_lake_stage/raw/b.csv', $dir.'/b.csv'));
$db->exec("COPY INTO t_load FROM @pdo_lake_stage/raw/ FILE_FORMAT = (type = csv)");
var_dump($count());

$stats = $db->loadData('INSERT INTO t_load VALUES', "6,f\n7,g\n", Pdo\Lake::LOAD_STAGE);
var_dump(is_array($stats), $count());
$db->loadData('INSERT INTO t_load VALUES', "8,h\n", Pdo\Lake::LOAD_STREAMING);
var_dump($count());

$db->loadFile('INSERT INTO t_load VALUES', $dir.'/b.csv');
var_dump($count());
file_put_contents($dir.'/c.tsv', "9\ti\n");
$db->loadFile('INSERT INTO t_load VALUES', $dir.'/c.tsv', Pdo\Lake::LOAD_STAGE, ['type' => 'TSV'], ['purge' => 'true']);
var_dump($count());

$stats = $db->streamLoad('INSERT INTO t_load VALUES', [[10, "quote \" comma , newline \n"], ['11', null], [12, true]]);
var_dump($stats['write_rows']);
echo json_encode($db->query('SELECT id, v FROM t_load WHERE id >= 10 ORDER BY id')->fetchAll(PDO::FETCH_NUM)), "\n";

try {
    $db->streamLoad('INSERT INTO t_load VALUES', [[1, 'a'], [2]]);
} catch (PDOException | ValueError $e) {
    echo get_class($e), "\n";
}

file_put_contents($dir.'/d.csv', "13,m\n");
$put = $db->putFiles('file://'.$dir.'/*.csv', '@pdo_lake_stage/put/');
usort($put, fn ($a, $b) => $a['file'] <=> $b['file']);
foreach ($put as $row) {
    echo basename($row['file']), ' ', $row['status'], ' ', $row['size'], "\n";
}
echo json_encode($db->query("SELECT name FROM list_stage(location => '@pdo_lake_stage/put/') ORDER BY name")->fetchAll(PDO::FETCH_COLUMN), JSON_UNESCAPED_SLASHES), "\n";

array_map('unlink', glob($dir.'/*.*'));
rmdir($dir);
$db->exec('DROP STAGE pdo_lake_stage');
?>
--EXPECT--
bool(true)
int(2)
bool(true)
int(5)
bool(true)
int(7)
int(8)
int(11)
int(12)
int(3)
[[10,"quote \" comma , newline \n"],[11,null],[12,"1"]]
ValueError
b.csv SUCCESS 12
d.csv SUCCESS 5
["put/b.csv","put/d.csv"]
