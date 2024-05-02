#!/usr/bin/perl -w


# Logger for Elster A1100 meter reader
#

use Device::SerialPort 0.12;
use warnings;   	# Generate warnings
use Sys::Hostname;  	# Module that provides host name.
use FindBin;                    # find the script's directory
use lib $FindBin::Bin;          # add that directory to the library path
use xPL::common;
use Net::MQTT::Simple;
use Net::MQTT::Simple::Auth;
use File::Basename ;
use POSIX  'strftime';

use Config::Simple;

my $cfg = new Config::Simple('/etc/elster.cfg') ;

my $mqtt ;
my $sel ;



#################################################################################################
# Shouldn't really touch anything under this point !						#
#################################################################################################

$ENV{MQTT_SIMPLE_ALLOW_INSECURE_LOGIN} = 1;

# XPL vars
my $separator = '-' x 80 ;
my $indent = ' ' x 2 ;


my %meterData = () ;
my $serialOb ;


sub openSerial {

	my $error = 0 ;

	if ( $serialOb = Device::SerialPort->new ($cfg->param('port')) ) {
	
		$serialOb->baudrate($cfg->param('baud')) ;
		$serialOb->parity($cfg->param('parity')) ;
		$serialOb->databits($cfg->param('databits')) ; 
		$serialOb->handshake("none") ;
		$serialOb->stopbits(1);
		$serialOb->write_settings or $error = 1 ;
	} else {
		# Couldn't open serial port; 
		logit ("***ERROR*** Elster Logger could not open serial port " . $cfg->param('port')) ;
		return -1 ;
	}

	if ( ! $error ) {
		open(SERIAL, "<", $cfg->param('port')) || die "Cannot open " . $cfg->param('port') . ": $!";
		logit("Elster Logger connected on " . $cfg->param('port')) ;
		return 0;
	} else {
		return -1 ;
	}

}





sub meterRead {
	
	# Get the running statistics
	my $data =  <SERIAL> ;
	if ($data ne EOF) {
		chop $data ; chop $data ; #remove CR/LF
		if ($data =~ /\d*\.?\d*:\d*\.?\d*:\d*\.?\d*/ ) {
			my @reading = split (':', $data) ;
			$meterData{IMPORTKWH} = $reading[0];
			$meterData{EXPORTKWH} = $reading[1];
			$meterData{STATE} = $reading[2];
			$meterData{MANF} = "Elster" ;
			$meterData{MODEL} = "A1100" ;
			$meterData{SERIAL} = $cfg->param('meter_serial') ;
			$rc=0;
		} else {
			logit("BAD DATA Received");
			$rc=1;
		}
	} else { 
		$rc=-1;
	}


	return($rc) ;	

}	



sub logCSV {

	my %pvData = @_ ;
	my $year = getTime_YYYY();
	
	# append filename
	my $csvfile = $cfg->param('csv_path')  . "/$year" . "/feedin_" . $pvData{SERIAL} . "_" . &getDate_YYYYMMDD() . ".csv";

	# Create a directory for the year if it doesn't yet exist
	# (check dirname of the directory of csvfiles and if it exists
	# then we can create a year directory within
	# or cheat and just mkdir $csvdir !
	my $csvdir = dirname $csvfile ;

	if ( ! -d $csvdir ) {
		if ( -d (dirname $csvdir) ) {
			warn "*** Year subdirectory will be created under $csvdir\n";
			mkdir $csvdir ;
		} else {
			warn "*** WARNING csv directory does not exist: $csvdir\n";
		}
	} 


	# open csvfile in 'append' mode
	if ( open(CSVFILE, ">>$csvfile") ) {
		#print ("Logging to: $csvfile");
		# add file header to csvfile (if file exists & is empty)
		if ( -z $csvfile ) {
			print CSVFILE "#DATE,TIMESTAMP,IMPORTKWH,EXPORTKWH,STATE\n" ;
		}


# write data to csvfile & close it
		my $unixTimeStamp = time;                           # secs since epoch
		my $dateTimeStr = &getDateTime($unixTimeStamp);
		my $csvLine = "$dateTimeStr,$unixTimeStamp,$pvData{IMPORTKWH},$pvData{EXPORTKWH},$pvData{STATE}";

		print CSVFILE "$csvLine\n";
		close (CSVFILE);

		# Log live state
		open (ELSTATE,">" . $cfg->param('state_file') ) || warn ("*** WARNING Could not open state file" . $cfg->param('state_file') . " $!\n" );
		print ELSTATE time() . "," .  eval($pvData{IMPORTKWH}) . ",$pvData{EXPORTKWH}" . ",$pvData{STATE}\n";
		close (ELSTATE) ;

	}
	else {
		warn "*** WARNING Could not open csvfile: $csvfile $!\n";
	}
} #end writeToFile

