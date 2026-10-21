param($sess, $from, $seed0, $startcp)
# The Mario Combat level's areas $from..5 for session $sess (tas_backtrack.py mc_runner), from checkpoint $startcp
# (the start of area $from); each later area from the end of the one before.
$py='tools\Python3.14.8\python.exe'; $seed=[int]$seed0
foreach ($n in $from..5) {
  $cp = if ($n -eq $from) { $startcp } else { "a$n" }
  if ($n -ne $from) { & $py -I tools\scripts\tas.py checkpoint $sess $cp | Out-Null }
  # (Bowser's area: SMW Mario's stomps are not boss punches, the runner's progress: a long stall limit)
  $stall = if ($n -eq 5) { '30000' } else { '1500' }
  $p = Start-Process -PassThru -Wait -FilePath $py -ArgumentList '-I','tools\scripts\tas_backtrack.py',$sess,$cp,'mc_runner','--seed',"$seed",'--stall',$stall,'--patience','40','--mcl' -RedirectStandardOutput "work\tas\${sess}_a$n.txt" -RedirectStandardError "work\tas\${sess}_a${n}_err.txt" -WindowStyle Hidden
  $seed++
  "area $n exit $($p.ExitCode): " + (Get-Content "work\tas\${sess}_a$n.txt" -Tail 1)
  if ($p.ExitCode -ne 0) { break }
}
