<?php
// Load the private database connection.
require_once __DIR__ . '/../private/db_connect.php';

// Get sensor measurements sorted by node and time.
$data_sql = "
    SELECT node_name, time_received, temperature, humidity
    FROM sensor_data
    ORDER BY node_name, time_received
";

$data_result = $conn->query($data_sql);

// Get registered sensor information.
$register_sql = "
    SELECT node_name, manufacturer, longitude, latitude
    FROM sensor_register
    ORDER BY node_name
";

$register_result = $conn->query($register_sql);

// Calculate averages for node_1.
$average_sql = "
    SELECT
        AVG(temperature) AS average_temperature,
        AVG(humidity) AS average_humidity
    FROM sensor_data
    WHERE node_name = 'node_1'
";

$average_result = $conn->query($average_sql);
$average = $average_result->fetch_assoc();
?>

<!DOCTYPE html>
<html>
<head>
    <title>Sensor Database</title>

    <style>
        body {
            font-family: Arial, sans-serif;
            background-color: #eeeeee;
            margin: 30px;
        }

        h1, h2 {
            color: #222222;
        }

        table {
            width: 100%;
            border-collapse: collapse;
            background-color: white;
            margin-bottom: 30px;
        }

        th {
            background-color: #1769aa;
            color: white;
        }

        th, td {
            border: 1px solid #777777;
            padding: 8px;
            text-align: center;
        }

        tr:nth-child(even) {
            background-color: #dddddd;
        }

        .average-box {
            background-color: white;
            border: 1px solid #777777;
            padding: 15px;
            margin-bottom: 30px;
        }
    </style>
</head>

<body>

<h1>Sensor Database</h1>

<div class="average-box">
    <h2>Node 1 Averages</h2>

    <p>
        Average Temperature:
        <?php echo number_format($average["average_temperature"], 2); ?>
        °C
    </p>

    <p>
        Average Humidity:
        <?php echo number_format($average["average_humidity"], 2); ?>
        %
    </p>
</div>

<h2>Sensor Data</h2>

<table>
    <tr>
        <th>Node Name</th>
        <th>Time Received</th>
        <th>Temperature</th>
        <th>Humidity</th>
    </tr>

    <?php while ($row = $data_result->fetch_assoc()): ?>
        <tr>
            <td><?php echo htmlspecialchars($row["node_name"]); ?></td>
            <td><?php echo htmlspecialchars($row["time_received"]); ?></td>
            <td><?php echo htmlspecialchars($row["temperature"]); ?></td>
            <td><?php echo htmlspecialchars($row["humidity"]); ?></td>
        </tr>
    <?php endwhile; ?>
</table>

<h2>Registered Sensors</h2>

<table>
    <tr>
        <th>Node Name</th>
        <th>Manufacturer</th>
        <th>Longitude</th>
        <th>Latitude</th>
    </tr>

    <?php while ($row = $register_result->fetch_assoc()): ?>
        <tr>
            <td><?php echo htmlspecialchars($row["node_name"]); ?></td>
            <td><?php echo htmlspecialchars($row["manufacturer"]); ?></td>
            <td><?php echo htmlspecialchars($row["longitude"]); ?></td>
            <td><?php echo htmlspecialchars($row["latitude"]); ?></td>
        </tr>
    <?php endwhile; ?>
</table>

</body>
</html>

<?php
$conn->close();
?>
