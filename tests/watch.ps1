# samples the primary screen every 120 ms without touching any window: the mean colour of a 16x9 downscale and the colour at the centre
param([string]$OutDir, [int]$Count = 40)
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
Add-Type -Namespace W -Name U -MemberDefinition '[DllImport("user32.dll")] public static extern bool SetProcessDPIAware();'
[W.U]::SetProcessDPIAware() | Out-Null
New-Item -ItemType Directory -Force $OutDir | Out-Null
$b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
"screen $($b.Width)x$($b.Height)"
for ($i = 0; $i -lt $Count; $i++) {
  $bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
  $g = [System.Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($b.Location, [System.Drawing.Point]::Empty, $b.Size); $g.Dispose()
  $small = New-Object System.Drawing.Bitmap 16, 9; $g2 = [System.Drawing.Graphics]::FromImage($small); $g2.DrawImage($bmp, 0, 0, 16, 9); $g2.Dispose()
  $r = 0; $gg = 0; $bb = 0; for ($x = 0; $x -lt 16; $x++) { for ($y = 0; $y -lt 9; $y++) { $c = $small.GetPixel($x, $y); $r += $c.R; $gg += $c.G; $bb += $c.B } }
  $c0 = $small.GetPixel(8, 4)
  "{0,2} t={1} mean=({2},{3},{4}) centre=({5},{6},{7})" -f $i, (Get-Date).ToString('HH:mm:ss.fff'), [int]($r/144), [int]($gg/144), [int]($bb/144), $c0.R, $c0.G, $c0.B
  if ($i % 8 -eq 0) { $bmp.Save("$OutDir\shot_$i.png", [System.Drawing.Imaging.ImageFormat]::Png) }
  $bmp.Dispose(); $small.Dispose()
  Start-Sleep -Milliseconds 100
}
