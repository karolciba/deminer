from deminer import Board

from collections import defaultdict
from collections import namedtuple

# Naive Bayses solver for MineSweeper
# P(M|Window) = P(Window|M)*P(M)/P(Window)
# P(M|Window) ~= P(Window|M)
# P(M|Windows) ~= P(W1|M)*P(W2|M)*...*P(Wn|M)

class Record(object):
    def __init__(self):
        # smooth, couse in wrong case you can get 1/2 or 2/3 mines and
        # virtually never try it again. Solver has to find at least
        # 150 mines to disregard it over base probal ~= 0.150
        self.safe = 1000
        self.tries = 1000
    def chance(self):
        return float(self.safe)/self.tries
    def risk(self):
        return float(self.tries - self.safe)/self.tries
    def mine(self):
        self.tries += 1
        return self
    def nomine(self):
        self.safe += 1
        self.tries += 1
        return self
    def __str__(self):
        return "Record {}/{} = {}".format(self.safe,self.tries,self.chance())
    def __repr__(self):
        return "Record {}/{} = {}".format(self.safe,self.tries,self.chance())

def mul(iter):
    m = 1.0
    for i in iter:
        m = m * i
    return m

def train(database, window_size = 3):
    board = Board()
    ingame = True

    counter = 0
    while ingame:
        fringe = []
        counter+=1
        # print "Itera", counter
        for move in board.valid_moves():
            probs = []
            window = board.window(move,window_size)
            for i,w in enumerate(window):
                key = i,w
                rec = database.get(key)
                if not rec:
                    rec = Record()
                    database[key] = rec
                probs.append((key,rec.chance(),rec))

            prob = mul(x[1] for x in probs)

            # print "Move in db", move, rec.chance(), key
            fringe.append( (move, prob, probs) )

        best = max(fringe, key=lambda x: x[1])

        move = best[0]
        prob = best[1]
        probs = best[2]

        # print "Best move", best[0], "probability", best[1], "key", best[2]

        # import pdb; pdb.set_trace()

        ret = board.uncover(best[0])
        if ret is None:
            # print "in game"
            for row in probs:
                row[2].nomine()
            # print "Move", move
            # print board
        elif ret:
            for row in probs:
                row[2].nomine()
            print "Won after", counter, "with move", move, prob
            print board
            ingame = False
        else:
            # print "Lost after", counter, "with move", move, prob
            # print board
            for row in probs:
                row[2].mine()
            # import pdb; pdb.set_trace()
            ingame = False
            return board

def save(database):
    import cPickle
    with open("db_naive.db","w+b") as file:
        pickler = cPickle.Pickler(file)
        pickler.dump(database)

def load():
    import cPickle
    with open("db_naive.db","rb") as file:
        pickler = cPickle.Unpickler(file)
        return pickler.load()

def clear():
    save({})


def least_prob(db, min = 5):
    return sorted([ (k,v) for k,v in db.iteritems() if v.tries >= min ],key=lambda x: x[1].chance())

def most_prob(db, min = 5):
    return sorted([ (k,v) for k,v in db.iteritems() if v.tries >= min ], key=lambda x: x[1].risk())

def most_tried(db):
    return sorted([ (k,v) for k,v in db.iteritems() ],key=lambda x: x[1].tries, reverse=True)

def most_mines(db):
    return sorted([ (k,v) for k,v in db.iteritems() ],key=lambda x: x[1].mines, reverse=True)

def procedure(db = None):
    if not db:
        try:
            db = load()
        except:
            clear()
            db = load()

    wons = 0
    losts = 0
    try:
        while True:
            ret = train(db)
            if ret:
                losts += 1
            else:
                wons += 1
    except:
        pass

    print "Wons {} Losts {} Ratio {}".format(wons,losts,float(wons)/(wons+losts))
    print "Saving db",
    save(db)
    print "saved"

    return db

