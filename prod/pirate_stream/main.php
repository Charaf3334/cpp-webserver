<?php
    require 'init.php';
    $movies = load_movies();
    
    require 'header.php';
?>
    <div>
        <?php
            if (!isset($_SESSION['user'])) {
                echo '<h1 id="hero">
                        Welcome dear 1337 students<br/>
                        Main character syndrome: now streaming in 4k<br/>
                        <a href="login.php">Start my character arc</a>
                      </h1>';
            } else {
                $fname = $_SESSION['user']['fname'];
                echo '<h1 id="hero">
                        Welcome ' . htmlspecialchars($fname) . ', the journey begins now<br/>
                        Main character vibes activated<br/>
                        Let\'s make this arc unforgettable.<br/>
                        <a href="logout.php">Logout</a>
                      </h1>';
            }
        ?>

        <main class="main-container">
            <div class="movies">
                <?php foreach ($movies as $movie): ?>
                    <div class="movie-card">
                        <div class="movie-image">
                            <img src="<?php echo $movie['url']; ?>" alt="<?php echo $movie['title']; ?>" class="poster">
                            <button onclick="window.location.href='<?php echo strtolower(str_replace(' ', '_', $movie['title'])); ?>.html'" class="watch-button">Watch</button>
                        </div>
                        <div class="movie-info">
                            <h2><?php echo $movie['title']; ?> - <?php echo $movie['year']; ?></h2>
                            <p><?php echo $movie['description']; ?></p>
                        </div>
                    </div>
                <?php endforeach; ?>
            </div>
        </main>
    </div>

<?php
    require 'footer.php';
?>