sub mqttSend {

	my %pvData = @_ ;

	foreach $key (keys %pvData) {
		$topic = $cfg->param('mqtt_topic');
		$mqtt->publish("$topic/$key" => $pvData{$key});
	}
}

sub xplSend {

	# Bleat via XPL

	my %pvData = @_ ;
	
	my $vendor_id = $cfg->param('xpl_src_vendor') ;
	my $client_base_port = $cfg->param('xpl_client_base_port') ;
	my $xpl_class = $cfg->param('xpl_msg_schema_class') . "." . $cfg->param('xpl_msg_schema_type') ;

	my $xpl_device_id = hostname ;
	my $xpl_type = "xpl-stat" ;
	my $instance_id = xpl_build_automatic_instance_id;
	my $xpl_source = $vendor_id . '-' . $xpl_device_id . '.' . xpl_trim_instance_name($instance_id);
	my $xpl_target = '*' ;
	my %xpl_body = () ;
	my $client_port = 0 ;
	my $xpl_socket ;


	($client_port, $xpl_socket) = xpl_open_socket($xpl_port, $client_base_port);
	$xpl_body{Inverter_Manufacturer} = $pvData{MANF} ;
	$xpl_body{Inverter_Model} = $pvData{MODEL} ;
	$xpl_body{Total_Kwh} = $pvData{ETOTAL} ;
	$xpl_body{Timestamp} = time() ;


	xpl_send_message( $xpl_socket, $xpl_port,
		$xpl_type, $xpl_source, $xpl_target, $xpl_class, %xpl_body
	);


	close($xpl_socket);


}
sub getTime_YYYY() {
	local($sec,$min,$hour,$dayOfMth,$monthOffset,$yearOffset,$dayOfWk,$dayOfYr,$isDST) = localtime;
	return sprintf("%4d", $yearOffset + 1900);
} #end getTime_YYYY

sub getDate_YYYYMMDD() {
	local($sec,$min,$hour,$dayOfMth,$monthOffset,$yearOffset,$dayOfWk,$dayOfYr,$isDST) = localtime;
	return sprintf("%04d%02d%02d", $yearOffset+1900, $monthOffset+1, $dayOfMth);
} 

sub getDateTime() {
	local($sec,$min,$hour,$dayOfMth,$monthOffset,$yearOffset,$dayOfWk,$dayOfYr,$isDST) = localtime;
	return sprintf("%02d/%02d/%04d %02d:%02d:%02d", $dayOfMth, $monthOffset+1, $yearOffset+1900, $hour, $min, $sec);
} #end getDateTime

sub logit {

        my $msg = "$_[0]";
        my $timestamp = strftime('%H:%M:%S', localtime) ;
        my $logfile = $cfg->param('log_dir') . "/elster_" . &getDate_YYYYMMDD() . ".log";

        if ( open(LOG, ">>$logfile") ) {
                print LOG "[$timestamp] $msg\n" ;
        } 
        close (LOG) ;
        return ;
}



#################################################################################################################
# main														#
#################################################################################################################


logit("Elster Logger started") ;

if ($cfg->param('mqtt_enabled') eq "true") {
	logit ("MQTT enabled");
	$mqtt = Net::MQTT::Simple::->new($cfg->param('mqtt_hostname') );
	$mqtt->login($cfg->param('mqtt_user'), $cfg->param('mqtt_pass')) ;
	if ( $mqtt ) {
		logit ("MQTT connected to broker " . $cfg->param('mqtt_hostname')) ;
	} else {
		logit ("MQTT ERROR could not connect to broker " . $cfg->param('mqtt_hostname')) ;
	}

} else {
	logit ("MQTT disabled by configuration");
}



my $lastMqttSend = time() ;
my $lastCSVWrite = time() ;


while (1) {


	if ( openSerial() == 0 ) {
		%meterData = () ;
		while (! meterRead() != -1 ) {
			if (%meterData) {
				if ( ($cfg->param('csv_enabled') eq "true") && ((time() - $lastCSVWrite) > $cfg->param('csv_write_secs')) ) {
					logCSV(%meterData) ;
					$lastCSVWrite = time() ;
				}

				if ($cfg->param('xpl_enabled') eq "true") {
					xplSend(%meterData);
				}

				if ($cfg->param('mqtt_enabled') eq "true") {
					if ( (time() - $lastMqttSend) > $cfg->param('mqtt_send_secs') ) {
						$lastMqttSend = time() ;
						mqttSend(%meterData);
					}
				}
			} else {
				#print "BAD data\n";
			}

			$mqtt->tick() ;
		} 
		#Bad data received
		#logit("Bad data received") ;
		#meterRead return -1, serial port bad
		$serialOb->close() ;
	} else {
		sleep (1) ;
	}
}


	return; 
