#!/bin/sh

echo "Content-Type: text/html"
echo ""

HACK_INFO="/home/app/.hackinfo"
url="http://freegeoip.net/csv"

get_info()
{
	key=$1
	grep $1 $HACK_INFO | cut -d "=" -f2
}
		
VAR_CAMERA=`get_info "CAMERA"`

get_internet_info=$(wget -O- -q $url)
wget_ret_internet_info=$?

get_proxychains_info=$(proxychains4 wget -O- -q $url)
wget_ret_proxychains_info=$?

oIFS=$IFS
IFS=","

set -- $get_internet_info
VAR_NET_PUBLICIP="$1"
VAR_NET_COUNTRYCODE="$2"
VAR_NET_COUNTRYNAME="$3"
VAR_NET_REGIONCODE="$4"
VAR_NET_REGIONNAME="$5"
VAR_NET_CITY="$6"
VAR_NET_ZIPCODE="$7"
VAR_NET_TIMEZONE="$8"
VAR_NET_LATITUDE="$9"
VAR_NET_LONGITUDE="$10"
VAR_NET_METROCODE="$11"

set -- $get_proxychains_info
VAR_PROXY_PUBLICIP="$1"
VAR_PROXY_COUNTRYCODE="$2"
VAR_PROXY_COUNTRYNAME="$3"
VAR_PROXY_REGIONCODE="$4"
VAR_PROXY_REGIONNAME="$5"
VAR_PROXY_CITY="$6"
VAR_PROXY_ZIPCODE="$7"
VAR_PROXY_TIMEZONE="$8"
VAR_PROXY_LATITUDE="$9"
VAR_PROXY_LONGITUDE="$10"
VAR_PROXY_METROCODE="$11"

IFS=$oIFS

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
						<li class ="active"><a href="/cgi-bin/read_proxy_config.sh">ProxyChains-ng</a></li>
						<li><a href="/cgi-bin/read_config.sh">System Config</a></li>
						<li><a href="/cgi-bin/about.sh">About</a></li>
						<li><a href="/cgi-bin/reboot.sh">Reboot</a></li>
					</ul>
				</div><!--/.nav-collapse -->
			</div><!--/.container-fluid -->
		</nav>

		<!-- Main component for a primary marketing message or call to action -->
		<div class="page-header">
			<h1>ProxyChains-ng Test</h1>
		</div>

EOM3

if [ "$wget_ret_internet_info" != "0" ]
then
cat << EOM4
		<div class="alert alert-danger" role="alert">
			<strong>Internet Connection Failed.</strong> Ensure that you are connected to the Internet.
		</div>
EOM4
elif [ "$wget_ret_proxychains_info" != "0" ]
then
cat << EOM5
		<div class="alert alert-danger" role="alert">
			<strong>Proxy Server Connection Failed.</strong> Ensure that ProxyChains-ng has been configured.
		</div>
EOM5
fi

cat << EOM6

		<table class="table table-bordered table-striped">
			<thead>
				<tr>
					<th> </th>
					<th>Internet Connection</th>
					<th>Through ProxyChains-ng</th>
				</tr>
			</thead>
			<tbody>
				<tr>
					<td><strong>Public IP</strong></td>
EOM6

echo "					<td>$VAR_NET_PUBLICIP</td>"
echo "					<td>$VAR_PROXY_PUBLICIP</td>"

cat << EOM7
				</tr>
				<tr>
					<td><strong>Country Code</strong></td>
EOM7

echo "					<td>$VAR_NET_COUNTRYCODE</td>"
echo "					<td>$VAR_PROXY_COUNTRYCODE</td>"

cat << EOM8
				</tr>
				<tr>
					<td><strong>Country Name</strong></td>
EOM8

echo "					<td>$VAR_NET_COUNTRYNAME</td>"
echo "					<td>$VAR_PROXY_COUNTRYNAME</td>"

cat << EOM9
				</tr>
				<tr>
					<td><strong>Region Code</strong></td>
EOM9

echo "					<td>$VAR_NET_REGIONCODE</td>"
echo "					<td>$VAR_PROXY_REGIONCODE</td>"

cat << EOM10
				</tr>
				<tr>
					<td><strong>Region Name</strong></td>
EOM10

echo "					<td>$VAR_NET_REGIONNAME</td>"
echo "					<td>$VAR_PROXY_REGIONNAME</td>"

cat << EOM11
				</tr>
				<tr>
					<td><strong>City</strong></td>
EOM11

echo "					<td>$VAR_NET_CITY</td>"
echo "					<td>$VAR_PROXY_CITY</td>"

cat << EOM12
				</tr>
				<tr>
					<td><strong>ZIP Code</strong></td>
EOM12

echo "					<td>$VAR_NET_ZIPCODE</td>"
echo "					<td>$VAR_PROXY_ZIPCODE</td>"

cat << EOM13
				</tr>
				<tr>
					<td><strong>Time Zone</strong></td>
EOM13

echo "					<td>$VAR_NET_TIMEZONE</td>"
echo "					<td>$VAR_PROXY_TIMEZONE</td>"

cat << EOM14
				</tr>
				<tr>
					<td><strong>Latitude</strong></td>
EOM14

echo "					<td>$VAR_NET_LATITUDE</td>"
echo "					<td>$VAR_PROXY_LATITUDE</td>"

cat << EOM15
				</tr>
				<tr>
					<td><strong>Longitude</strong></td>
EOM15

echo "					<td>$VAR_NET_LONGITUDE</td>"
echo "					<td>$VAR_PROXY_LONGITUDE</td>"

cat << EOM16
				</tr>
				<tr>
					<td><strong>Metro Code</strong></td>
EOM16

echo "					<td>$VAR_NET_METROCODE</td>"
echo "					<td>$VAR_PROXY_METROCODE</td>"

cat << EOM17
				</tr>
			</tbody>
		</table>
		<p>Service Provided by <a href="http://freegeoip.net">freegeoip.net</a>.</p>
		<a class="btn btn-primary" href="/cgi-bin/read_proxy_config.sh" role="button">Back</a>
		<a class="btn btn-primary" href="/cgi-bin/test_proxy_config.sh" role="button">Retry</a>
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
EOM17
