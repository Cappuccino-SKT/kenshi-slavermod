param([string]$exe, [string]$filter)
$asm = [Reflection.Assembly]::ReflectionOnlyLoadFrom($exe)
[AppDomain]::CurrentDomain.add_ReflectionOnlyAssemblyResolve({ param($s,$e) try { [Reflection.Assembly]::ReflectionOnlyLoad($e.Name) } catch { $null } })
$one=@{}; $two=@{}
foreach ($f in [Reflection.Emit.OpCodes].GetFields()) { $op=$f.GetValue($null); $v=[uint16]$op.Value; if ($op.Size -eq 1) { $one[[int]($v -band 0xff)]=$op } else { $two[[int]($v -band 0xff)]=$op } }
try { $types = $asm.GetTypes() } catch [Reflection.ReflectionTypeLoadException] { $types = $_.Exception.Types | ? { $_ } }
foreach ($t in $types) {
  if ($filter -and $t.FullName -notmatch $filter) { continue }
  $flags=[Reflection.BindingFlags]'Public,NonPublic,Instance,Static,DeclaredOnly'
  foreach ($m in @($t.GetMethods($flags)) + @($t.GetConstructors($flags))) {
    try { $body=$m.GetMethodBody() } catch { continue }
    if (-not $body) { continue }
    $il=$body.GetILAsByteArray(); $i=0; $out=@()
    while ($i -lt $il.Length) {
      $b=$il[$i]; $i++
      if ($b -eq 0xFE) { $op=$two[[int]$il[$i]]; $i++ } else { $op=$one[[int]$b] }
      if (-not $op) { break }
      $ot=$op.OperandType.ToString()
      $tok=0; if ($i+4 -le $il.Length) { $tok=[BitConverter]::ToInt32($il,$i) }
      switch ($ot) {
        'InlineString' { try { $out += 'ldstr "' + $m.Module.ResolveString($tok) + '"' } catch {} ; $i+=4 }
        'InlineMethod' { try { $mm=$m.Module.ResolveMethod($tok); $out += $op.Name + ' ' + $mm.DeclaringType.Name + '::' + $mm.Name } catch { $out += $op.Name + ' ?' } ; $i+=4 }
        'InlineField' { try { $ff=$m.Module.ResolveField($tok); $out += $op.Name + ' ' + $ff.Name } catch {} ; $i+=4 }
        'InlineNone' {}
        'ShortInlineBrTarget' { $i+=1 } 'ShortInlineI' { $i+=1 } 'ShortInlineVar' { $i+=1 }
        'InlineVar' { $i+=2 }
        'InlineI8' { $i+=8 } 'InlineR' { $i+=8 }
        'InlineSwitch' { $n=[BitConverter]::ToInt32($il,$i); $i+=4+4*$n }
        default { $i+=4 }
      }
    }
    "### " + $t.FullName + "::" + $m.Name
    $out
  }
}
