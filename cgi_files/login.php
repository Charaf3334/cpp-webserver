<?php
// Output proper HTTP headers
header("HTTP/1.1 200 OK");
header("Content-Type: text/html; charset=UTF-8");

function h($value) {
    return htmlspecialchars((string)$value, ENT_QUOTES, 'UTF-8');
}

echo "<!DOCTYPE html>";
echo "<html>";
echo "<head>";
echo "<title>PHP Environment Variables</title>";
echo "<style>
        body { font-family: Arial, sans-serif; }
        table { border-collapse: collapse; width: 100%; }
        th, td { border: 1px solid #ccc; padding: 6px; text-align: left; }
        th { background: #f4f4f4; }
        h2 { margin-top: 30px; }
      </style>";
echo "</head>";
echo "<body>";

echo "<h1>PHP Environment Variables</h1>";

/* =======================
   Login Form
   ======================= */
echo "<h2>Login</h2>";
echo "<form method='post'>";
echo "<label>Username: <input type='text' name='username'></label><br><br>";
echo "<label>Password: <input type='password' name='password'></label><br><br>";
echo "<input type='submit' value='Submit'>";
echo "</form>";

if (!empty($_POST)) {
    echo "<h3>Submitted Values</h3>";
    echo "<p>Username: " . h($_POST['username'] ?? '') . "</p>";
    echo "<p>Password: " . h($_POST['password'] ?? '') . "</p>";
}

/* =======================
   $_SERVER variables
   ======================= */
echo "<h2>\$_SERVER</h2>";
echo "<table><tr><th>Key</th><th>Value</th></tr>";
foreach ($_SERVER as $key => $value) {
    echo "<tr><td>" . h($key) . "</td><td>" . h($value) . "</td></tr>";
}
echo "</table>";

/* =======================
   getenv()
   ======================= */
echo "<h2>getenv()</h2>";
echo "<table><tr><th>Key</th><th>Value</th></tr>";
foreach (getenv() as $key => $value) {
    echo "<tr><td>" . h($key) . "</td><td>" . h($value) . "</td></tr>";
}
echo "</table>";

echo "</body>";
echo "</html>";
?>
