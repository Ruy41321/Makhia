// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class MakhiaEditorTarget : TargetRules
{
    public MakhiaEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		
		// 1. Usa la V7 per UE 5.8
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;

		// 2. IMPORTANTE: Rimuovi o commenta la riga "Unique"
		// BuildEnvironment = TargetBuildEnvironment.Unique; 

		// 3. Forza il superamento del controllo sugli identificatori non definiti
		// Questo permette di usare l'ambiente condiviso senza l'errore precedente
		bOverrideBuildEnvironment = true;

		ExtraModuleNames.AddRange( new string[] { "Makhia" } );

		// Steam online backend and Discord rich presence: both are Win64 only (Linux runs the Win64 build through Proton).
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			ExtraModuleNames.Add("MakhiaSteam");
			ExtraModuleNames.Add("MakhiaDiscord");
		}
	}
}
