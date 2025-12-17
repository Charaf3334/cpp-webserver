<?php
require 'init.php';

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $users = load_users();

    $email = strtolower(trim($_POST['email'] ?? ''));

    if (isset($users[$email])) {
        die("User already exists");
    }

    $users[$email] = [
        'fname' => trim($_POST['fname']),
        'lname' => trim($_POST['lname']),
        'password' => password_hash($_POST['password'], PASSWORD_DEFAULT)
    ];

    save_users($users);
    echo "Registration successful. <a href='login.php'>Login</a>";
    exit;
}
?>

<h1>Register</h1>
<form method="post">
    First name: <input name="fname" required><br><br>
    Last name: <input name="lname" required><br><br>
    Email: <input name="email" type="email" required><br><br>
    Password: <input name="password" type="password" required><br><br>
    <button>Register</button>
</form>
