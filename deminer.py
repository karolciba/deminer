from enum import Enum

class Field(Enum):
    ZERO = 0
    ONE = 1
    TWO = 2
    THREE = 3
    FOUR = 4
    FIVE = 5
    SIX = 6
    SEVEN = 7
    EIGHT = 8
    MINE = 9
    UNKNOWN = 10
    NONE = 11

field_string_map = dict( [(Field.MINE,'*'), (Field.UNKNOWN,'#'), (Field.NONE, '_')] + [(Field(x),str(x)) for x in range(9)])

def vsum(pos, delta):
    """Sums two vectors in tuple/list format"""
    return (pos[0]+delta[0],pos[1]+delta[1])

# cell neighbours
neighbours = [(-1,-1), (-1,0), (-1,1),
               (0,-1),          (0,1),
               (1,-1),  (1,0),  (1,1)]

sides       = [         (-1,0),
               (0,-1),          (0,1),
                        (1,0)]
class Board(object):
    def __init__(self, rows = 8, cols = 8, mines_count = 10, initialize = True):
        from collections import Counter
        self.rows = rows
        self.cols = cols
        self.mines_count = mines_count
        self.board = Counter()
        self.mines = set()
        self.visible = set()
        self.left_fields = rows * cols
        self.fields_count = rows * cols
        if initialize:
            self.initialize()

    def filter(self,positions):
        """Filter invalid positions for board, ones that are outside board"""
        rows = self.rows
        cols = self.cols
        def gen(positions):
            for pos in positions:
                if pos[0] >= 0 and pos[0] < rows and pos[1] >= 0 and pos[1] < rows:
                    yield pos
        return gen(positions)

    def window(self,pos,side):
        """Return list of Field enums for given window"""
        w = []
        for x in range(pos[0]-side,pos[0]+side+1):
            for y in range(pos[1]-side,pos[1]+side+1):
                test_pos = (x,y)
                if self._inboard(test_pos):
                    if not test_pos in self.visible:
                        w.append(Field.UNKNOWN)
                    else:
                        w.append(Field(self.board[test_pos]))
                else:
                    w.append(Field.NONE)
        return w
    def window_to_string(self, window):
        l = [ field_string_map[f] for f in window ]
        return "".join(l)

    def valid_moves(self):
        possible = ((x,y) for x in range(self.rows) for y in range(self.cols))
        return [pos for pos in possible if pos not in self.visible]

    def initialize(self):
        from random import randint
        from collections import Counter
        self.mines = set()
        self.visible = set()
        self.board = Counter()

        possible = [(x,y) for x in range(self.rows) for y in range(self.cols)]
        count = len(possible)
        for i in range(self.mines_count):
            rand_index = randint(0, count - i - 1)
            element = possible[rand_index]
            self.mines.add(element)
            possible.remove(element)
            x, y = element

            # TODO: refactor to list of valid positions
            for pos in self.filter( vsum(element,delta) for delta in neighbours):
                self.board[pos] += 1
            # self.board[x-1,y-1] += 1
            # self.board[x-1,y] += 1
            # self.board[x-1,y+1] += 1
            # self.board[x,y-1] += 1
            # self.board[x,y+1] += 1
            # self.board[x+1,y-1] += 1
            # self.board[x+1,y] += 1
            # self.board[x+1,y+1] += 1


    def uncover(self,pos):
        """Uncovers field, returns tri-state logic: True if won, False if lost, None otherwise"""
        if pos in self.mines:
            self.visible.add(pos)
            return False
        else:
            self._uncover(pos)
        # if self.left_fields == self.mines_count:
        if len(self.visible) == self.fields_count - self.mines_count:
            return True
        return None

    def _inboard(self,pos):
        if pos[0] < 0:
            return False
        if pos[0] >= self.rows:
            return False
        if pos[1] < 0:
            return False
        if pos[1] >= self.cols:
            return False
        return True

    def _uncover(self,pos):
        # check if in board
        if not self._inboard(pos):
            return True
        # check if no already visible
        if pos in self.visible:
            return True
        # make visible
        self.visible.add(pos)
        self.left_fields -= 1
        # if not zero do nothing more
        if self.board[pos] > 0:
            return True
        else:
            for npos in self.filter([vsum(pos,delta) for delta in neighbours]):
                # print "tested position", npos,
                if npos not in self.visible:
                    # print "not visible"
                    self._uncover(npos)


    def print_board(self, debug = False):
        out = "Mines {} left fields {}\n".format(self.mines.count,self.left_fields)
        for pos in [(x,y) for x in range(self.rows) for y in range(self.cols)]:
            if pos in self.visible:
                if pos in self.mines:
                    out += '*'
                else:
                    out += str(self.board[pos])
            else:
                out += '#'

            if pos[1] == 8:
                out += '\n'
        print out

    def print_debug_board(self):
        print self.__str__()

    def __str__(self):
        out = "Mines {} left fields {}\n".format(self.mines_count,self.left_fields)
        out += "  "
        for x in range(self.cols):
            out += str(x)[-1]
        out += "\n"
        out += "  "
        out += "-"*self.cols
        out += "\n"

        for pos in [(x,y) for x in range(self.rows) for y in range(self.cols)]:
            if pos[1] == 0:
                out += str(pos[0])[-1]
                out += "|"
            if pos not in self.visible:
                out += '\033[7m'
            if pos in self.mines:
                out += '*'
            else:
                out += str(self.board[pos])
            out += '\033[0m'

            if pos[1] == self.cols - 1:
                out += '\n'
        return out
