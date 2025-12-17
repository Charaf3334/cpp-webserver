<?php
session_start();

define('USER_FILE', __DIR__ . '/users.json');

function h($v) {
    return htmlspecialchars((string)$v, ENT_QUOTES, 'UTF-8');
}

function load_users() {
    if (!file_exists(USER_FILE)) {
        return [];
    }
    return json_decode(file_get_contents(USER_FILE), true) ?: [];
}

function save_users($users) {
    file_put_contents(USER_FILE, json_encode($users, JSON_PRETTY_PRINT));
}
