import time

class player_t:
    def __init__(self):
        self.id = 0
        self.name = ''
        self.x = 0
        self.y = 0
        self.last_move_tm = time.time()

