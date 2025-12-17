<?php
// Output proper HTTP headers
header("HTTP/1.1 200 OK");
header("Content-Type: text/html");

function h($value) {
    return htmlspecialchars((string)$value, ENT_QUOTES, 'UTF-8');
}

echo "<!DOCTYPE html>";
echo "<html>";
echo "<head>";
echo "<title>Register</title>";
echo "<style>
        body { font-family: Arial, sans-serif; max-width: 500px; margin: 40px auto; }
        label { display: block; margin-top: 10px; }
        input { width: 100%; padding: 8px; margin-top: 4px; }
        button { margin-top: 15px; padding: 10px; width: 100%; }
        .success { background: #e6ffe6; padding: 10px; margin-top: 20px; }
      </style>";
echo "</head>";
echo "<body>";

echo "<h1>Register</h1>";

/* =======================
   Registration Form
   ======================= */
echo "<form method='post'>";
echo "<label>Username
        <input type='text' name='username' required>
      </label>";

echo "<label>Email
        <input type='email' name='email' required>
      </label>";

echo "<label>Password
        <input type='password' name='password' required>
      </label>";

echo "<button type='submit'>Register</button>";
echo "</form>";

/* =======================
   Handle Submission
   ======================= */
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $username = $_POST['username'] ?? '';
    $email    = $_POST['email'] ?? '';
    $password = $_POST['password'] ?? '';

    $passwordHash = password_hash($password, PASSWORD_DEFAULT);

    echo "<div class='success'>";
    echo "<h3>Registered Data</h3>";
    echo "<p><strong>Username:</strong> " . h($username) . "</p>";
    echo "<p><strong>Email:</strong> " . h($email) . "</p>";
    echo "<p><strong>Password Hash:</strong> " . h($passwordHash) . "</p>";
    echo "</div>";
}

echo "</body>";
echo "</html>";
?>
