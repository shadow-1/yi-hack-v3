#!/bin/sh

echo "Content-Type: text/html"
echo ""

BASE_VER="/home/homever"
HACK_INFO="/home/app/.hackinfo"

get_info()
{
	key=$1
	grep $1 $HACK_INFO | cut -d "=" -f2
}
		
VAR_CAMERA=`get_info "CAMERA"`
VAR_VERSION=`get_info "VERSION"`

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
						<li><a href="/cgi-bin/read_config.sh">System Config</a></li>
						<li class="active"><a href="/cgi-bin/about.sh">About</a></li>
						<li><a href="/cgi-bin/reboot.sh">Reboot</a></li>
					</ul>
				</div><!--/.nav-collapse -->
			</div><!--/.container-fluid -->
		</nav>

		<!-- Main component for a primary marketing message or call to action -->
		<div class="page-header">
			<h1>About yi-hack-v3</h1>
		</div>
		
		<p>Yi-hack-v3 unlocks features not available with the official Yi camera firmware. Features implemented include:</p>
		<ul>
			<li>Telnet Server</li>
			<li>FTP Server</li>
			<li>Web Server</li>
			<li>Proxychains-ng</li>
			<li>Shell script automatically executed from microSD card automatically upon boot</li>
		</ul>
		<p>Yi-hack-v3 currently supports the following cameras:</p>
		<ul>
			<li>Yi 1080p Dome</li>
			<li>Yi 1080p Home</li>
		</ul>
	
		<h2>Version</h2>
		<dl class="dl-horizontal">
			<dt>Camera</dt>
EOM3

echo "          <dd>$VAR_CAMERA</dd>"

cat << EOM4
			<dt>Base Firmware</dt>
EOM4

echo "         <dd>`cat $BASE_VER`</dd>"

cat << EOM5
			<dt>yi-hack-v3 Firmware</dt>
EOM5

echo "		<dd>$VAR_VERSION</dd>"

cat << EOM6
		</dl>
		
		<h2>More Information</h2>
		<a href="https://github.com/shadow-1/yi-hack-v3">https://github.com/shadow-1/yi-hack-v3</a>
			
	</div>

	<!-- Bootstrap core JavaScript
	================================================== -->
	<!-- Placed at the end of the document so the pages load faster -->
	<script src="https://ajax.googleapis.com/ajax/libs/jquery/1.12.4/jquery.min.js"></script>
	<!--<script>window.jQuery || document.write('<script src="js/vendor/jquery.min.js"><\/script>')</script>-->
	<script src="../js/bootstrap.min.js"></script>
	<!-- IE10 viewport hack for Surface/desktop Windows 8 bug -->
	<script src="../js/ie10-viewport-bug-workaround.js"></script>
</body>

</html>
EOM6
