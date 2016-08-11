import sys
import os
import re
import pprint

email_ex = '((?:\w+\.)*(?:\w+))\s*(?:\(?at\)?|@|&#x40;|where)\s*(stanford|lcs|csl|cs|ogun|gradiance|robotics|graphics)\s*(?:\.|dot|;|dt|dom)\s*(edu|com|org|gov|uk|net).*'

ext_email_ex = '((?:\w+\.)*(?:\w+))\s*(?:\(?at\)?|@|&#x40;|where)\s*(robotics|graphics|lcs|csl|cs|ogun|gradiance)\s*(?:\.|dot|;|dt|dom)\s*(\w+)\s*(?:dot|;|\.|dt)\s*(edu|com|org|gov|uk|net).*'

pal_ex = '((?:\w+\.)*(?:\w+))\s*(?:\(?at\)?|@|&#x40;|where)\s*(robotics|graphics|lcs|csl|cs|ogun|gradiance)\s+(\w+)\s+(edu|com|org|gov|uk|net).*'

obfuscate_email = '<script>\s*obfuscate\s*\(\'(\w+)\.(\w+)\',\'(\w+)\'\);\s*<\/script>'

obfuscate_phone = '(?:\+[0-9])\(?([0-9]{3})\)?-?([0-9]{3})(?:\-)?([0-9]{4})'

ouster_case = '<td class="value">((?:\w+\.)*(?:\w+)) \(followed by (?:"|&ldquo;)(?:\(?at\)?|@|&#x40;|where)(\w+)\.(\w+)(?:"|&rdquo;)\)<\/td>'

ouster_two = '<td class="value">((?:\w+\.)*(?:\w+)) \(followed by (?:"|&ldquo;)(?:\(?at\)?|@|&#x40;|where)(robotics|graphics|lcs|csl|cs|ogun|gradiance)(?:\.|dot|;|dt|dom)(\w+)(?:dot|;|\.|dt)(edu|com|org|gov|uk|net)(?:"|&rdquo;)\)<\/td>'

phone_ex = '[^0-9]\(?([0-9]{3})\)?\s*-?\s*([0-9]{3})(?:-|\s+)([0-9]{4})[^0-9]'

phone_start = '^\(?([0-9]{3})\)?-?([0-9]{3})-([0-9]{4})'

def remove_symbols(line, symbol):
    count = line.count(symbol)
    if count > 8:
        line = line.replace(symbol, "")

    return line

"""
This function takes in a filename along with the file object (actually
a StringIO object at submission time) and
scans its contents against regex patterns. It returns a list of
(filename, type, value) tuples where type is either an 'e' or a 'p'
for e-mail or phone, and value is the formatted phone number or e-mail.
The canonical formats are:
     (name, 'p', '###-###-#####')
     (name, 'e', 'someone@something')

NOTE: You shouldn't need to worry about this, but just so you know, the
'f' parameter below will be of type StringIO at submission time. So, make
sure you check the StringIO interface if you do anything really tricky,
though StringIO should support most everything.
"""
def process_file(name, f):
    # note that debug info should be printed to stderr
    # sys.stderr.write('[process_file]\tprocessing file: %s\n' % (path))
    res = []
    if (name.isalpha()):
        for l in f:
            line = (l.lower())
            line = remove_symbols(line, '-')
            line = remove_symbols(line, ';')


            e_matches = re.findall(email_ex,line)

            for m in e_matches:
                if (m[0] != 'server'):
                    email = '%s@%s.%s' % m
                    res.append((name,'e',email))

            more_e_matches = re.findall(ext_email_ex,line)

            for m in more_e_matches:
                if (m[0] != 'server'):

                    email = '%s@%s.%s.%s' % m
                    res.append((name,'e',email))

            html_matches = re.findall(obfuscate_email,line)

            for m in html_matches:
                if (m[2] != 'server'):
                    email = "{}@{}.{}".format(m[2], m[0], m[1])
                    res.append((name,'e',email))

            oust_matches = re.findall(ouster_case,line)

            for m in oust_matches:
                if (m[0] != 'server'):
                    email = "%s@%s.%s" % m
                    res.append((name,'e',email))

            oust_more = re.findall(ouster_two,line)

            for m in oust_more:
                if (m[0] != 'server'):
                    email = "%s@%s.%s.%s" % m
                    res.append((name,'e',email))

            pal_matches = re.findall(pal_ex,line)
            for m in pal_matches:
                if (m[0] != 'server'):
                    if (m[2] != 'dt'):
                        email = "%s@%s.%s.%s" % m
                        res.append((name,'e',email))

            p_matches = re.findall(phone_ex, line)
            for n in p_matches:
                phone = '%s-%s-%s' % n
                res.append((name, 'p',phone))

            obfuscate_phone_matches = re.findall(obfuscate_phone, line)
            for n in obfuscate_phone_matches:
                phone = '%s-%s-%s' % n
                res.append((name, 'p',phone))

            phone_begin = re.findall(phone_start, line)
            for n in phone_begin:
                phone = '%s-%s-%s' % n
                res.append((name, 'p',phone))

    return res

