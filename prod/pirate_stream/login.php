<?php
    require 'init.php';

    $error_message = '';

    if ($_SERVER['REQUEST_METHOD'] === 'POST') {
        $users = load_users();
        $email = strtolower(trim($_POST['email'] ?? ''));

        if (!isset($users[$email]))
            $error_message = "Invalid credentials";
        elseif (!password_verify($_POST['password'], $users[$email]['password']))
            $error_message = "Invalid credentials";
        else {
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
                <h1 class="login-title">The Hero Returns</h1>
                <p class="login-subtitle">Main character energy required</p>

                <form method="post">
                    <div class="input-group">
                        <label>Email</label>
                        <input name="email" type="email" required value="<?php echo htmlspecialchars($_POST['email'] ?? '', ENT_QUOTES); ?>">
                    </div>

                    <div class="input-group">
                        <label>Password</label>
                        <input name="password" type="password" required>

                        <?php if ($error_message): ?>
                            <p class="error-message">
                                <?php echo $error_message; ?>
                            </p>
                        <?php endif; ?>
                    </div>


                    <p class="register-link">
                        Still an NPC? <a href="register.php">Create a character</a>
                    </p>

                    <button class="login-btn">Login</button>
                </form>
            </div>
        </div>

<?php 
    require "footer.php"
?>
