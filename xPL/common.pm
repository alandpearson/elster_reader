package xPL::common;

use IO::Socket;
use IO::Select;
use Sys::Hostname;
#use Time::HiRes qw/ualarm/;

use strict;
use Exporter;
use vars qw($VERSION @ISA @EXPORT);

$VERSION = 1.00;
@ISA     = qw(Exporter);
#@ISA     = ("Exporter");
@EXPORT  = qw(
  $xpl_port $instance_name_length
  $xpl_end
  &xpl_find_ip
  &xpl_build_automatic_instance_id
  &xpl_build_id
  &xpl_trim_instance_name
  &xpl_open_socket
  &xpl_send_braodcast
  &xpl_send_message
  &xpl_get_message_elements
  &xpl_is_only_for_me &xpl_is_for_me
  &xpl_get_message
  &xpl_send_heartbeat
  &xpl_disconnect
);


################################################################################
# Exported constants
#
our $xpl_port = 3865;
our $instance_name_length = 16; # instance names have max. 16 chars
our $xpl_end = 0;


################################################################################
# Exported functions: utilities
#

#-------------------------------------------------------------------------------
# Find machine IP address
#
sub xpl_find_ip {
	my ($host_name) = @_;

  my $xpl_ip = inet_ntoa((gethostbyname(hostname))[4]);

	return $xpl_ip;
}

#-------------------------------------------------------------------------------
# Build automatic instance name
#
sub xpl_build_automatic_instance_id {

  my $host_name = Sys::Hostname::hostname;
  my $automatic_instance_id = substr($host_name, 0, $instance_name_length);

	return $automatic_instance_id;
}

#-------------------------------------------------------------------------------
# Build xPL id
#
sub xpl_build_id {
	my ($vendor_id, $device_id, $instance_id) = @_;

  my $xpl_id = $vendor_id . '-' .
               $device_id . '.' .
               xpl_trim_instance_name($instance_id);

	return $xpl_id;
}

#-------------------------------------------------------------------------------
# Trim instance name to valid characters and max length
#
sub xpl_trim_instance_name {
	my ($instance_name) = @_;
                                                    # replace invalid characters
	$instance_name =~ s/(-|\.|!|;)//g;
                                                            # trim to max length
	if (length($instance_name) > $instance_name_length) {
		$instance_name = substr($instance_name, 0, $instance_name_length);
	}

	return $instance_name;
}

#-------------------------------------------------------------------------------
# Open an UDP socket to the xPL hub
#
sub xpl_open_socket {
	my ($xpl_port, $client_base_port) = @_;
                                                      # start on base port port
  my $client_port = $client_base_port;
  my $xpl_socket;
                                                             # open a free port
  while (!$xpl_socket && $client_port < $client_base_port+1000) {
    $xpl_socket = IO::Socket::INET->new(
      Broadcast => 1,
      PeerPort => $xpl_port,
      LocalPort => $client_port,
      Proto     => 'udp'
    );

    if (!$xpl_socket) {
      $client_port = $client_port + 1;
    }
  }
  die "Could not create socket: $!\n" unless $xpl_socket;

  return ($client_port, $xpl_socket);
}

#-------------------------------------------------------------------------------
# Send UDP message to broadcast address
#
sub xpl_send_braodcast {
	my ($xpl_socket, $xpl_port, $message) = @_;
                                                        # send broadcast message
  my $ipaddr   = INADDR_BROADCAST;
  my $portaddr = sockaddr_in($xpl_port, $ipaddr);
  $xpl_socket->autoflush(1);
  $xpl_socket->send($message, 0, $portaddr);
}

#-------------------------------------------------------------------------------
# Send xPL message to broadcast address
#
sub xpl_send_message {
	my (
	  $xpl_socket, $xpl_port,
	  $type, $source, $target, $class,
	  %body
  ) = @_;
                                                             # build xPL message
  my $message = "$type\n";
  $message .= "{\n";
  $message .= "hop=1\n";
  $message .= "source=$source\n";
  $message .= "target=$target\n";
  $message .= "}\n";
  $message .= "$class\n";
  $message .= "{\n";
  foreach my $item (keys(%body)) {
    $message .= "$item=$body{$item}\n";
  }
  $message .= "}\n";
#print "$message\n";
                                                              # send xPL message
  xpl_send_braodcast($xpl_socket, $xpl_port, $message);
}

