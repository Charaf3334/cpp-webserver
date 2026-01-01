<?php
require 'init.php';
session_destroy();
header("Location: login.php");
?>