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
my ($connector, $connect, $listener, $stream, $print_tweet);
my $cv = AnyEvent->condvar;

# receive updates from @following_ids
$listener = sub {
  AnyEvent::Twitter::Stream->new(
  consumer_key =>    $config->[0]{'config.json'}{twitter}{consumer_key},
  consumer_secret => $config->[0]{'config.json'}{twitter}{consumer_secret},
  token =>           $config->[0]{'config.json'}{twitter}{token},
  token_secret =>    $config->[0]{'config.json'}{twitter}{token_secret},
  method =>          "filter",	# "firehose" for everything, "sample" for sample timeline
  track =>           join(",", @{$config->[0]{'config.json'}{twitter}{track}}),
  #follow =>          join(",", @{$config->[0]{'config.json'}{twitter}{follow}}), # numeric IDs
  on_connect => sub {
    undef $connector;
    print "Connected\n";
  },
  on_tweet => sub {
    my $tweet = shift;
    $print_tweet->($tweet);
  },
  on_keepalive => sub {
    #warn "ping\n";
  },
  on_eof => sub {
    print "eof\n";
    $connect->();
  },
  on_error => sub {
    my $err = shift;
    warn "Error: $err\n";
    sleep 10;
    $connect->();
  },
  on_delete => sub {},
  timeout => 90,
  );
};

$connect = sub {
  print "Connecting...\n";
  $connector = AE::timer 0, 2, sub {
    $stream = $listener->();
  };
};

$print_tweet = sub {
  my $tweet = shift;

  if($tweet->{user}{screen_name} ne "") {
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

$cv = AnyEvent->condvar;
$connect->();
$cv->recv;

1;
