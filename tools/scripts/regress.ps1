# Run the regression pilots (rounds 2-8) in parallel and print one summary line each (frames, player states, game
# modes, lowest MC life, crash). Contact sheets land in work\pilot\<name>_sheet.png: look at them.
# Usage: powershell -File tools\scripts\regress.ps1 [-Parallel 6]
param([int]$Parallel = 6)
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$names = @(
  'blockwall_yi1', 'blockwall2_yi1', 'msgblock_yh', 'toggle_yi1', 'yoshi_yi2', 'pipe_yi3', 'swim_00a', 'climb_iggy',
  'cape_yi1', 'goal_yi1', 'boss_iggy', 'hpipe_1cb_mc', 'hpipe_1cb_smw', 'run_yi1', 'cape_flight_yi2', 'carry_yi1',
  'powerup_colors', 'multihit', 'strong_hit', 'replay_castle', 'cape_look', 'cheats_yi1', 'cape_flight_poses',
  'cheat_lives_death', 'save100',
  # round 5
  'triple_enemies', 'cheats_r5', 'powerup_heal', 'menu_r5', 'objects_r5', 'noteblock', 'tiles_mc',
  # round 6
  'cape_spin', 'air_kick_bricks', 'hit_chain', 'excl_blocks', 'flying_qblock', 'wiggler_punch', 'mc_koopas',
  'mc_koopas_yi2', 'mc_koopas_ledge', 'cheats_r6', 'menu_r6',
  # round 7
  'rerender_test', 'king_koopas', 'king_koopas_hits', 'king_koopas_yi2', 'king_koopas_damage', 'king_koopas_mirror',
  'mc_enemies_despawn', 'mc_enemies_yoshi', 'mc_enemies_pause_yoshi', 'cheats_r7_moves', 'cheats_r7_walljump', 'menu_r7',
  # round 8
  'cheats_r8', 'menu_r8', 'jump_distance', 'jump_swim_cam', 'door_up', 'hud_yi4', 'ghostship_draw', 'mc_wall_held_jump',
  'autoscroll_edge', 'layer2_pillar',
  # round 9
  'cheats_r9', 'menu_r9',
  # round 10
  'instant_hits', 'cheats_r10', 'menu_r10',
  # round 11
  'pound_anim', 'open_map', 'cheats_r11', 'menu_r11',
  # round 12
  'knockback_r12', 'cheats_r12', 'menu_r12',
  # round 13
  'airdash_r13', 'settings_r13', 'menu_r13',
  # round 14
  'levelselect_r14', 'menu_r14',
  # round 15
  'mchits_chuck_r15', 'mchits_mole_r15', 'stun_r15', 'ice_r15', 'jumpenemies_r15', 'dash_pipe_r15', 'bossrush_r15',
  'bossrush_death_r15',
  # round 16
  'title_r16', 'bossrush_transitions_r16', 'levelselect_subroom_r16',
  # round 17
  'door_walk_r17', 'air_attack_cling_r17',
  # round 18
  'pswitch_coins_r18', 'solid_wall_r18', 'reznor_platform_r18', 'menu_r18', 'menu_rush_r18'
)
$runner = Join-Path $PSScriptRoot 'run_pilot.ps1'
$jobs = @{}
foreach ($n in $names) {
  while (@($jobs.Values | Where-Object { -not $_.HasExited }).Count -ge $Parallel) { Start-Sleep -Milliseconds 300 }
  $log = Join-Path $root "work\pilot\regress_$n.log"
  $jobs[$n] = Start-Process powershell -ArgumentList '-NoProfile', '-File', $runner, $n -PassThru -WindowStyle Hidden `
    -RedirectStandardOutput $log -RedirectStandardError "$log.err"
}
$jobs.Values | ForEach-Object { $_.WaitForExit() }
foreach ($n in $names) {
  $summary = Get-Content (Join-Path $root "work\pilot\regress_$n.log") -ErrorAction SilentlyContinue |
    Select-String '^frames' | Select-Object -Last 1
  $shots = @(Get-ChildItem (Join-Path $root "work\pilot\$n\*.bmp") -ErrorAction SilentlyContinue).Count
  '{0,-20} shots {1,2}  {2}' -f $n, $shots, $(if ($summary) { $summary } else { '(no csv)' })
}
