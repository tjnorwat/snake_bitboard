def evaluate(player, depth):
    if (player.done):
        return -1000 - depth
    elif (player.ate_apple):
        return 100 + depth
    
def minimax(game_state, depth):
    best_score = -inf
    for (player1_move in player1_moves):
        for(player2_move in player2_moves):
            new_game_state = step(player1_move, player2_move)
            score = minimax(new_game_state, depth)
            best_score = max(best_score, score)