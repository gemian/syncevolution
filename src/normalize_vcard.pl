#! /usr/bin/perl -w

use strict;

sub Usage {
  print "normalize_vcard <vcards.vcf\n";
  print "                 vcards1.vcf vcards2.vcf\n\n";
  print "Either normalizes one file (stdin or single argument)\n";
  print "or compares the two files.\n";
}

sub Normalize {
  my $in = shift;
  my $out = shift;

  $_ = join( "", grep( !/^(BEGIN:VCARD|BEGIN:VCALENDAR|VERSION|END:VCARD|END:VCALENDAR|UID:)/, <$in> ) );
  s/\r//g;

  my @cards = ();

  foreach $_ ( split( /\n\n/ ) ) {
    # undo line continuation
    s/\n\s//gs;
    # ignore charset specifications, assume UTF-8
    s/;CHARSET="UTF-8"//g;

    # ignore extra email type
    s/^EMAIL;.*TYPE=INTERNET/EMAIL$1/mg;
    s/^EMAIL;TYPE=OTHER/EMAIL$1/mg;
    # ignore extra ADR type
    s/^ADR;TYPE=OTHER/ADR/mg;
    # ignore TYPE=PREF in address, does not matter in Evolution
    s/^((ADR|LABEL)[^:]*);TYPE=PREF/$1/mg;
    # ignore extra separators in multi-value fields
    s/^((ORG|N|(ADR[^:]*)):.*?);*$/$1/mg;
    # the type of certain fields is ignore by Evolution
    s/^X-(AIM|GROUPWISE|ICQ|YAHOO);TYPE=HOME/X-$1/gm;
    # TYPE=VOICE is the default in Evolution and may or may not appear in the vcard
    s/^TEL([^:]*);TYPE=VOICE([^:]*):/TEL$1$2:/mg;
    # don't care about the TYPE property of PHOTOs
    s/^PHOTO;(.*)TYPE=[A-Z]*/PHOTO;$1/mg;
    # encoding is not case sensitive
    s/^PHOTO;(.*)ENCODING=b/PHOTO;$1ENCODING=B/mg;

    # remove extra timezone specification, it is not supported
    # by SyncEvolution
    s/BEGIN:VTIMEZONE.*?END:VTIMEZONE\r?\n?//gs;
    # remove fields which may differ
    s/^(PRODID|CREATED|LAST-MODIFIED):.*\r?\n?//gm;
    # remove optional fields
    s/^(METHOD):.*\r?\n?//gm;

    # replace parameters with a sorted parameter list
    s!^([^;:]*);(.*?):!$1 . ";" . join(';',sort(split(/;/, $2))) . ":"!meg;


    # sort entries, putting "N:" resp. "SUMMARY:" first
    # then modify entries to cover not more than
    # 60 characters
    my @lines = split( "\n" );
    push @cards, join( "\n",
                       grep( ( s/(.{60})/$1\n /g || 1),
                             grep( /^(N|SUMMARY):/, @lines ),
                             sort( grep ( !/^(N|SUMMARY):/, @lines ) ) ) );
  }

  print $out join( "\n\n", sort @cards ), "\n";
}

if($#ARGV > 1) {
  # error
  Usage();
  exit 1;
} elsif($#ARGV == 1) {
  # comparison

  my ($file1, $file2) = ($ARGV[0], $ARGV[1]);
  my $normal1 = `mktemp`;
  my $normal2 = `mktemp`;
  chomp($normal1);
  chomp($normal2);

  open(IN1, "<$file1") || die "$file1: $!";
  open(IN2, "<$file2") || die "$file2: $!";
  open(OUT1, ">$normal1") || die "$normal1: $!";
  open(OUT2, ">$normal2") || die "$normal2: $!";
  Normalize(*IN1{IO}, *OUT1{IO});
  Normalize(*IN2{IO}, *OUT2{IO});
  close(IN1);
  close(IN2);
  close(OUT1);
  close(OUT2);

  my $res = system( "diff", "--suppress-common-lines", "-y", $normal1, $normal2 );
  
  unlink($normal1);
  unlink($normal2);
  exit($res ? 1 : 0);
} else {
  # normalize
  my $in;
  if( $#ARGV >= 0 ) {
    open(IN, "<$ARGV[0]") || die "$ARGV[0]: $!";
    $in = *IN{IO};
  } else {
    $in = *STDIN{IO};
  }

  Normalize($in, *STDOUT{IO});
}