#-------------------------------------------------------------------------------
# Get xPL message constituting elements
#
sub xpl_get_message_elements {
	my ($message) = @_;
                                                               # trim line feeds
  $message =~ s/\r/\n/g;
  $message =~ s/\n+/\n/g;
                                                           # split into elements
  my ($type, $schema, $body) = split(/{/, $message);
  $type =~ s/\n//g;
  $type = lc($type);
  (my $source, $schema) = split(/}/, $schema);
  $schema =~ s/\n//g;
  $schema = lc($schema);
                                                                # process header
  $source =~ s/hop=\d+//i;
  my $target = $source;
  $source =~ s/.*source=//si;
  $source =~ s/\n.*//s;
  $target =~ s/.*target=//si;
  $target =~ s/\n.*//s;
                                                                  # process body
  $body =~ s/}//;
  $body =~ s/\A\n*//s;
  $body =~ s/\n*\Z//s;
  $body =~ s/=/\n/sg;
  my @body = split(/\n/, $body);
  my %body = @body;
                                                               # return elements
  return ($type, $source, $target, $schema, %body);
}

#-------------------------------------------------------------------------------
# Check if xPL message is for the client
#
sub xpl_is_only_for_me {
	my ($xpl_id, $target) = @_;
                                                            # check target field
  my $matches = 0;
  if ($target eq $xpl_id) {
    $matches = 1;
  }
                                                                    # read value

  return ($matches);
}

sub xpl_is_for_me {
	my ($xpl_id, $target) = @_;
                                                            # check target field
  my $matches = 0;
  if ( ($target eq $xpl_id) or ($target eq '*') ) {
    $matches = 1;
  }
                                                                    # read value

  return ($matches);
}

################################################################################
# Exported functions: main program
#

#-------------------------------------------------------------------------------
# Get new xPl message with timeout
#
sub xpl_get_message {
	my ($xpl_socket, $timeout) = @_;
                                                    # read message from UDP port
  my $message;
  my $source_address;
  eval {
    local $SIG{ALRM} = sub { die "starting alarm time out" };
    alarm $timeout;
    $source_address = recv($xpl_socket, $message, 1500, 0);
    (my $port, $source_address) = sockaddr_in($source_address);
    chomp($message);
    $source_address = inet_ntoa($source_address);
    alarm 0;
    1;  # return value from eval on normalcy
  } or  $message = '';
                                                                # return message
  return ($message, $source_address);
}

#-------------------------------------------------------------------------------
# Check for elapsed time and send heartbeat
#
sub xpl_send_heartbeat {
	my ($xpl_socket, $xpl_id, $xpl_ip, $client_port,
	    $heartbeat_interval, $last_heartbeat_time) = @_;
                                                            # check elapsed time
  my $elapsed_time = (time - $last_heartbeat_time) / 60;
                                                        # send heartbeat message
  if ($elapsed_time >= $heartbeat_interval) {
#print "Sending heartbeat\n";
    xpl_send_message(
      $xpl_socket, $xpl_port,
      'xpl-stat', $xpl_id, '*', 'hbeat.app',
      (
        'interval'  => $heartbeat_interval,
        'remote-ip' => $xpl_ip,
        'port'      => $client_port
      )
    );
    $last_heartbeat_time = time;
  }
                                                    # return last heartbeat time
  return ($last_heartbeat_time);
}

#-------------------------------------------------------------------------------
# Send disconnect (heartbeat) message
#
sub xpl_disconnect {
	my ($xpl_socket, $xpl_id, $xpl_ip, $client_port) = @_;
                                             # send disconnext heartbeat message
  xpl_send_message(
    $xpl_socket, $xpl_port,
    'xpl-stat', $xpl_id, '*', 'hbeat.end',
    (
      'port'      => $client_port,
      'remote-ip' => $xpl_ip
    )
  );
                                                                  # close socket
  close($xpl_socket);
}

1;
