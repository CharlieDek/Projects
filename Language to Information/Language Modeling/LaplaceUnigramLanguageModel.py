import math, collections

class LaplaceUnigramLanguageModel:

  def __init__(self, corpus):
    """Initialize your data structures in the constructor."""
    self.uniqueWords = set([])
    self.unigramCounts = collections.defaultdict(lambda: 0) #should set all counts to 1 as default
    self.total = 0
    self.train(corpus)


  def train(self, corpus):
    """ Takes a corpus and trains your language model. 
        Compute any counts or other corpus statistics in this function.
    """  
    for sentence in corpus.corpus:
      for datum in sentence.data:
        token = datum.word
        self.unigramCounts[token] = self.unigramCounts[token] + 1
        self.total += 1
        if not token in self.uniqueWords:
            self.uniqueWords.add(token)

    pass


  def score(self, sentence):
    """ Takes a list of strings as argument and returns the log-probability of the
        sentence using your language model. Use whatever data you computed in train() here.
    """
    score = 0.0
    v = len(self.uniqueWords)
    for token in sentence:

        uCount = self.unigramCounts[token]
        if not token in self.uniqueWords:
            uCount = 0
        score += math.log(uCount + 1)


    score -= (math.log(self.total + v) * len(sentence))
    return score
