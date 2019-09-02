#!/usr/bin/perl
use JSON::PP;
use Data::Dumper;
use List::MoreUtils qw(uniq);

sub WorldToGPS {
  my ($x, $y) = @_;
  my $long = (($x / 21000000) * 200) - 100;
  my $lat = 100 - (($y / 21000000) * 200);
  return ($long,$lat);
}

sub GPSToWorld {
  my ($x, $y) = @_;
  my $long = ( ( $x + 100 ) / 200 ) * 21000000;
  my $lat =  ( (-$y + 100 ) / 200  ) * 21000000;
  return ($long,$lat);
}

open( my $fh, "<", "ServerGrid.json" );
my $serverConfig = decode_json(
    do { local $/; <$fh> }
);
close $fh;

open( my $fh, "<", "mapResources.json" );
my $mapResources = decode_json(
    do { local $/; <$fh> }
);
close $fh;

my %overrides;
for (my $x = 0; $x < 15; $x++) {
    for (my $y = 0; $y < 15; $y++) {
        my $grid = chr( 65 + $x ) . ( 1 + $y );
        open( my $fh, "<", "./resources/$grid.json" ) or next;
        $overrides{$grid} = decode_json(
            do { local $/; <$fh> }
        );
        close $fh;
    }
}

my %key_islandID;
foreach $server ( @{ $serverConfig->{'servers'} } ) {
    foreach $island ( @{ $server->{'islandInstances'} } ) {
        my $grid = chr( 65 + $server->{gridX} ) . ( 1 + $server->{gridY} );
        $island->{grid} = $grid;
        $key_islandID{ $island->{id} } = $island;
        $key_islandID{ $island->{id} }->{homeServer} = $server->{isHomeServer};

        if ($server->{OverrideShooterGameModeDefaultGameIni}->{bDontUseClaimFlags} == undef && $server->{name} !~ /Freeport/) {
            $key_islandID{ $island->{id} }->{claimable} = 1;
        } else {
            $key_islandID{ $island->{id} }->{claimable} = 0;
        }

        # get resources
        foreach my $key (keys %{ $overrides{$grid}{"Resources"} } ) {
            my @coords = GPSToWorld(split(/:/, $key));
            if (
                inside(
                    $island->{worldX} - ($island->{islandHeight} / 2),
                    $island->{worldY} - ($island->{islandHeight} / 2),
                    $island->{worldX} + ($island->{islandHeight} / 2),
                    $island->{worldY} + ($island->{islandHeight} / 2),
                    $coords[0],
                    $coords[1],
                )
              )
            {
                $key_islandID{ $island->{id} }->{resources} = $overrides{$grid}{"Resources"}{$key};
            }   
        }
        foreach my $disco ( @{ $server->{'discoZones'} } ) {
            if (
                $disco->{bIsManuallyPlaced} == JSON::PP::false &&
                inside(
                    $island->{worldX} - ($island->{islandHeight} / 2),
                    $island->{worldY} - ($island->{islandHeight} / 2),
                    $island->{worldX} + ($island->{islandHeight} / 2),
                    $island->{worldY} + ($island->{islandHeight} / 2),
                    $disco->{worldX},
                    $disco->{worldY}
                )
              )
            {
                my @coords = WorldToGPS($disco->{worldX}, $disco->{worldY});
                push @{ $key_islandID{ $island->{id} }->{discoveries} }, 
                { 
                    name => $disco->{name}, 
                    long => $coords[0], 
                    lat => $coords[1], 
                };
            }

            if ($overrides{$grid}{"Discoveries"}{$disco->{ManualVolumeName}}) {
                my @gps = @{$overrides{$grid}{"Discoveries"}{$disco->{ManualVolumeName}}};
                my @coords = GPSToWorld(@gps);
                if (
                    inside(
                        $island->{worldX} - ($island->{islandHeight} / 1.8),
                        $island->{worldY} - ($island->{islandWidth} / 1.8),
                        $island->{worldX} + ($island->{islandHeight} / 1.8),
                        $island->{worldY} + ($island->{islandWidth} / 1.8),
                        $coords[0],
                        $coords[1],
                    )
                )
                {
                    push @{ $key_islandID{ $island->{id} }->{discoveries} }, 
                    { 
                        name => $disco->{name}, 
                        long => $overrides{$grid}{"Discoveries"}{$disco->{ManualVolumeName}}[0], 
                        lat => $overrides{$grid}{"Discoveries"}{$disco->{ManualVolumeName}}[1], 
                    };
                }
            }
        }
    }

    foreach my $sublevel ( @{ $server->{'sublevels'} } ) {
        push(
            @{ $key_islandID{ $sublevel->{id} }->{sublevels} },
            $sublevel->{name}
        );
        if ( $mapResources->{ $sublevel->{name} } ) {
            if ( $mapResources->{ $sublevel->{name} }->{overrides} ) {
                foreach my $resource (
                    @{ $mapResources->{ $sublevel->{name} }->{overrides} } )
                {
                    push @{ $key_islandID{ $sublevel->{id} }->{animals} },
                      $resource;
                }
            }            
        }
       
        @{ $key_islandID{ $sublevel->{id} }->{animals} } =
          uniq  @{ $key_islandID{ $sublevel->{id} }->{animals} };
    }

}

my $json = JSON::PP->new->ascii->pretty->allow_nonref;
open( my $fh, ">", "islandList.json" ) or die "cannot write islandList.json";
print $fh $json->encode( \%key_islandID );
close($fh);

my %key_grid;
foreach my $island (keys %key_islandID)
{
    foreach my $resource (  keys %{ $key_islandID{ $island }->{resources} }  )
    {
        push(@{$key_grid{$key_islandID{$island}->{grid}}->{resources}}, $resource);
        @{ $key_grid{ $key_islandID{$island}->{grid}}->{resources} } = uniq sort @{$key_grid{$key_islandID{$island}->{grid}}->{resources} };
    }
    foreach my $resource ( @{$key_islandID{ $island }->{animals}} )
    {
        push(
            @{$key_grid{$key_islandID{$island}->{grid}}->{animals}}, 
            $resource);
        @{ $key_grid{$key_islandID{$island}->{grid}}->{animals} } =
        uniq sort @{ $key_grid{$key_islandID{$island}->{grid}}->{animals} };
    }
    $key_grid{$key_islandID{$island}->{grid}}->{claimable} += $key_islandID{ $island }->{claimable};
}

open( my $fh, ">", "gridList.json" ) or die "cannot write gridList.json";
print $fh $json->encode( \%key_grid );
close($fh);

sub inside {
    ( $x1, $y1, $x2, $y2, $x, $y ) = @_;
    if (   $x > $x1
        && $x < $x2
        && $y > $y1
        && $y < $y2 )
    {
        return 1;
    }
    return 0;
}
