--TEST--
pdo_lake: result type mapping and column metadata
--EXTENSIONS--
pdo_lake
--SKIPIF--
<?php require __DIR__.'/skipif.inc'; ?>
--FILE--
<?php
require __DIR__.'/config.inc';

$db = lake_connect();
$st = $db->query(<<<'SQL'
SELECT -2::INT8 a, 9223372036854775807::INT64 b, 18446744073709551615::UINT64 c, 1.25::FLOAT d, 2.5::DOUBLE e,
       123.450::DECIMAL(10,3) f, 12345678901234567890.123456789::DECIMAL(38,9) g, true h, 'txt' i, FROM_HEX('00ff41') j,
       '2026-09-28'::DATE k, '2026-09-28 10:11:12.123456'::TIMESTAMP l, [1,2,3] m, {'a':1} n, (1,'x') o,
       parse_json('{"k":[1,2]}') p, NULL::INT NULL q
SQL);
foreach ($st->fetch(PDO::FETCH_ASSOC) as $name => $value) {
    echo $name, ' ', get_debug_type($value), ' ', is_string($value) ? bin2hex($value) : var_export($value, true), "\n";
}

$st = $db->query("SELECT 123.450::DECIMAL(10,3) dec, NULL::INT NULL n, FROM_HEX('00') bin, true flag, 1::INT8 small");
for ($i = 0; $i < $st->columnCount(); $i++) {
    $meta = $st->getColumnMeta($i);
    printf("%s %s pdo_type=%d precision=%d scale=%s flags=%s\n", $meta['name'], $meta['native_type'], $meta['pdo_type'], $meta['precision'], $meta['scale'] ?? '-', implode(',', $meta['flags']));
}

$db->setAttribute(PDO::ATTR_STRINGIFY_FETCHES, true);
var_dump($db->query('SELECT 5::INT, 1.5::DOUBLE, false')->fetch(PDO::FETCH_NUM));
?>
--EXPECT--
a int -2
b int 9223372036854775807
c string 3138343436373434303733373039353531363135
d float 1.25
e float 2.5
f string 3132332e343530
g string 31323334353637383930313233343536373839302e313233343536373839
h bool true
i string 747874
j string 00ff41
k string 323032362d30392d3238
l string 323032362d30392d32382031303a31313a31322e313233343536
m string 5b312c322c335d
n string 7b2261223a317d
o string 5b312c2278225d
p string 7b226b223a5b312c325d7d
q null NULL
dec Decimal(10, 3) pdo_type=2 precision=10 scale=3 flags=not_null
n Nullable(Int32) pdo_type=1 precision=0 scale=- flags=
bin Binary pdo_type=3 precision=0 scale=- flags=not_null
flag Boolean pdo_type=5 precision=0 scale=- flags=not_null
small Int8 pdo_type=1 precision=0 scale=- flags=not_null
array(3) {
  [0]=>
  string(1) "5"
  [1]=>
  string(3) "1.5"
  [2]=>
  string(1) "0"
}
