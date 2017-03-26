#!/bin/sh

echo "Content-type: text/html"
echo ""

CONF_FILE="/home/app/proxychains.conf"
HACK_INFO="/home/app/.hackinfo"

get_info()
{
	key=$1
	grep $1 $HACK_INFO | cut -d "=" -f2
}
                                
VAR_CAMERA=`get_info "CAMERA"`
                                
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
						<li class="active"><a href="/cgi-bin/read_proxy_config.sh">ProxyChains-ng</a></li>
						<li><a href="/cgi-bin/read_config.sh">System Config</a></li>
						<li><a href="/cgi-bin/about.sh">About</a></li>
						<li><a href="/cgi-bin/reboot.sh">Reboot</a></li>
					</ul>
				</div><!--/.nav-collapse -->
			</div><!--/.container-fluid -->
		</nav>

		<div class="page-header">
			<h1>ProxyChains-ng Configuration</h1>
		</div>
		<form action="/cgi-bin/write_proxy_config.sh" method="POST">
			<textarea class="form-control" name="config">
EOM3

cat "$CONF_FILE"

cat << EOM4
</textarea><br>

			<input class="btn btn-primary" type="submit" value="Save">
			<input class="btn btn-primary" type="reset" value="Reset">
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
EOM4
