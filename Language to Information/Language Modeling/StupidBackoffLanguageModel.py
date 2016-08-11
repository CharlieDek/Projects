import math, collections

class StupidBackoffLanguageModel:

  def __init__(self, corpus):
    """Initialize your data structures in the constructor."""
    self.uniqueWords = set([])
    self.unigramCounts = collections.defaultdict(lambda: 0) #should set all counts to 1 as default
    self.bigramCounts = collections.defaultdict(lambda: 0)
    self.total = 0
    self.train(corpus)

  def train(self, corpus):
    """ Takes a corpus and trains your language model. 
        Compute any counts or other corpus statistics in this function.
    """
    for sentence in corpus.corpus:
      previous = "<s>"
      self.unigramCounts["<s>"] = 1
      for datum in sentence.data:
         if (datum.word != "<s>"):
           token = datum.word
           if not token in self.uniqueWords:
             self.uniqueWords.add(token)
           bg = (previous, token)
           self.unigramCounts[token] = self.unigramCounts[token] + 1
           self.bigramCounts[bg] = self.bigramCounts[bg] + 1
           self.total += 1
           previous = token

  pass

  def score(self, sentence):
    """ Takes a list of strings as argument and returns the log-probability of the 
        sentence using your language model. Use whatever data you computed in train() here.
    """
    score = 0.0
    uCount = 0
    WEIGHT = .4
    v = len(self.uniqueWords)
    previous = sentence[0]
    uCount += self.unigramCounts[previous]
    score += math.log(uCount + 1)
    score -= math.log(self.total + v)
    score += math.log(WEIGHT)
    previous = previous[:]
    uCount = 0

    for i in xrange(1, len(sentence) - 1):
        token = sentence[i]
        bg = (previous, token)

        if self.bigramCounts[bg] is 0:
          uCount += self.unigramCounts[token]
          score += math.log(uCount + 1)
          score -= math.log(self.total + v)
          score += math.log(WEIGHT)
          previous = token[:]
          uCount = 0

        else:
          score += math.log(self.bigramCounts[bg])
          score -= math.log(self.unigramCounts[previous])
          previous = token[:]


    return score