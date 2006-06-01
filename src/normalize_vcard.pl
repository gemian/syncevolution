#! /usr/bin/perl -w

use strict;
use encoding 'utf8';

# ignore differences caused by sync.scheduleworld.com?
my $scheduleworld = 1;

sub Usage {
  print "$0 <vcards.vcf\n";
  print "   normalizes one file (stdin or single argument), prints to stdout\n";
  print "$0 vcards1.vcf vcards2.vcf\n";
  print "   compares the two files\n";
  print "Also works for iCalendar files.\n";
}

sub Normalize {
  my $in = shift;
  my $out = shift;
  my $width = shift;

  $_ = join( "", <$in> );
  s/\r//g;

  my @items = ();

  foreach $_ ( split( /(?:(?<=\nEND:VCARD)|(?<=\nEND:VCALENDAR))\n*/ ) ) {
    # undo line continuation
    s/\n\s//gs;
    # ignore charset specifications, assume UTF-8
    s/;CHARSET="?UTF-8"?//g;

    # UID may differ, but only in vCards
    s/(VCARD.*)^UID:[^\n]*\n/$1/msg;

    # the distinction between an empty and a missing property
    # is vague and handled differently, so ignore empty properties
    s/^[^;:]*:\n//mg;

    # use separate TYPE= fields
    while( s/^(\w*)([^:\n]*);TYPE=(\w*),(\w*)/$1$2;TYPE=$3;TYPE=$4/mg ) {}

    # replace parameters with a sorted parameter list
    s!^([^;:]*);(.*?):!$1 . ";" . join(';',sort(split(/;/, $2))) . ":"!meg;

    # Ignore "other" email, address and telephone type - this is
    # an Evolution specific extension which might not be preserved.
    s/^(ADR|EMAIL|TEL)([^:]*);TYPE=OTHER/$1$2/mg;
    # TYPE=PREF on the other hand is not used by Evolution, but
    # might be sent back.
    s/^(ADR|EMAIL|TEL)([^:]*);TYPE=PREF/$1$2/mg;
    # Evolution does not need TYPE=INTERNET for email
    s/^(EMAIL)([^:]*);TYPE=INTERNET/$1$2/mg;
    # ignore TYPE=PREF in address, does not matter in Evolution
    s/^((ADR|LABEL)[^:]*);TYPE=PREF/$1/mg;
    # ignore extra separators in multi-value fields
    s/^((ORG|N|(ADR[^:]*)):.*?);*$/$1/mg;
    # the type of certain fields is ignore by Evolution
    s/^X-(AIM|GROUPWISE|ICQ|YAHOO);TYPE=HOME/X-$1/gm;
    # Evolution ignores an additional pager type
    s/^TEL;TYPE=PAGER;TYPE=WORK/TEL;TYPE=PAGER/gm;
    # PAGER property is sent by Evolution, but otherwise ignored
    s/^LABEL[;:].*\n//mg;
    # TYPE=VOICE is the default in Evolution and may or may not appear in the vcard
    s/^TEL([^:]*);TYPE=VOICE,([^:]*):/TEL$1;TYPE=$2:/mg;
    s/^TEL([^:]*);TYPE=([^;:]*),VOICE([^:]*):/TEL$1;TYPE=$2$3:/mg;
    s/^TEL([^:]*);TYPE=VOICE([^:]*):/TEL$1$2:/mg;
    # don't care about the TYPE property of PHOTOs
    s/^PHOTO;(.*)TYPE=[A-Z]*/PHOTO;$1/mg;
    # encoding is not case sensitive, skip white space in the middle of binary data
    if (s/^PHOTO;.*?ENCODING=(b|B|BASE64).*?:\s*/PHOTO;ENCODING=B: /mgi) {
      while (s/^PHOTO(.*?): (\S+)[\t ]+(\S+)/PHOTO$1: $2$3/mg) {}
    }

    # remove fields which may differ
    s/^(PRODID|CREATED|LAST-MODIFIED):.*\r?\n?//gm;
    # remove optional fields
    s/^(METHOD|X-WSS-COMPONENT|X-WSS-LUID):.*\r?\n?//gm;

    if ($scheduleworld) {
      # does not preserve X-EVOLUTION-UI-SLOT=
      s/^(\w+)([^:]*);X-EVOLUTION-UI-SLOT=\d+/$1$2/mg;
    }


    my @formatted = ();

    # Modify lines to cover not more than
    # $width characters by folding lines (as done for the N or SUMMARY above),
    # but also indent each inner BEGIN/END block by 2 spaces
    # and finally sort the lines.
    # We need to keep a stack of open blocks in @formatted:
    # - BEGIN creates another open block
    # - END closes it, sorts it, and adds as single string to the parent block
    push @formatted, [];
    foreach $_ (split /\n/, $_) {
      if (/^BEGIN:/) {
        # start a new block
        push @formatted, [];
      }

      my $spaces = "  " x ($#formatted - 1);
      my $thiswidth = $width -1 - length($spaces);
      $thiswidth = 1 if $thiswidth <= 0;
      s/(.{$thiswidth})(?!$)/$1\n /g;
      s/^(.*)$/$spaces$1/mg;
      push @{$formatted[$#formatted]}, $_;

      if (/^\s*END:/) {
        my $block = pop @formatted;
        my $begin = shift @{$block};
        my $end = pop @{$block};

        # Keep begin/end as first/last line,
        # inbetween sort, but so that N or SUMMARY are
        # at the top. This ensures that the order of items
        # is the same, even if individual properties differ.
        # Also put indented blocks at the end, not the top.
        sub numspaces {
          my $str = shift;
          $str =~ /^(\s*)/;
          return length($1);
        }
        $_ = join("\n",
                  $begin,
                  sort( { $a =~ /^\s*(N|SUMMARY):/ ? -1 :
                          $b =~ /^\s*(N|SUMMARY):/ ? 1 :
                          ($a =~ /^\s/ && $b =~ /^\S/) ? 1 :
                          numspaces($a) == numspaces($b) ? $a cmp $b :
                          numspaces($a) - numspaces($b) }
                        @{$block} ),
                  $end);
        push @{$formatted[$#formatted]}, $_;
      }
    }

    push @items, ${$formatted[0]}[0];
  }

  print $out join( "\n\n", sort @items ), "\n";
}

# number of columns available for output:
# try tput without printing the shells error if not found,
# default to 80
my $columns = `which tput >/dev/null && tput cols`;
if ($? || !$columns) {
  $columns = 80;
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

  open(IN1, "<:utf8", $file1) || die "$file1: $!";
  open(IN2, "<:utf8", $file2) || die "$file2: $!";
  open(OUT1, ">:utf8", $normal1) || die "$normal1: $!";
  open(OUT2, ">:utf8", $normal2) || die "$normal2: $!";
  my $singlewidth = int(($columns - 3) / 2);
  $columns = $singlewidth * 2 + 3;
  Normalize(*IN1{IO}, *OUT1{IO}, $singlewidth);
  Normalize(*IN2{IO}, *OUT2{IO}, $singlewidth);
  close(IN1);
  close(IN2);
  close(OUT1);
  close(OUT2);

  # Produce output where each line is marked as old (aka remove) with o,
  # as new (aka added) with n, and as unchanged with u at the beginning.
  # This allows simpler processing below.
  $_ = `diff "--old-line-format=o %L" "--new-line-format=n %L" "--unchanged-line-format=u %L" "$normal1" "$normal2"`;
  my $res = $?;

  if ($res) {
    # fix confusing output like:
    # BEGIN:VCARD                             BEGIN:VCARD
    #                                      >  N:new;entry
    #                                      >  FN:new
    #                                      >  END:VCARD
    #                                      >
    #                                      >  BEGIN:VCARD
    # and replace it with:
    #                                      >  BEGIN:VCARD
    #                                      >  N:new;entry
    #                                      >  FN:new
    #                                      >  END:VCARD
    #
    # BEGIN:VCARD                             BEGIN:VCARD
    #
    # With the o/n/u markup this presents itself as:
    # u BEGIN:VCARD
    # n N:new;entry
    # n FN:new
    # n END:VCARD
    # n
    # n BEGIN:VCARD
    #

    while( s/^u BEGIN:(VCARD|VCALENDAR)\n((?:^n .*\n)+?)^n BEGIN:/n BEGIN:$1\n$2u BEGIN:/m) {}
    
    # same for the other way around
    while( s/^u BEGIN:(VCARD|VCALENDAR)\n((?:^o .*\n)+?)^o BEGIN:/o BEGIN:$1\n$2u BEGIN:/m) {}

    # split at end of each record
    my $spaces = " " x $singlewidth;
    foreach $_ (split /(?:(?<=. END:VCARD\n)|(?<=. END:VCALENDAR\n))(?:^. \n)*/m, $_) {
      # ignore unchanged records
      if (!length($_) || /^((u [^\n]*\n)*(u [^\n]*?))$/s) {
        next;
      }

      # make all lines equally long in terms of printable characters
      s/^(.*)$/$1 . (" " x ($singlewidth + 2 - length($1)))/gme;

      # convert into side-by-side output
      my @buffer = ();
      foreach $_ (split /\n/, $_) {
        if (/^u (.*)/) {
          print join(" <\n", @buffer), " <\n" if $#buffer >= 0;
          @buffer = ();
          print $1, "   ", $1, "\n";
        } elsif (/^o (.*)/) {
          # preserve in buffer for potential merging with "n "
          push @buffer, $1;
        } else {
          /^n (.*)/;
          # have line to be merged with?
          if ($#buffer >= 0) {
            print shift @buffer, " | ", $1, "\n";
          } else {
            print join(" <\n", @buffer), " <\n" if $#buffer >= 0;
            print $spaces, " > ", $1, "\n";
          }
        }
      }
      print join(" <\n", @buffer), " <\n" if $#buffer >= 0;
      @buffer = ();

      print "-" x $columns, "\n";
    }
  }

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

  Normalize($in, *STDOUT{IO}, $columns);
}
