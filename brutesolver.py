from deminer import Board

from collections import defaultdict
from collections import namedtuple
import shelve

class Record(object):
    def __init__(self):
        self.mines = 0
        # smooth, couse in wrong case you can get 1/2 or 2/3 mines and
        # virtually never try it again. Solver has to find at least
        # 150 mines to disregard it over base probal ~= 0.150
        self.tries = 1000
    def chance(self):
        return float(self.tries - self.mines)/self.tries
    def risk(self):
        return float(self.mines)/self.tries
    def mine(self):
        self.mines += 1
        self.tries += 1
        return self
    def nomine(self):
        self.tries += 1
        return self
    def __str__(self):
        return "Record {}/{} = {}".format(self.mines,self.tries,self.risk())
    def __repr__(self):
        return "Record {}/{} = {}".format(self.mines,self.tries,self.risk())

def train(database, window = 2):
    board = Board()
    ingame = True

    counter = 0
    while ingame:
        probs = []
        counter+=1
        # print "Itera", counter
        for move in board.valid_moves():
            w = board.window(move,window)
            key = board.window_to_string(w)
            rec = database.get(key)
            if rec:
                # print "Move in db", move, rec.chance(), key
                probs.append( (move, rec.risk(), rec, key) )
            else:
                # print "Move not i", move, key
                rec = Record()
                # database[key] = rec
                probs.append( (move, 0.0, rec, key) )

        best = min(probs, key=lambda x: x[1])

        move = best[0]
        prob = best[1]
        rec = best[2]
        key = best[3]

        # print "Best move", best[0], "probability", best[1], "key", best[2]

        # import pdb; pdb.set_trace()

        ret = board.uncover(best[0])
        if ret is None:
            # print "in game"
            database[key] = rec.nomine()
        elif ret:
            database[key] = rec.nomine()
            print "Won after", counter
            print board
            ingame = False
        else:
            # print "you lost after", counter, "with move", move
            database[key] = rec.mine()
            # import pdb; pdb.set_trace()
            ingame = False
            # print board
            return board

def save(database):
    import cPickle
    with open("db_brute.db","w+b") as file:
        pickler = cPickle.Pickler(file)
        pickler.dump(database)

def load():
    import cPickle
    with open("db_brute.db","rb") as file:
        pickler = cPickle.Unpickler(file)
        return pickler.load()

def clear():
    save({})


def least_prob(db, min = 5):
    return sorted([ (k,v) for k,v in db.iteritems() if v.tries >= min ],key=lambda x: x[1].risk())

def most_prob(db, min = 5):
    return sorted([ (k,v) for k,v in db.iteritems() if v.tries >= min ],key=lambda x: x[1].risk(), reverse=True)

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

