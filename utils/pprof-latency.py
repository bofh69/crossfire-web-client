"""
pprof-latency.py -- process Crossfire client latency profile
usage:
1. gather a profile using:

   $ crossfire-client-gtk2 --profile-latency > out.txt

2. process the profile:

   $ python3 utils/pprof-latency.py out.txt
"""
import sys

def main():
    if len(sys.argv) < 2:
        print("usage: python3 pprof-latency.py FILE")
        return

    path = sys.argv[1]
    with open(path, 'r') as dat, open(path + ".hist", 'w') as hist:
        data = dat.readlines()
        fields = map(lambda l: l.strip().split('\t'), data)
        pending = {}
        for f in fields:
            if f[0] == 'profile/com':
                n, cmd = f[1:]
                n = int(n)
                pending[n] = cmd
            elif f[0] == 'profile/comc':
                n, t, s, _ = f[1:]
                n = int(n)
                t = int(t)
                tdiff = t
                cmd = pending[n]
                del pending[n]
                print("%d,%s" % (tdiff, cmd))
                print("%d" % tdiff, file=hist)

if __name__ == '__main__':
    main()
