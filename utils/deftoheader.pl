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

print OUTFILE "char *$ARGV[2]\[\] = {\n";
while (<INFILE>) {
	chomp;
	s/\"/\\\"/g;
	print OUTFILE "\"$_\\n\",\n";
}
print OUTFILE "};\n";
print "$. lines processed\n";
close(INFILE);
close(OUTFILE);
