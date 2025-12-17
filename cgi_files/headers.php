<?php
// Output proper HTTP headers
header("HTTP/1.1 200 OK");
header("Content-Type: text/html; charset=UTF-8");
// Don't output any other headers

echo "<html><body>";
echo "<h1>Hello from PHP CGI!</h1>";
echo "<p>Request Method: " . $_SERVER['REQUEST_METHOD'] . "</p>";
echo "<p>Query String: " . $_SERVER['QUERY_STRING'] . "</p>";
echo "<p>Path Info: " . $_SERVER['PATH_INFO'] . "</p>";
echo "<p>Script Filename: " . $_SERVER['SCRIPT_FILENAME'] . "</p>";
echo "<p>Content Length: " . ($_SERVER['CONTENT_LENGTH'] ?? 'Not set') . "</p>";
echo "<p>User Agent: " . ($_SERVER['HTTP_USER_AGENT'] ?? 'Not set') . "</p>";
echo "<p>REDIRECT STATUS: " . ($_SERVER['REDIRECT_STATUS'] ?? 'Not set') . "</p>";
echo "</body></html>";
?>