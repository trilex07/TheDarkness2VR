param([Parameter(Mandatory=$true)][int]$GamePid,
 [Parameter(Mandatory=$true)][int]$ThreadId,
 [Parameter(Mandatory=$true)][string]$Output,
 [ValidateRange(1,10)][int]$Seconds=5)
$ErrorActionPreference='Stop'
# Diagnostic lab only: brief control-context snapshots, never stack walking or
# symbol loading while suspended. Every successful suspend has a finally resume.
# Samples include waits and scheduling; they are NOT sampled CPU execution time.
if(![Environment]::Is64BitProcess){throw 'Use 64-bit PowerShell for WOW64 context'}
$game=Get-Process -Id $GamePid
$start=$game.StartTime.ToUniversalTime().ToString('o')
$exe=$game.Path
if((Get-FileHash -LiteralPath $exe).Hash -ne 'D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1'){throw 'Unrecognized game EXE'}
if(!($game.Threads | Where-Object Id -eq $ThreadId)){throw 'Thread does not belong to game'}
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class D2ThreadSample {
 [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr OpenThread(uint access,bool inherit,uint tid);
 [DllImport("kernel32.dll",SetLastError=true)] static extern uint Wow64SuspendThread(IntPtr thread);
 [DllImport("kernel32.dll",SetLastError=true)] static extern uint ResumeThread(IntPtr thread);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool Wow64GetThreadContext(IntPtr thread,IntPtr context);
 [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool QueryThreadCycleTime(IntPtr thread,out ulong cycles);
 public static ulong[] Sample(uint tid) {
  IntPtr h=OpenThread(0x004a,false,tid);
  if(h==IntPtr.Zero)throw new System.ComponentModel.Win32Exception();
  IntPtr p=Marshal.AllocHGlobal(716);
  try {
   for(int i=0;i<716;i+=4)Marshal.WriteInt32(p,i,0);
   Marshal.WriteInt32(p,0,0x10001);
   uint count=Wow64SuspendThread(h);
   if(count==uint.MaxValue)throw new System.ComponentModel.Win32Exception();
   uint ip=0;bool ok=false;
   try {ok=Wow64GetThreadContext(h,p);if(ok)ip=unchecked((uint)Marshal.ReadInt32(p,184));}
   finally {if(ResumeThread(h)==uint.MaxValue)throw new System.ComponentModel.Win32Exception();}
   if(!ok)throw new System.ComponentModel.Win32Exception();
   ulong cycles;if(!QueryThreadCycleTime(h,out cycles))throw new System.ComponentModel.Win32Exception();
   return new ulong[]{ip,cycles,count};
  } finally {Marshal.FreeHGlobal(p);CloseHandle(h);}
 }
}
'@
$moduleCode='$p=Get-Process -Id '+$GamePid+'; @($p.Modules | ForEach-Object { @{name=$_.ModuleName;base=[uint64]$_.BaseAddress.ToInt64();size=$_.ModuleMemorySize} }) | ConvertTo-Json -Compress'
$modules=& "$env:WINDIR\SysWOW64\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -Command $moduleCode | ConvertFrom-Json
if($LASTEXITCODE -or !$modules){throw 'Could not read WOW64 module inventory'}
$rows=@();$clock=[Diagnostics.Stopwatch]::StartNew()
while($clock.Elapsed.TotalSeconds -lt $Seconds){
 $game.Refresh()
 if($game.HasExited -or $game.StartTime.ToUniversalTime().ToString('o') -ne $start){throw 'Game identity changed'}
 $t=[Diagnostics.Stopwatch]::GetTimestamp()
 $s=[D2ThreadSample]::Sample([uint32]$ThreadId)
 $rows+=@{ip=$s[0];cycles=$s[1];previousSuspendCount=$s[2];qpc=$t;sampleTicks=([Diagnostics.Stopwatch]::GetTimestamp()-$t)}
 Start-Sleep -Milliseconds 20
}
@{pid=$GamePid;tid=$ThreadId;start=$start;dll=(Get-FileHash -LiteralPath (Join-Path (Split-Path $exe) 'd3d9.dll')).Hash;frequency=[Diagnostics.Stopwatch]::Frequency;modules=$modules;samples=$rows;limits='Wall-clock instruction-pointer samples include blocked and descheduled states; not ETW CPU execution or causal proof. No stack data collected.'} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $Output
Write-Output "Saved $($rows.Count) context samples; each successful suspend resumed."
