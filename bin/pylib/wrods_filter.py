# -*- coding: utf-8 -*-

dest_code = 'utf-8'
class words_filter_t:
    def __init__(self, words_list_):
        self.word_index = {}# {one word -> []}
        self.all_words  = {}
        for w in words_list_:
            word = unicode(w, dest_code, 'ignore')
            word_len = len(word)
            if word_len == 0:
                continue
            dest_array = self.word_index.get(word[0])
            if None == dest_array:
                dest_array = []
                #print(__name__, 'filer')
                #print(word[0])
                self.word_index[word[0]] = dest_array
            if word_len not in dest_array:
                dest_array.append(word_len)
            dest_array.sort(reverse=True)
            self.all_words[word] = True
        print(__name__, "load filter words %d"%(len(self.all_words)))
    def dump(self, ):
        print (self.word_index, self.all_words)

    def filter(self, src_, replace_ = '*'):
        ret = unicode('', dest_code, 'ignore')
        word_src = unicode(src_, dest_code, 'ignore')
        i = 0 
        while i < len(word_src):
            w = word_src[i]
            #print(__name__, 'filer')
            #print(w)
            dest_list = self.word_index.get(w)
            if None == dest_list:
                i += 1
                ret += w
                continue
            replace_num = 0
            for dest_len in dest_list:
                check_word = word_src[i:i + dest_len]
                #print('check_word', check_word)
                if None != self.all_words.get(check_word):
                    i += dest_len
                    #print('del', check_word)
                    replace_num = len(check_word)#len(check_word.encode(dest_code))
                    break
            if replace_num == 0:
                ret += w
                i += 1
            else:
                ret += replace_ * replace_num
        return ret.encode(dest_code)

word_filter = None
def init(src_file_):
    global word_filter
    wlist = []
    f = open(src_file_)
    for line in f.readlines():
        tmp = line.split('\n')
        wlist.append(tmp[0])
    word_filter = words_filter_t(wlist)

def filter(src, replace_ = '*'):
    global word_filter
    return word_filter.filter(src, replace_)


#init('fw.txt')

