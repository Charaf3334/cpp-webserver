<?php
require 'init.php';

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $users = load_users();
    $email = strtolower(trim($_POST['email'] ?? ''));

    if (!isset($users[$email])) {
        die("Invalid credentials");
    }

    if (!password_verify($_POST['password'], $users[$email]['password'])) {
        die("Invalid credentials");
    }

    $_SESSION['user'] = [
        'email' => $email,
        'fname' => $users[$email]['fname'],
        'lname' => $users[$email]['lname']
    ];

    header("Location: profile.php");
    exit;
}
?>

<h1>Login</h1>
<form method="post">
    Email: <input name="email" type="email" required><br><br>
    Password: <input name="password" type="password" required><br><br>
    <button>Login</button>
</form>
