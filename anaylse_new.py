import os
import sys


class CwndChange(object):
    """docstring for CwndChange"""

    def __init__(self, t, old, new):
        super(CwndChange, self).__init__()
        self.t = float(t)
        self.old = int(old)
        self.new = int(new)
        self.diff = self.new - self.old


def analyse_cwnd(filename):
    with open('result/cwnd/' + filename) as f:
        lines = f.readlines()
    cwnd_list = [CwndChange(*x.split()) for x in lines]
    for index in xrange(len(cwnd_list) - 1):
        print cwnd_list[index + 1].old, cwnd_list[index + 1].t - cwnd_list[index].t


class FlowStats(object):
    """docstring for FlowStats"""

    def __init__(self, flow_id, flow_args):
        super(FlowStats, self).__init__()
        self.flow_id = flow_id
        self.src, self.dst, self.prio, self.size, self.start, self.end = tuple(
            flow_args)
        self.fct = None

    def __str__(self):
        return "%d, %d, %d, %d, %d, %f, %d, %d" % (self.flow_id, self.src, self.dst, self.prio, self.size, self.start, self.end, self.fct)
# for filename in os.listdir('result/cwnd'):
#   if ".cwnd" in filename:
#       print filename
#       analyse_file(filename)
# analyse_cwnd('10203_mice_1_to_10.1.4.2.cwnd')


def get_result(fn_trace, fn_fct):
    with open(fn_trace) as f:
        f_trace = f.readlines()
    with open(fn_fct) as f:
        f_fct = f.readlines()
    flow_list = [FlowStats(flow_id+1, map(eval, line.split()))
                 for flow_id, line in enumerate(f_trace[1:])]
    # print len(flow_list)
    for line in f_fct:
        # flow_list[int(line.split()[0]) - 10000].fct = float(line.split()[-1])
        try:
            flow_list[int(line[-6:]) - 10001].fct = int(line.split()[0][4:])
        except Exception as e:
            print len(flow_list),len(f_trace),fn_trace
            print line
            raise e
    return dict([[flow.flow_id, flow] for flow in flow_list if flow.fct and flow.start > 0.5])



def analyse_fct(flow_dict):
    print "\t".rjust(11), "count\t".rjust(11), "min\t".rjust(11), "max\t".rjust(11), "avg\t".rjust(11)
    mice_flow = [flow.fct for flow in flow_dict.values() if flow.prio == 2]
    mice_flow.sort()
    mice_count = len(mice_flow)
    mice_flow = mice_flow[mice_count / 20:-mice_count / 20]
    mice_count = len(mice_flow)
    print "%10s\t" % ("mice"),
    print "%10d\t" % (mice_count),
    print "%10d\t" % (mice_flow[0]),
    print "%10d\t" % (mice_flow[-1]),
    print "%10d\t" % (1.0 * sum(mice_flow) / mice_count)
    elephant_flow = [flow.fct for flow in flow_dict.values()  if flow.prio == 3]
    elephant_flow.sort()
    elephant_count = len(elephant_flow)
    elephant_flow = elephant_flow[elephant_count / 20:-elephant_count / 20]
    elephant_count = len(elephant_flow)
    print "%10s\t" % ("elephant"),
    print "%10d\t" % (elephant_count),
    print "%10d\t" % (elephant_flow[0]),
    print "%10d\t" % (elephant_flow[-1]),
    print "%10d\t" % (1.0 * sum(elephant_flow) / elephant_count)

    all_flow = mice_flow + elephant_flow
    all_flow.sort()
    all_count = mice_count + elephant_count
    print "%10s\t" % ("total"),
    print "%10d\t" % (all_count),
    print "%10d\t" % (all_flow[0]),
    print "%10d\t" % (all_flow[-1]),
    print "%10d\t" % (1.0 * sum(all_flow) / all_count)

def analyse_all(dn_trace, dn_runner, suffix):
    for offerload in xrange(1,11):
        print offerload
        analyse_fct(get_result(os.path.join(dn_trace,"offerload_%.1f.tr"%(offerload/10.0)),os.path.join(dn_runner,"%d%s"%(offerload, suffix),"ns3-fct-output.txt")))
    pass


if __name__ == '__main__':

    analyse_all("trace_0.1","runner_0.1","_15_delay_1us")
    # if len(sys.argv) != 3:
    #     print '''   Usage:
    #         python anaylse.py <trace_file> <fct_output>
    #     '''
    #     sys.exit(1)
    # flow_dict = get_result(sys.argv[1], sys.argv[2])
    # analyse_fct(flow_dict)
