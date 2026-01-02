<?php
    require 'init.php';

    $error_message = '';

    if ($_SERVER['REQUEST_METHOD'] === 'POST') {
        $users = load_users();
        $email = strtolower(trim($_POST['email'] ?? ''));

        if (isset($users[$email]))
            $error_message = "Email already exists. Main character conflict detected.";
        else {
            $passwordHash = password_hash($_POST['password'], PASSWORD_DEFAULT);

            $users[$email] = [
                'fname' => $_POST['fname'] ?? 'Hero',
                'lname' => $_POST['lname'] ?? '',
                'password' => $passwordHash
            ];

            save_users($users);

            $_SESSION['user'] = [
                'email' => $email,
                'fname' => $users[$email]['fname'],
                'lname' => $users[$email]['lname']
            ];

            header("Location: main.php");
            exit;
        }
    }

    require 'header.php';
?>

    <div class="login-container">
        <div class="login-card">
            <h1 class="login-title">Create Your Heroic Tale</h1>
            <p class="login-subtitle">This is not a sidekick origin story</p>

            <form method="post">
                <div class="input-group">
                    <label>First Name</label>
                    <input name="fname" type="text" required value="<?php echo htmlspecialchars($_POST['fname'] ?? '', ENT_QUOTES); ?>">
                </div>

                <div class="input-group">
                    <label>Last Name</label>
                    <input name="lname" type="text" value="<?php echo htmlspecialchars($_POST['lname'] ?? '', ENT_QUOTES); ?>">
                </div>

                <div class="input-group">
                    <label>Email</label>
                    <input name="email" type="email" required value="<?php echo htmlspecialchars($_POST['email'] ?? '', ENT_QUOTES); ?>">

                    <?php if ($error_message): ?>
                        <p class="error-message"><?php echo $error_message; ?></p>
                    <?php endif; ?>
                </div>

                <div class="input-group">
                    <label>Password</label>
                    <input name="password" type="password" required>
                </div>

                <p class="register-link">
                    Already a legend? <a href="login.php">Start your arc</a>
                </p>

                <button class="login-btn">Create Account</button>
            </form>
        </div>
    </div>

<?php
    require 'footer.php';
?>
