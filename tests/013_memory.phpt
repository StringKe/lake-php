--TEST--
pdo_lake: repeated connect, query, bind, error and load cycles do not grow PHP or process memory
--EXTENSIONS--
pdo_lake
--SKIPIF--
<?php require __DIR__.'/skipif.inc'; ?>
--FILE--
<?php
require __DIR__.'/config.inc';

$db = lake_connect();
$db->exec('CREATE OR REPLACE TABLE t_mem (id INT, s STRING NULL)');
$db->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_SILENT);
$payload = str_repeat('p', 4096);

$cycle = function (int $i) use ($db, $payload): void {
    $st = $db->prepare('SELECT :p, :x AS x, number FROM numbers(50)');
    $st->bindValue('p', $payload);
    $st->bindValue('x', $i, PDO::PARAM_INT);
    $st->execute();
    $st->fetchAll(PDO::FETCH_ASSOC);
    $st->getColumnMeta(0);
    $st->getAttribute(Pdo\Lake::ATTR_QUERY_STATS);
    $db->query('SELECT * FROM pdo_lake_missing_table');
    $db->errorInfo();
    $half = $db->query('SELECT number FROM numbers(5000)');
    $half->fetch();
    $half = null;
    $db->streamLoad('INSERT INTO t_mem VALUES', [[$i, $payload], [$i, null]]);
    $db->exec('DELETE FROM t_mem');
    $db->info();
    $db->lastQueryId();
    $other = PDO::connect(lake_dsn(), lake_user(), lake_password());
    $other->query('SELECT 1')->fetchColumn();
    $other = null;
};

for ($i = 0; $i < 20; $i++) {
    $cycle($i);
}
gc_collect_cycles();
$php = memory_get_usage();
$rss = getrusage()['ru_maxrss'];

for ($i = 0; $i < 200; $i++) {
    $cycle($i);
}
gc_collect_cycles();

$php_growth = memory_get_usage() - $php;
$rss_growth_kb = (getrusage()['ru_maxrss'] - $rss) / (PHP_OS_FAMILY === 'Darwin' ? 1024 : 1);
echo $php_growth < 16 * 1024 ? "php stable\n" : "php grew {$php_growth} bytes\n";
echo $rss_growth_kb < 8 * 1024 ? "rss stable\n" : "rss grew {$rss_growth_kb} KiB\n";
?>
--EXPECT--
php stable
rss stable
