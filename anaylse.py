import os

class CwndChange(object):
	"""docstring for CwndChange"""
	def __init__(self, t, old, new):
		super(CwndChange, self).__init__()
		self.t = float(t)
		self.old = int(old)
		self.new = int(new)
		self.diff = self.new - self.old
def analyse_file(filename):
	with open('result/cwnd/'+filename) as f:
		lines = f.readlines()
	cwnd_list = [ CwndChange(*x.split()) for x in lines ]
	for index in xrange(len(cwnd_list)-1):
		print cwnd_list[index+1].old,cwnd_list[index+1].t-cwnd_list[index].t 

# for filename in os.listdir('result/cwnd'):
# 	if ".cwnd" in filename:
# 		print filename
# 		analyse_file(filename)
analyse_file('10203_mice_1_to_10.1.4.2.cwnd')
		