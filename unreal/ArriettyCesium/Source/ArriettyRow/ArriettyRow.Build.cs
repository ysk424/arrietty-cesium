using UnrealBuildTool;
using System.IO;
public class ArriettyRow : ModuleRules {
    public ArriettyRow(ReadOnlyTargetRules Target):base(Target) {
        PCHUsage=PCHUsageMode.NoPCHs;
        bUseUnity=false;
        bEnableExceptions=true;
        PublicDependencyModuleNames.AddRange(new[]{"Core","CoreUObject","Engine","InputCore","HeadMountedDisplay","XRBase","Json","UMG","Slate","SlateCore","ProceduralMeshComponent"});
        PrivateDependencyModuleNames.AddRange(new[]{"AudioMixer","CesiumRuntime"});
        string root=Path.GetFullPath(Path.Combine(ModuleDirectory,"../../../.."));
        PublicIncludePaths.Add(Path.Combine(root,"Source"));
        string sdk=Path.Combine(root,"ThirdParty/OpenVR");
        PublicSystemIncludePaths.Add(Path.Combine(sdk,"headers"));
        PublicAdditionalLibraries.Add(Path.Combine(sdk,"lib/win64/openvr_api.lib"));
        PublicSystemLibraries.Add("windowsapp.lib");
        PublicSystemIncludePaths.Add(Path.Combine(Target.WindowsPlatform.WindowsSdkDir!,"Include",Target.WindowsPlatform.WindowsSdkVersion!,"cppwinrt"));
        PublicDelayLoadDLLs.Add("openvr_api.dll");
        RuntimeDependencies.Add("$(TargetOutputDir)/openvr_api.dll",Path.Combine(sdk,"bin/win64/openvr_api.dll"));
        RuntimeDependencies.Add("$(TargetOutputDir)/OpenVR-LICENSE.txt",Path.Combine(sdk,"LICENSE"));
    }
}
