"""
pprof-latency.py -- process Crossfire client latency profile
"""
import sys

def main():
    data = sys.stdin.readlines()
    fields = map(lambda l: l.strip().split(','), data)
    pending = {}
    for f in fields:
        if f[0] == 'profile/com':
            n, t, cmd = f[1:]
            n = int(n)
            t = int(t)
            pending[n] = (t, cmd)
        elif f[0] == 'profile/comc':
            n, t, s, _ = f[1:]
            n = int(n)
            t = int(t)
            tdiff = t - pending[n][0]
            cmd = pending[n][1]
            del pending[n]
            print("%d,%s" % (tdiff, cmd))

if __name__ == '__main__':
    main()