"""
You should not need to edit this function, nor should you alter
its interface as it will be called directly by the submit script
"""
def process_dir(data_path):
    # get candidates
    guess_list = []
    for fname in os.listdir(data_path):
        if fname[0] == '.':
            continue
        path = os.path.join(data_path,fname)
        f = open(path,'r')
        f_guesses = process_file(fname, f)
        guess_list.extend(f_guesses)
    return guess_list

"""
You should not need to edit this function.
Given a path to a tsv file of gold e-mails and phone numbers
this function returns a list of tuples of the canonical form:
(filename, type, value)
"""
def get_gold(gold_path):
    # get gold answers
    gold_list = []
    f_gold = open(gold_path,'r')
    for line in f_gold:
        gold_list.append(tuple(line.strip().split('\t')))
    return gold_list

"""
You should not need to edit this function.
Given a list of guessed contacts and gold contacts, this function
computes the intersection and set differences, to compute the true
positives, false positives and false negatives.  Importantly, it
converts all of the values to lower case before comparing
"""
def score(guess_list, gold_list):
    guess_list = [(fname, _type, value.lower()) for (fname, _type, value) in guess_list]
    gold_list = [(fname, _type, value.lower()) for (fname, _type, value) in gold_list]
    guess_set = set(guess_list)
    gold_set = set(gold_list)

    tp = guess_set.intersection(gold_set)
    fp = guess_set - gold_set
    fn = gold_set - guess_set

    pp = pprint.PrettyPrinter()
    #print 'Guesses (%d): ' % len(guess_set)
    #pp.pprint(guess_set)
    #print 'Gold (%d): ' % len(gold_set)
    #pp.pprint(gold_set)
    print 'True Positives (%d): ' % len(tp)
    pp.pprint(tp)
    print 'False Positives (%d): ' % len(fp)
    pp.pprint(fp)
    print 'False Negatives (%d): ' % len(fn)
    pp.pprint(fn)
    print 'Summary: tp=%d, fp=%d, fn=%d' % (len(tp),len(fp),len(fn))

"""
You should not need to edit this function.
It takes in the string path to the data directory and the
gold file
"""
def main(data_path, gold_path):
    guess_list = process_dir(data_path)
    gold_list =  get_gold(gold_path)
    score(guess_list, gold_list)

"""
commandline interface takes a directory name and gold file.
It then processes each file within that directory and extracts any
matching e-mails or phone numbers and compares them to the gold file
"""
if __name__ == '__main__':
    if (len(sys.argv) != 3):
        print 'usage:\tSpamLord.py <data_dir> <gold_file>'
        sys.exit(0)
    main(sys.argv[1],sys.argv[2])
