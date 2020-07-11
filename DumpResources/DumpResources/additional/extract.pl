#!/usr/bin/perl

use File::Find;
use JSON::PP;
use List::MoreUtils qw(uniq);
use strict;
use Data::Dumper;
use warnings;

#/mnt/i/atlas/ATLAS_DevKit_v12.0/Projects/ShooterGame/Content/Maps/SeamlessTest

@ARGV = qw(.) unless @ARGV;
my %maps;

print "Loading map resources\n";
find sub {
    if ( $File::Find::name =~ /umap$/ ) {
        $File::Find::name =~ /([A-Za-z0-9_]+)\.umap$/;
        my $name = $1;
        print "$1\n";
        my $file = $File::Find::name;
        open( my $fh, "<", $file );
        my $mapFile = do { local $/; <$fh> };

        $mapFile =~ s/Atlas//gi;
        $mapFile =~ s/Giant//gi;
        $mapFile =~ s/Wild//gi;
        close $fh;
 
        # load dino
        my @templateList;
        my @dinoTemplates = $mapFile =~ /([A-Za-z0-9_]+)_Character_BP/g;
        foreach my $dinoTemplate (@dinoTemplates) {
            push @templateList,$dinoTemplate;
        }
        
        my @uniqort = uniq @templateList;
        $maps{$name}{overrides} = \@uniqort;
    }
}, $ARGV[0] . "/Content/Maps/SeamlessTest";

print "Save mapResources\n";
open(my $fh, ">", "mapResources.json") or die "cannot write mapResources.json";;
print $fh encode_json( \%maps );
close ($fh);


