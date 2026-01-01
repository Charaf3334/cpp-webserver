<?php
require 'init.php';

if (!isset($_SESSION['user'])) {
    header("Location: login.php");
    exit;
}

$user = $_SESSION['user'];
?>

<h1>Profile</h1>
<p>Welcome <strong><?= h($user['fname'] . ' ' . $user['lname']) ?></strong></p>

<a href="logout.php">Logout</a>
