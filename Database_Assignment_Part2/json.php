<?php
require_once __DIR__ . '/../private/db_connect.php';

$nodeName = "node_1";

$sql = "
    SELECT node_name, time_received, temperature, humidity
    FROM sensor_data
    WHERE node_name = ?
    ORDER BY time_received ASC
";

$stmt = $conn->prepare($sql);
$stmt->bind_param("s", $nodeName);
$stmt->execute();

$stmt->bind_result(
    $returnedNode,
    $timeReceived,
    $temperature,
    $humidity
);

$data = [];

while ($stmt->fetch()) {
    $data[] = [
        "node_name" => $returnedNode,
        "time_received" => $timeReceived,
        "temperature" => (float) $temperature,
        "humidity" => (float) $humidity
    ];
}

$stmt->close();

header("Content-Type: application/json");

echo json_encode($data, JSON_PRETTY_PRINT);
?>