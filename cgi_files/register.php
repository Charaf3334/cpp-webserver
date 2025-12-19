<?php
  session_start(); // kat9eleb 3la PHPSESSID flcookies ola lgatha katshowi details dyal user

  header("HTTP/1.1 200 OK");
  header("Content-Type: text/html");
  function h($value) {
      return htmlspecialchars((string)$value, ENT_QUOTES, 'UTF-8');
  }
?>
<!DOCTYPE html>
<html lang="en">
  <head>
      <meta charset="UTF-8">
      <title>Register</title>
      <link rel="stylesheet" href="style.css">
  </head>
  <body>
    <h1>Register</h1>

    <form method="post">
        <label>
            Username
            <input type="text" name="username" required>
        </label>

        <label>
            Email
            <input type="text" name="email" required>
        </label>

        <label>
            Password
            <input type="password" name="password" required>
        </label>

        <button type="submit">Register</button>
    </form>

<?php
if ($_SERVER['REQUEST_METHOD'] === 'POST') {

    $username = $_POST['username'] ?? '';
    $email    = $_POST['email'] ?? '';
    $password = $_POST['password'] ?? '';

    $passwordHash = password_hash($password, PASSWORD_DEFAULT);

    session_regenerate_id(true); // m3a katgenerate lcookie again, the old one is ont valid any more

    $_SESSION['token'] = bin2hex(random_bytes(32));
    $_SESSION['username'] = $username;

    echo '<div class="success">';
    echo '<h3>Registered Data</h3>';
    echo '<p><strong>Username:</strong> ' . h($username) . '</p>';
    echo '<p><strong>Email:</strong> ' . h($email) . '</p>';
    echo '<p><strong>Session Token:</strong><br>' . h($_SESSION['token']) . '</p>';
    echo '</div>';
}
?>

</body>
</html>
