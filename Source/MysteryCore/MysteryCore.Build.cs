using UnrealBuildTool;

/**
 * The case generator, ported from the web game: plain C++ with no Unreal types (only the
 * module boilerplate in MysteryCoreModule.cpp touches the engine), so it also builds on its
 * own for the parity test in Tests/MysteryCoreTests.
 */
public class MysteryCore : ModuleRules
{
	public MysteryCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		bUseUnity = false;
		// The standard library containers it uses can throw; keep their semantics.
		bEnableExceptions = true;

		PrivateDependencyModuleNames.Add("Core");
	}
}
