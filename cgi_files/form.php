<?php
    echo "<html><body>";
    echo "<h1>Form Data Test</h1>";
    if ($_SERVER['REQUEST_METHOD'] == 'POST') {
        $input = file_get_contents('php://input');
        echo "<p>Raw POST data: " . htmlspecialchars($input) . "</p>";
    } else {
        echo "<p>No POST data</p>";
    }
?>
<form method="POST">
    <input type="text" name="test" value="hello">
    <input type="submit" value="Submit">
</form>
</body></html>
