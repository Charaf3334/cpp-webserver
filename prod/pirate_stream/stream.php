<?php
    require 'init.php';

    if (!isset($_SESSION['user'])) {
        header("Location: login.php");
        exit;
    }

    $movie_title = isset($_GET['title']) ? trim($_GET['title']) : '';

    $movies = load_movies();
    $selected_movie = null;

    if (!empty($movie_title)) {
        foreach ($movies as $movie) {
            if ($movie['title'] === $movie_title) {
                $selected_movie = $movie;
                break;
            }
        }
    }

    if (!$selected_movie) {
        header("Location: main.php");
        exit;
    }

    require 'header.php';
?>
    <div class="stream-container">
        <a href="main.php" class="back-btn">← Back to Movies</a>
        
        <div class="movie-player-section">
            <h1 class="movie-title"><?php echo h($selected_movie['title']); ?> (<?php echo h($selected_movie['year']); ?>)</h1>
            
            <div class="video-wrapper">
                <video controls autoplay class="movie-player">
                    <source src="<?php echo h($selected_movie['movie_url']); ?>">
                    Your browser does not support the video tag.
                </video>
            </div>
            
            <div class="movie-description">
                <p><?php echo h($selected_movie['description']); ?></p>
            </div>
        </div>
    </div>

<?php 
    require 'footer.php';
?>