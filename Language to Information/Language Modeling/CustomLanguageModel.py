import math, collections
class CustomLanguageModel:

  def __init__(self, corpus):
    """Initialize your data structures in the constructor."""
    self.uniqueWords = set([])
    self.unigramCounts = collections.defaultdict(lambda: 0)
    self.bigramCounts = collections.defaultdict(lambda: 0)
    self.trigramCounts = collections.defaultdict(lambda: 0)
    self.total = 0
    self.train(corpus)
    self.train(corpus)

  def train(self, corpus):
    """ Takes a corpus and trains your language model. 
        Compute any counts or other corpus statistics in this function.
    """

    for sentence in corpus.corpus:
        first = "<NULL>"
        second = "<NULL>"
        for datum in sentence.data:
           token = datum.word
           self.unigramCounts[token] = self.unigramCounts[token] + 1
           self.total += 1

           if not token in self.uniqueWords:
             self.uniqueWords.add(token)

           if (second != "<NULL>"):
             bg = (first, second)
             self.bigramCounts[bg] = self.bigramCounts[bg] + 1

           if (first != "<NULL>"):

             tg = (first, second, token)
             self.trigramCounts[tg] += 1

           first = second
           second = token


    pass

  def score(self, sentence):
    """ Takes a list of strings as argument and returns the log-probability of the
        sentence using your language model. Use whatever data you computed in train() here.
    """
    score = 0.0
    WEIGHT2 = .4

    #special case first 2

    v = len(self.uniqueWords)
    first = sentence[0]
    score += math.log(self.unigramCounts[first] + 1)
    score -= math.log(self.total + v)

    second = sentence[1]
    bg = (first, second)
    if not self.bigramCounts[bg] is 0:
        score += math.log(self.bigramCounts[bg])
        score += math.log(WEIGHT2)
        score -= math.log(self.unigramCounts[first])

    else:
        score += score

    for i in xrange(2, len(sentence) - 1):
        token = sentence[i]
        bg = (second, token)
        tg = (first, second, token)

        currbg = self.bigramCounts[bg]

        if self.trigramCounts[tg] is 0:
          x = currbg
          if (x != 0):
             score += math.log(self.bigramCounts[bg])
             score -= math.log(self.unigramCounts[second])
             score += math.log(WEIGHT2)
             first = second[:]
             second = token[:]

          else:
              score += math.log(self.unigramCounts[token] + 1)
              score -= math.log(self.total + v)
              first = second[:]
              second = token[:]

        else:
            score += math.log(self.trigramCounts[tg])
            thisbg = (first, second)
            score -= math.log(self.bigramCounts[thisbg])
            first = second[:]
            second = token[:]


    return score
