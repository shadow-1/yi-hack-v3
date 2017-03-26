#!/bin/sh

echo "Content-type: text/html"
echo ""

CONF_FILE="/home/app/system.conf"
HACK_INFO="/home/app/.hackinfo"

get_config()
{
	key=$1
	grep $1 $CONF_FILE | cut -d "=" -f2
}

get_info()
{
	key=$1
	grep $1 $HACK_INFO | cut -d "=" -f2
}
                
VAR_CAMERA=`get_info "CAMERA"`
VAR_PROXYCHAINSNG=`get_config "PROXYCHAINSNG"`
VAR_HTTPD=`get_config "HTTPD"`
VAR_TELNETD=`get_config "TELNETD"`
VAR_FTPD=`get_config "FTPD"`

cat << EOM1
<!DOCTYPE html>
<html lang="en">

<head>
	<meta charset="utf-8">
	<meta http-equiv="X-UA-Compatible" content="IE=edge">
	<meta name="viewport" content="width=device-width, initial-scale=1">
	<!-- The above 3 meta tags *must* come first in the head; any other head content must come *after* these tags -->

EOM1

echo "	<title>$VAR_CAMERA</title>"

cat << EOM2

	<!-- Bootstrap core CSS -->
	<link href="../css/bootstrap.min.css" rel="stylesheet">
	<!-- Bootstrap theme -->
	<link href="../css/bootstrap-theme.min.css" rel="stylesheet">
	<!-- IE10 viewport hack for Surface/desktop Windows 8 bug -->
	<link href="../css/ie10-viewport-bug-workaround.css" rel="stylesheet">
	<!-- Custom styles for this template -->
	<link href="../css/navbar.css" rel="stylesheet">

</head>

<body>

	<div class="container">

		<!-- Static navbar -->
		<nav class="navbar navbar-inverse">
			<div class="container-fluid">
				<div class="navbar-header">
					<button type="button" class="navbar-toggle collapsed" data-toggle="collapse" data-target="#navbar" aria-expanded="false" aria-controls="navbar">
						<span class="sr-only">Toggle navigation</span>
						<span class="icon-bar"></span>
						<span class="icon-bar"></span>
						<span class="icon-bar"></span>
					</button>
EOM2

echo "					<a class="navbar-brand">$VAR_CAMERA</a>"

cat << EOM3
				</div>
				<div id="navbar" class="navbar-collapse collapse">
					<ul class="nav navbar-nav">
						<li><a href="/cgi-bin/read_proxy_config.sh">ProxyChains-ng</a></li>
						<li class="active"><a href="/cgi-bin/read_config.sh">System Config</a></li>
						<li><a href="/cgi-bin/about.sh">About</a></li>
						<li><a href="/cgi-bin/reboot.sh">Reboot</a></li>
					</ul>
				</div><!--/.nav-collapse -->
			</div><!--/.container-fluid -->
		</nav>

		<div class="page-header">
			<h1>System Configuration</h1>
		</div>
		<form action="/cgi-bin/write_config.sh" method="POST">
			<div class="form-group">
				<label class="col-sm-3 control-label">ProxyChains-ng Enabled</label>
				<label class="radio-inline">
EOM3

if [[ $VAR_PROXYCHAINSNG == "yes" ]] ; then
	echo "					<input type="radio" name="PROXYCHAINSNG" id="PROXYCHAINSNG" value="yes" checked="checked"> Yes"
else
	echo "					<input type="radio" name="PROXYCHAINSNG" id="PROXYCHAINSNG" value="yes"> Yes"
fi

cat << EOM4
				</label>
				<label class="radio-inline">
EOM4

if [[ $VAR_PROXYCHAINSNG == "no" ]] ; then
	echo "					<input type="radio" name="PROXYCHAINSNG" id="PROXYCHAINSNG" value="no" checked="checked"> No"
else
	echo "					<input type="radio" name="PROXYCHAINSNG" id="PROXYCHAINSNG" value="no"> No"
fi

cat << EOM5
				</label>
			</div>

			<div class="form-group">
				<label class="col-sm-3 control-label">Web Server Enabled</label>
				<label class="radio-inline">
EOM5

if [[ $VAR_HTTPD == "yes" ]] ; then
	echo "					<input type="radio" name="HTTPD" id="HTTPD" value="yes" checked="checked"> Yes"
else
	echo "					<input type="radio" name="HTTPD" id="HTTPD" value="yes"> Yes"
fi

cat << EOM6
				</label>
				<label class="radio-inline">
EOM6

if [[ $VAR_HTTPD == "no" ]] ; then
	echo "					<input type="radio" name="HTTPD" id="HTTPD" value="no" checked="checked"> No"
else
	echo "					<input type="radio" name="HTTPD" id="HTTPD" value="no"> No"
fi

cat << EOM7
				</label>
			</div>

			<div class="form-group">
				<label class="col-sm-3 control-label">Telnet Server Enabled</label>
				<label class="radio-inline">
EOM7

if [[ $VAR_TELNETD == "yes" ]] ; then
	echo "					<input type="radio" name="TELNETD" id="TELNETD" value="yes" checked="checked"> Yes"
else
	echo "					<input type="radio" name="TELNETD" id="TELNETD" value="yes"> Yes"
fi

cat << EOM8
				</label>
				<label class="radio-inline">
EOM8

if [[ $VAR_TELNETD == "no" ]] ; then
	echo "					<input type="radio" name="TELNETD" id="TELNETD" value="no" checked="checked"> No"
else
	echo "					<input type="radio" name="TELNETD" id="TELNETD" value="no"> No"
fi

cat << EOM9
				</label>
			</div>

			<div class="form-group">
				<label class="col-sm-3 control-label">FTP Server Enabled</label>
				<label class="radio-inline">
EOM9

if [[ $VAR_FTPD == "yes" ]] ; then
	echo "					<input type="radio" name="FTPD" id="FTPD" value="yes" checked="checked"> Yes"
else
	echo "					<input type="radio" name="FTPD" id="FTPD" value="yes"> Yes"
fi

cat << EOM10
				</label>
				<label class="radio-inline">
EOM10

if [[ $VAR_FTPD == "no" ]] ; then
	echo "					<input type="radio" name="FTPD" id="FTPD" value="no" checked="checked"> No"
else
	echo "					<input type="radio" name="FTPD" id="FTPD" value="no"> No"
fi

cat << EOM11
				</label>
			</div>

			<input class="btn btn-primary" type="submit" value="Apply">
		</form>
	</div>
	
	<!-- Bootstrap core JavaScript
	================================================== -->
	<!-- Placed at the end of the document so the pages load faster -->
	<script src="https://ajax.googleapis.com/ajax/libs/jquery/1.12.4/jquery.min.js"></script>
	<!--<script>window.jQuery || document.write('<script src="../js/vendor/jquery.min.js"><\/script>')</script>-->
	<script src="../js/bootstrap.min.js"></script>
	<!-- IE10 viewport hack for Surface/desktop Windows 8 bug -->
	<script src="../js/ie10-viewport-bug-workaround.js"></script>
</body>

</html>
EOM11

