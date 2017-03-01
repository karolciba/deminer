from deminer import Board
from deminer import Field
import numpy as np

from collections import defaultdict
from collections import namedtuple


window_size = 3
neurons_count = 128
vector_len = len(Field)
neurons_input_len = (2*window_size+1)**2 * vector_len

# Neural network solver
default_database = {}
default_database['hidden'] = 2*np.random.random((neurons_input_len+1,neurons_count)) - 1
default_database['output'] = 2*np.random.random(neurons_count+1) - 1

db = default_database

def sigmoid(x):
    return 1/(1+np.exp(-x))

def d_sigmoid(x):
    return x*(1-x)

def tanh(x):
    return np.tanh(x)

def d_tanh(x):
    return 1.0 - np.tanh(x)**2

nonlin = sigmoid
d_nonlin = d_sigmoid

def forward(window, database):
    vector = vectorize(window)

    # TODO: this is inherently wrong
    biased = [1]
    biased.extend(vector)
    biased = np.array(biased)

    hidden = database['hidden']

    l1 = np.dot(biased, hidden)

    a1 =  nonlin(l1)

    # TODO: this is inherently wrong
    a1_biased = [1]
    a1_biased.extend(a1)
    a1_biased = np.array(a1_biased)

    out = database['output']
    l2 = np.dot(a1_biased, out)

    a2 = nonlin(l2)

    return a2

def forward_backward(window, desired, database):
    vector = vectorize(window)
    # import pdb; pdb.set_trace()

    # TODO: this is inherently wrong
    biased = [1]
    biased.extend(vector)
    biased = np.array(biased)

    hidden = database['hidden']

    l1 = np.dot(biased, hidden)

    a1 =  nonlin(l1)

    # TODO: this is inherently wrong
    a1_biased = [1]
    a1_biased.extend(a1)
    a1_biased = np.array(a1_biased)

    out = database['output']
    l2 = np.dot(a1_biased, out)

    a2 = nonlin(l2)

    l2_error = desired - a2
    l2_delta = l2_error * d_nonlin(a2)
    l1_error = l2_delta * hidden
    l1_delta = l1_error * d_nonlin(a1)

    out += a1_biased * l2_delta
    hidden += biased[:,None] * l1_delta

    return a2

vector_map = [
    #0,1,2,3,4,5,6,7,8,9,0,1
    [1,0,0,0,0,0,0,0,0,0,0,0],
    [0,1,0,0,0,0,0,0,0,0,0,0],
    [0,0,1,0,0,0,0,0,0,0,0,0],
    [0,0,0,1,0,0,0,0,0,0,0,0],
    [0,0,0,0,1,0,0,0,0,0,0,0],
    [0,0,0,0,0,1,0,0,0,0,0,0],
    [0,0,0,0,0,0,1,0,0,0,0,0],
    [0,0,0,0,0,0,0,1,0,0,0,0],
    [0,0,0,0,0,0,0,0,1,0,0,0],
    [0,0,0,0,0,0,0,0,0,1,0,0],
    [0,0,0,0,0,0,0,0,0,0,1,0],
    [0,0,0,0,0,0,0,0,0,0,0,1],
]
def vectorize(window):
    vector = [ vector_map[v.value] for v in window ]
    out = []
    for v in vector:
        out.extend(v)
    return np.array(out)

def train(database, window_size = window_size, debug = False):
    board = Board()
    ingame = True

    move_history = []
    counter = 0
    while ingame:
        fringe = []
        counter+=1
        # print "Itera", counter
        for move in board.valid_moves():
            probs = []
            window = board.window(move,window_size)
            prob = forward(window,database)

            # print "Move in db", move, rec.chance(), key
            fringe.append( (move, prob, window) )

        best = min(fringe, key=lambda x: x[1])

        move = best[0]
        prob = best[1]
        window = best[2]

        # print "Best move", best[0], "probability", best[1], "key", best[2]

        # import pdb; pdb.set_trace()
        move_history.append((move,prob))

        ret = board.uncover(best[0])
        if ret is None:
            # print "in game"
            forward_backward(window,0,database)
            # print "Move", move
            # print board
        elif ret:
            forward_backward(window,0,database)
            print "Won after", counter, "with move", move, prob
            print board
            # import pdb; pdb.set_trace()
            ingame = False
        else:
            if debug:
                print "Lost after", counter, "with move", move, prob
                print move_history
                print board
            forward_backward(window,1,database)
            # import pdb; pdb.set_trace()
            ingame = False
            return board

def save(database):
    import cPickle
    with open("db_neural.db","w+b") as file:
        pickler = cPickle.Pickler(file)
        pickler.dump(database)

def load():
    import cPickle
    with open("db_neural.db","rb") as file:
        pickler = cPickle.Unpickler(file)
        return pickler.load()

def clear():
    save(default_database)


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
    count = 0
    out_layer = np.array(list(db['output']))
    try:
        while True:
            ret = train(db)
            if ret:
                losts += 1
            else:
                wons += 1
            count += 1
            if count % 1000 == 0:
                print "saved"
                save(db)
    except:
        pass

    print "Wons {} Losts {} Ratio {}".format(wons,losts,float(wons)/(wons+losts))
    print "Out layer diff", db['output'] - out_layer
    print "Out layer", db['output']
    print "Saving db",
    save(db)
    print "saved"

    return db

