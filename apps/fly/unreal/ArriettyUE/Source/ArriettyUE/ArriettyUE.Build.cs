using UnrealBuildTool;
using System.IO;
public class ArriettyUE : ModuleRules
{
    public ArriettyUE(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory,"../../../../../../shared/unreal")));
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay", "XRBase", "Sockets", "Networking", "Json", "UMG", "Slate", "SlateCore", "ProceduralMeshComponent", "CesiumRuntime" });
    }
}
