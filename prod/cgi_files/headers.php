<?php
echo "<html><body>";
echo "<h1>CGI & POST Data Test</h1>";

if (isset($_GET['name'])) {
    echo "<p>Query parameter 'name' is present! Value: " . htmlspecialchars($_GET['name']) . "</p>";
} else {
    echo "<p>No query parameter 'name' present.</p>";
}
?>
<h2>Send POST Data</h2>
<form method="POST">
    <label>Test input: <input type="text" name="test" value="hello"></label><br><br>
    <input type="submit" value="Submit">
</form>
<?php
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $raw_input = file_get_contents('php://input');
    echo "<h2>Raw POST data</h2>";
    echo "<pre>" . htmlspecialchars($raw_input) . "</pre>";
} else {
    echo "<p>No POST data yet</p>";
}

// CGI / server variables
$server_vars = [
    "AUTH_TYPE",
    "CONTENT_LENGTH",
    "CONTENT_TYPE",
    "GATEWAY_INTERFACE",
    "REMOTE_ADDR",
    "REQUEST_METHOD",
    "SCRIPT_NAME",
    "SCRIPT_FILENAME",
    "QUERY_STRING",
    "PATH_INFO",
    "PATH_TRANSLATED",
    "SERVER_NAME",
    "SERVER_PORT",
    "SERVER_PROTOCOL",
    "SERVER_SOFTWARE"
];

$http_headers = [];
foreach ($_SERVER as $key => $value) {
    if (strpos($key, 'HTTP_') === 0) {
        $http_headers[$key] = $value;
    }
}

echo "<h2>CGI Variables</h2>";
echo "<table border='1' cellpadding='5'><tr><th>Variable</th><th>Value</th></tr>";

foreach ($server_vars as $var) {
    $value = $_SERVER[$var] ?? 'Not set';
    echo "<tr><td>" . htmlspecialchars($var) . "</td><td>" . htmlspecialchars($value) . "</td></tr>";
}

foreach ($http_headers as $key => $value) {
    echo "<tr><td>" . htmlspecialchars($key) . "</td><td>" . htmlspecialchars($value) . "</td></tr>";
}

echo "</table>";
?>
</body></html>
