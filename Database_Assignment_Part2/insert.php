<?php
// Prevent MySQL errors from becoming HTTP 500 errors.
mysqli_report(MYSQLI_REPORT_OFF);

// Load the private database connection.
require_once __DIR__ . '/../private/db_connect.php';

// Read values from the URL.
$node = $_GET["nodeId"] ?? "";
$temperature = $_GET["nodeTemp"] ?? "";
$humidity = $_GET["nodeHumidity"] ?? "";
$time = $_GET["timeReceived"] ?? "";

// If the page is opened without values, show valid formatting instructions.
if ($node == "" || $temperature == "" || $humidity == "") {
    ?>
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">

        <title>Sensor Data Insert</title>

        <style>
            body {
                font-family: Arial, sans-serif;
                margin: 30px;
                background-color: #f4f5f7;
                color: #222;
            }

            .box {
                max-width: 850px;
                background-color: white;
                padding: 25px;
                border-radius: 10px;
                border-left: 5px solid #10776f;
                box-shadow: 0 2px 6px rgba(0, 0, 0, 0.1);
            }

            h1 {
                color: #10776f;
            }

            h2 {
                color: #555;
                margin-top: 25px;
            }

            code {
                display: block;
                background-color: #eeeeee;
                padding: 15px;
                margin: 12px 0;
                word-wrap: break-word;
                border-radius: 5px;
            }

            li {
                margin: 10px 0;
            }

            a {
                color: #007e87;
            }
        </style>
    </head>

    <body>
        <div class="box">
            <h1>Sensor Data Insert Endpoint</h1>

            <p>
                Use the following format to insert sensor data:
            </p>

            <code>
                https://taineengineering.com/insert.php?nodeId=node_4&nodeTemp=27&nodeHumidity=55&timeReceived=2022-10-01%2015:00:00
            </code>

            <h2>Required URL Parameters</h2>

            <ul>
                <li>
                    <strong>nodeId</strong> -
                    Registered sensor node, such as node_1
                </li>

                <li>
                    <strong>nodeTemp</strong> -
                    Temperature between -10 and 100 °C
                </li>

                <li>
                    <strong>nodeHumidity</strong> -
                    Humidity between 0 and 100 percent
                </li>

                <li>
                    <strong>timeReceived</strong> -
                    Optional time using YYYY-MM-DD HH:MM:SS format
                </li>
            </ul>

            <h2>Example Using curl</h2>

            <code>
                curl "https://taineengineering.com/insert.php?nodeId=node_4&nodeTemp=27&nodeHumidity=55&timeReceived=2022-10-01%2015:00:00"
            </code>

            <h2>Example Without a Time</h2>

            <code>
                curl "https://taineengineering.com/insert.php?nodeId=node_4&nodeTemp=27&nodeHumidity=55"
            </code>

            <p>
                Opening this page without parameters only displays these
                instructions. It does not insert any data.
            </p>
        </div>
    </body>
    </html>
    <?php
    exit;
}

// Make sure the temperature and humidity are numbers.
if (!is_numeric($temperature) || !is_numeric($humidity)) {
    die("Temperature and humidity must be numeric.");
}

$temperature = (float) $temperature;
$humidity = (float) $humidity;

// Check the allowed temperature range.
if ($temperature < -10 || $temperature > 100) {
    die("Temperature must be between -10 and 100 degrees Celsius.");
}

// Check the allowed humidity range.
if ($humidity < 0 || $humidity > 100) {
    die("Humidity must be between 0 and 100 percent.");
}

// Check whether the sensor node is registered.
$check_sql = "
    SELECT node_name
    FROM sensor_register
    WHERE node_name = ?
";

$check_statement = $conn->prepare($check_sql);

if (!$check_statement) {
    die("Could not prepare node check: " . $conn->error);
}

$check_statement->bind_param("s", $node);
$check_statement->execute();
$check_statement->store_result();

if ($check_statement->num_rows == 0) {
    $check_statement->close();
    $conn->close();

    die("Rejected: this sensor node is not registered.");
}

$check_statement->close();

// If no time was provided, the database adds the current time automatically.
if ($time == "") {
    $insert_sql = "
        INSERT INTO sensor_data
            (node_name, temperature, humidity)
        VALUES (?, ?, ?)
    ";

    $insert_statement = $conn->prepare($insert_sql);

    if (!$insert_statement) {
        die("Could not prepare insert: " . $conn->error);
    }

    $insert_statement->bind_param(
        "sdd",
        $node,
        $temperature,
        $humidity
    );
} else {
    // Verify the required time format.
    $date = DateTime::createFromFormat("Y-m-d H:i:s", $time);

    if (!$date || $date->format("Y-m-d H:i:s") !== $time) {
        die("Time must use this format: YYYY-MM-DD HH:MM:SS");
    }

    $insert_sql = "
        INSERT INTO sensor_data
            (node_name, time_received, temperature, humidity)
        VALUES (?, ?, ?, ?)
    ";

    $insert_statement = $conn->prepare($insert_sql);

    if (!$insert_statement) {
        die("Could not prepare insert: " . $conn->error);
    }

    $insert_statement->bind_param(
        "ssdd",
        $node,
        $time,
        $temperature,
        $humidity
    );
}

// Try to insert the data.
if ($insert_statement->execute()) {
    echo "<h1>Data inserted successfully</h1>";
    echo "<p>Node: "
        . htmlspecialchars($node)
        . "</p>";

    echo "<p>Temperature: "
        . htmlspecialchars((string) $temperature)
        . " °C</p>";

    echo "<p>Humidity: "
        . htmlspecialchars((string) $humidity)
        . " %</p>";

    if ($time == "") {
        echo "<p>The database automatically added the time.</p>";
    } else {
        echo "<p>Time: "
            . htmlspecialchars($time)
            . "</p>";
    }
} else {
    // Error 1062 means duplicate primary-key values.
    if ($insert_statement->errno == 1062) {
        echo "<h1>Rejected</h1>";
        echo "<p>This node already has a reading at that exact time.</p>";
    } else {
        echo "<h1>Insert failed</h1>";
        echo "<p>Error: "
            . htmlspecialchars($insert_statement->error)
            . "</p>";
    }
}

// Close the statement and connection.
$insert_statement->close();
$conn->close();
?>