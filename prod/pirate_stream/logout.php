<?php
require 'init.php';
session_destroy();
header("Location: main.php");
?>