$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$ethIp = Get-NetIPAddress -AddressFamily IPv4 -IPAddress '192.168.137.1'
$ethAdapter = Get-NetAdapter -InterfaceIndex $ethIp.InterfaceIndex
$wlanRoute = Get-NetRoute -AddressFamily IPv4 -DestinationPrefix '0.0.0.0/0' | Sort-Object RouteMetric | Select-Object -First 1
$wlanDns = Get-DnsClientServerAddress -InterfaceAlias $wlanRoute.InterfaceAlias -AddressFamily IPv4
$ics = Get-Service SharedAccess
Write-Output "ethernet_adapter=$($ethAdapter.InterfaceDescription) status=$($ethAdapter.Status) ip=$($ethIp.IPAddress)/$($ethIp.PrefixLength)"
Write-Output "default_route_interface=$($wlanRoute.InterfaceAlias) next_hop=$($wlanRoute.NextHop)"
Write-Output "wlan_dns=$($wlanDns.ServerAddresses -join ',')"
Write-Output "ics_status=$($ics.Status) start_type=$($ics.StartType)"
$ping = New-Object System.Net.NetworkInformation.Ping
foreach ($target in @('192.168.137.48','223.5.5.5')) {
  foreach ($n in 1..2) {
    $reply = $ping.Send($target, 1500)
    Write-Output "ping target=$target seq=$n status=$($reply.Status) rtt_ms=$($reply.RoundtripTime)"
  }
}
foreach ($server in @($null,'192.168.137.1')) {
  $label = if ($server) { $server } else { 'system_default' }
  $result = Resolve-DnsName 'token-plan-cn.xiaomimimo.com' -Server $server -QuickTimeout
  $ips = @($result | Where-Object { $_.Type -eq 'A' } | ForEach-Object { $_.IPAddress }) -join ','
  Write-Output "dns server=$label cname=$($result[0].Name) A=$ips"
}
