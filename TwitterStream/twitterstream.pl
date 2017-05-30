#!/usr/bin/perl -CS
# cpan install AnyEvent::Twitter AnyEvent::Twitter::Stream
# cpan install Net::OAuth
# cpan install Config::Any
use AnyEvent;
use AnyEvent::Twitter;
use AnyEvent::Twitter::Stream;
use JSON;
use utf8;
use strict;
use IO::Socket::INET;
use Config::Any;

# Find id with https://tweeterid.com/

my @configfiles = ( 'config.json' );
my $config = Config::Any->load_files( { files => \@configfiles , use_ext => 1 } );
my ($print_tweet);
my ($connector_track, $connect_track, $listener_track, $stream_track, $print_tweet_track);
my ($connector_follow, $connect_follow, $listener_follow, $stream_follow, $print_tweet_follow);
my $cv = AnyEvent->condvar;

# receive updates from @following_ids
$listener_track = sub {
  AnyEvent::Twitter::Stream->new(
  consumer_key =>    $config->[0]{'config.json'}{twitter}{consumer_key},
  consumer_secret => $config->[0]{'config.json'}{twitter}{consumer_secret},
  token =>           $config->[0]{'config.json'}{twitter}{token},
  token_secret =>    $config->[0]{'config.json'}{twitter}{token_secret},
  method =>          "filter",	# "firehose" for everything, "sample" for sample timeline
  track =>           join(",", @{$config->[0]{'config.json'}{twitter}{track}}),
  #follow =>          join(",", @{$config->[0]{'config.json'}{twitter}{follow}}), # numeric IDs
  on_connect => sub {
    undef $connector_track;
    print "Connected (track)\n";
  },
  on_tweet => sub {
    my $tweet = shift;
    $print_tweet_track->($tweet);
  },
  on_keepalive => sub {
    #warn "ping\n";
  },
  on_eof => sub {
    print "eof\n";
    $connect_track->();
  },
  on_error => sub {
    my $err = shift;
    warn "Error: $err\n";
    sleep 10;
    sleep rand(60);
    $connect_track->();
  },
  on_delete => sub {},
    timeout => 90,
  );
};
$connect_track = sub {
  print "Connecting (track)...\n";
  $connector_track = AE::timer 0, 2, sub {
    $stream_track = $listener_track->();
  };
};


$listener_follow = sub {
  AnyEvent::Twitter::Stream->new(
  consumer_key =>    $config->[0]{'config.json'}{twitter}{consumer_key},
  consumer_secret => $config->[0]{'config.json'}{twitter}{consumer_secret},
  token =>           $config->[0]{'config.json'}{twitter}{token},
  token_secret =>    $config->[0]{'config.json'}{twitter}{token_secret},
  method =>          "filter",	# "firehose" for everything, "sample" for sample timeline
  #track =>           join(",", @{$config->[0]{'config.json'}{twitter}{track}}),
  follow =>          join(",", @{$config->[0]{'config.json'}{twitter}{follow}}), # numeric IDs
  on_connect => sub {
    undef $connector_follow;
    print "Connected (follow)\n";
  },
  on_tweet => sub {
    my $tweet = shift;
    $print_tweet_follow->($tweet);
  },
  on_keepalive => sub {
    #warn "ping\n";
  },
  on_eof => sub {
    print "eof\n";
    $connect_follow->();
  },
  on_error => sub {
    my $err = shift;
    warn "Error: $err\n";
    sleep 10;
    sleep rand(60);
    $connect_follow->();
  },
  on_delete => sub {},
    timeout => 90,
  );
};
$connect_follow = sub {
  print "Connecting (follow)...\n";
  $connector_follow = AE::timer 0, 2, sub {
    $stream_follow = $listener_follow->();
  };
};


$print_tweet_follow = sub {
  my $tweet = shift;
  return if exists $tweet->{in_reply_to_status_id};	# no reples
  return if exists $tweet->{retweeted_status};		# no retweets
  $print_tweet->($tweet);
};
$print_tweet_track = sub {
  my $tweet = shift;
  return if exists $tweet->{retweeted_status};		# no retweets
  $print_tweet->($tweet);
};

$print_tweet = sub {
  my $tweet = shift;
  if($tweet->{user}{screen_name} eq "") { return; };
  my $json_output = to_json($tweet, {utf8 => 1});
  print "\n";
#  print $json_output;
#  print "\n";
  print "\n";

  my $sock = new IO::Socket::INET(
    PeerAddr => $config->[0]{'config.json'}{display}{ip},
    PeerPort => $config->[0]{'config.json'}{display}{port},
    Proto => 'udp',
    Timeout => 1
  ) or die('Error opening socket.');
  my $message = $tweet->{text};
  print $sock $tweet->{user}{screen_name} . ": " . $message . "\n";
  print $tweet->{user}{screen_name} . ": " . $message . "\n";

};



my $ua = AnyEvent::Twitter->new(
  consumer_key => $config->[0]{'config.json'}{twitter}{consumer_key},
  consumer_secret => $config->[0]{'config.json'}{twitter}{consumer_secret},
  token => $config->[0]{'config.json'}{twitter}{token},
  token_secret => $config->[0]{'config.json'}{twitter}{token_secret}
);

sub getinfo_byid {
  my $id = shift;
  my ($header, $response, $reason);
  $cv->begin;
  $ua->request(
  method => 'GET',
  api    => 'users/show',
  params => { 'user_id' => $id },
  sub {
    ($header, $response, $reason) = @_;
    $cv->end;
  });
  $cv->recv;
  return ($response->{screen_name}, $response->{name}, $response->{id_str});
};


print "Display address: " . $config->[0]{'config.json'}{display}{ip} . ":" . $config->[0]{'config.json'}{display}{port} . "\n";
print "Following users:\n";

foreach (@{$config->[0]{'config.json'}{twitter}{follow}}) {
  $cv = AnyEvent->condvar;
  my ($screen_name, $name, $id_str) = getinfo_byid($_);
  print "  https://twitter.com/$screen_name \"$name\" (id: $id_str)\n";
}

print "Following hashtags:\n";
foreach (@{$config->[0]{'config.json'}{twitter}{track}}) {
  my $urltag = reverse($_);
  chop($urltag);
  $urltag = reverse($urltag);

  print "  $_\thttps://twitter.com/hashtag/$urltag\n";
}


$cv = AnyEvent->condvar;
$connect_track->();
$connect_follow->();
$cv->recv;

1;
