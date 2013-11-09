#!/usr/bin/php
<?
$fp = file("output");

$min = 10000;
$max = 0;

foreach ($fp as $point) {
	if ($point < $min) $min = $point;
	if ($point > $max) $max = $point;
}

echo "Min: $min\n";
echo "Max: $max\n\n";

?>
