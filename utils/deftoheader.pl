#!/usr/local/bin/perl
#
# This program takes some file and encapsulates it into a header file
# as a giant string.
#
# It takes three options <infile> <outfile> <enscapsulation string>.
# the encapsulation string is what string the script uses to encapsulate
# the data.
#

if ($#ARGV!=2) {
	print "Usage: $0 <infile> <outfile> <enscapsulation>\n";
	exit;
}

open(INFILE, "<$ARGV[0]") || die("can not open $ARGV[0]\n");
open(OUTFILE, ">$ARGV[1]") || die("can not open $ARGV[1]\n");

$LINES += tr/\n/\n/ while sysread(INFILE, $_, 2 ** 16);

if ( $ARGV[1] =~ m/.h$/) {
    print OUTFILE "extern char *$ARGV[2]\[" . $LINES . "\];\n";
} else {
    seek(INFILE, 0, 0);
    print OUTFILE "char *$ARGV[2]\[" . $LINES . "\] = {\n";
    while (<INFILE>) {
        chomp;
        s/\"/\\\"/g;
        print OUTFILE "\"$_\\n\",\n";
    }
    print OUTFILE "};\n";
}
print $LINES . " lines processed\n";
close(INFILE);
close(OUTFILE);

