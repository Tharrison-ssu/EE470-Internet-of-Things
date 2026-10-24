<?php
/*
    Base64 Sensor Data Insert

    This file:
    1. Receives a Base64-encoded message
    2. Decodes the message
    3. Converts it into normal URL parameters
    4. Sends the values to insert.php for validation and insertion
*/

// Read the encoded value from either format:
//
// Format 1:
// encoded_insert.php?data=BASE64_VALUE
//
// Format 2:
// encoded_insert.php?BASE64_VALUE

if (isset($_GET["data"])) {
    $encodedMessage = $_GET["data"];
} else {
    // Get the text after the question mark
    $encodedMessage = $_SERVER["QUERY_STRING"];
}

// Decode the Base64 message
$decodedMessage = base64_decode($encodedMessage, true);

if ($decodedMessage === false) {
    die("Error: The message was not valid Base64.");
}

// Convert the decoded message into URL parameters
parse_str($decodedMessage, $decodedValues);

// Make sure the decoded message contains the required values
if (
    !isset($decodedValues["nodeId"]) ||
    !isset($decodedValues["nodeTemp"]) ||
    !isset($decodedValues["nodeHumidity"])
) {
    die(
        "Error: The decoded message must include nodeId, "
        . "nodeTemp, and nodeHumidity."
    );
}

// Replace the current GET values with the decoded values
$_GET = $decodedValues;

// Reuse the normal insert program.
// This performs all validation and database insertion.
require_once __DIR__ . "/insert.php";
?>