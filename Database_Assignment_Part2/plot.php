<?php
/*
    Sensor graph with node selection
*/

// Use your private database connection
require_once __DIR__ . '/../private/db_connect.php';

// Get all registered node names for the dropdown menu
$nodeOptions = [];

$nodeQuery = "
    SELECT node_name
    FROM sensor_register
    ORDER BY node_name
";

$nodeResult = $conn->query($nodeQuery);

while ($row = $nodeResult->fetch_assoc()) {
    $nodeOptions[] = $row["node_name"];
}

// Select node_1 by default
$selectedNode = $_GET["node"] ?? "node_1";

// Only allow registered nodes to be selected
if (!in_array($selectedNode, $nodeOptions)) {
    $selectedNode = "node_1";
}

// Retrieve data for the selected node
$sql = "
    SELECT time_received, temperature, humidity
    FROM sensor_data
    WHERE node_name = ?
    ORDER BY time_received ASC
";

$stmt = $conn->prepare($sql);
$stmt->bind_param("s", $selectedNode);
$stmt->execute();

// bind_result works reliably on Hostinger
$stmt->bind_result($timeReceived, $temperature, $humidity);

$times = [];
$temperatures = [];
$humidities = [];

while ($stmt->fetch()) {
    $times[] = $timeReceived;
    $temperatures[] = (float) $temperature;
    $humidities[] = (float) $humidity;
}

$stmt->close();

// Calculate statistics
$averageTemperature = count($temperatures) > 0
    ? array_sum($temperatures) / count($temperatures)
    : 0;

$averageHumidity = count($humidities) > 0
    ? array_sum($humidities) / count($humidities)
    : 0;

$maximumTemperature = count($temperatures) > 0
    ? max($temperatures)
    : 0;

$minimumHumidity = count($humidities) > 0
    ? min($humidities)
    : 0;

// Convert PHP arrays to JavaScript arrays
$timesJson = json_encode($times);
$temperaturesJson = json_encode($temperatures);
?>

<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">

    <title>Sensor Graph</title>

    <!-- Chart.js library -->
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>

    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 30px;
            background-color: #f4f6f8;
            color: #222;
        }

        h1 {
            color: #1f4e79;
        }

        select {
            padding: 8px;
            font-size: 16px;
            margin-bottom: 20px;
        }

        .stats {
            display: flex;
            flex-wrap: wrap;
            gap: 15px;
            margin-bottom: 25px;
        }

        .stat-box {
            background-color: white;
            border: 1px solid #cccccc;
            border-radius: 8px;
            padding: 15px;
            min-width: 190px;
            box-shadow: 0 2px 5px rgba(0, 0, 0, 0.1);
        }

        .stat-title {
            font-weight: bold;
            color: #555555;
        }

        .stat-value {
            font-size: 20px;
            margin-top: 8px;
            color: #1f4e79;
        }

        .chart-container {
            background-color: white;
            padding: 20px;
            max-width: 1000px;
            height: 500px;
            border-radius: 8px;
            box-shadow: 0 2px 5px rgba(0, 0, 0, 0.1);
        }
    </style>
</head>

<body>

    <h1>
        Sensor Node <?php echo htmlspecialchars($selectedNode); ?>
    </h1>

    <!-- Node selection dropdown -->
    <form method="GET">
        <label for="node">Select a sensor node:</label>

        <select name="node" id="node" onchange="this.form.submit()">
            <?php foreach ($nodeOptions as $node): ?>
                <option
                    value="<?php echo htmlspecialchars($node); ?>"
                    <?php if ($node === $selectedNode) echo "selected"; ?>
                >
                    <?php echo htmlspecialchars($node); ?>
                </option>
            <?php endforeach; ?>
        </select>
    </form>

    <!-- Required statistics -->
    <div class="stats">

        <div class="stat-box">
            <div class="stat-title">Average Temperature</div>
            <div class="stat-value">
                <?php echo number_format($averageTemperature, 2); ?> °C
            </div>
        </div>

        <div class="stat-box">
            <div class="stat-title">Average Humidity</div>
            <div class="stat-value">
                <?php echo number_format($averageHumidity, 2); ?> %
            </div>
        </div>

        <div class="stat-box">
            <div class="stat-title">Maximum Temperature</div>
            <div class="stat-value">
                <?php echo number_format($maximumTemperature, 2); ?> °C
            </div>
        </div>

        <div class="stat-box">
            <div class="stat-title">Minimum Humidity</div>
            <div class="stat-value">
                <?php echo number_format($minimumHumidity, 2); ?> %
            </div>
        </div>

    </div>

    <!-- Graph -->
    <div class="chart-container">
        <canvas id="temperatureChart"></canvas>
    </div>

    <script>
        // Data received from PHP
        const timeLabels = <?php echo $timesJson; ?>;
        const temperatureValues = <?php echo $temperaturesJson; ?>;

        // Create the green line graph
        const chartArea = document.getElementById("temperatureChart");

        new Chart(chartArea, {
            type: "line",

            data: {
                labels: timeLabels,

                datasets: [{
                    label: "Temperature (°C)",
                    data: temperatureValues,

                    borderColor: "green",
                    backgroundColor: "rgba(0, 128, 0, 0.20)",
                    pointBackgroundColor: "green",
                    pointRadius: 5,
                    borderWidth: 3,
                    tension: 0.2
                }]
            },

            options: {
                responsive: true,
                maintainAspectRatio: false,

                scales: {
                    x: {
                        title: {
                            display: true,
                            text: "Time"
                        }
                    },

                    y: {
                        title: {
                            display: true,
                            text: "Temperature (°C)"
                        }
                    }
                },

                plugins: {
                    title: {
                        display: true,
                        text: "Sensor Node <?php echo htmlspecialchars($selectedNode); ?> Temperature"
                    }
                }
            }
        });
    </script>

</body>
</html>